#ifndef APPIMAGEUPDATER_H
#define APPIMAGEUPDATER_H

#include <QObject>
#include <QProcess>
#include <QString>

/*!
  Runs appimageupdatetool for the AppImage that is running and reports how far it is, so the window can show it:
  the tool downloads only the blocks that changed (zsync), prints "NN.NN% done (X of Y MiB)..." while it does, writes the new
  AppImage next to the old one and says "Update successful. New file created: <path>". The old file is removed afterwards (-r),
  or the folder fills with old versions; the running program does not mind, its file stays open until it ends.
*/
class appImageUpdater : public QObject
{
  Q_OBJECT
public:
  explicit appImageUpdater(QObject *parent=nullptr);
  ~appImageUpdater() override;
  //! starts the tool; false when it cannot be started (finished() is then not emitted)
  bool start(const QString &tool,const QString &appImage);
  void cancel();               //!< stops the download; finished(false, ...) follows
  bool running() const {return process.state()!=QProcess::NotRunning;}

  //! one line of the tool's output: "45.3% done (46.8 of 103.5 MiB)..." (false for any other line)
  static bool parseProgress(const QString &line,double &percent,double &doneMiB,double &totalMiB);
  //! the path after "New file created:" in the tool's output, or empty
  static QString parseNewFile(const QString &output);

signals:
  void progress(double percent,double doneMiB,double totalMiB);
  void finished(bool ok,const QString &newAppImage,const QString &error);

private slots:
  void readOutput();
  void processFinished(int exitCode,QProcess::ExitStatus status);
  void processError(QProcess::ProcessError e);

private:
  QProcess process;
  QByteArray pending;          // output that has not ended in a line break yet
  QString output;              // the whole output, for the result
  QString appImage;
  bool cancelled=false;
  bool done=false;
  void emitFinished(bool ok,const QString &newPath,const QString &error);
};

#endif
