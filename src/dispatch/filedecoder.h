/***************************************************************************
 *   QSSTV-Experimental: decode SSTV from audio files                       *
 *                                                                         *
 *   Runs recordings through the normal receive chain (sound thread ->     *
 *   RX thread -> dispatcher), one file after the other. Used by the       *
 *   "Decode SSTV From File..." menu item and RX button, by                *
 *   `qsstv --decode file.wav` (window opens, you watch it decode) and by  *
 *   `qsstv --batch files...` (headless: images are saved, results are     *
 *   printed, the exit code says what happened).                           *
 ***************************************************************************/
#ifndef FILEDECODER_H
#define FILEDECODER_H

#include <QElapsedTimer>
#include <QObject>
#include <QImage>
#include <QStringList>
#include <QTimer>
#include "sstvparam.h"

class QMediaDevices;
class QWidget;

class fileDecoder : public QObject
{
  Q_OBJECT
public:
  //! process exit codes of a batch run
  enum {EXIT_OK=0,EXIT_NOIMAGE=1,EXIT_BADFILE=2,EXIT_TIMEOUT=3};

  explicit fileDecoder(QObject *parent=nullptr);

  //! batch mode: no dialogs, images go to outDir as <input>_<n>_<MODE>.png, results are printed, finished() is emitted
  void setBatch(const QString &outDir,int timeoutSeconds);
  bool isBatch() const {return batch;}
  /*! `--listen`: headless like batch, but the input is the sound card (named `device`, only used in messages) and the
      run goes on until a stop signal (SIGINT/SIGTERM/SIGHUP) arrives or `timeoutSeconds` (0 = never) have passed.
      Pictures are named <MODE>_<UTC date>_<UTC time>.png in outDir. */
  void setListen(const QString &outDir,int timeoutSeconds,const QString &device);
  bool isListening() const {return listening;}
  void startListening();
  //! the sound device could not be reopened after it was lost; ends a --listen run
  void soundFailed(const QString &error);
  //! mainWindow::restartSound() is rebuilding the sound device: the receiver is idle on purpose
  void soundRestarting(bool on) {restarting=on;}
  //! batch: receive DRM (digital) instead of SSTV; the picture file goes to outDir as <input>_<n>_drm.<ext>
  void setDrm(bool on) {drm=on;}
  void setKgstv(bool on) {kgstv=on;}
  void kgstvImageDecoded(const QImage &image);
  void kgstvTextDecoded(const QString &sender,const QString &text);
  //! GUI: also print the drm-stats line on stdout (command line --decode --drm)
  void setVerbose(bool on) {verbose=on;}
  bool isActive() const {return running;}

  //! queue files and start decoding (or append to the queue when already decoding)
  void decodeFiles(const QStringList &files);
  //! GUI: ask for one or more WAV files and decode them
  void chooseAndDecode(QWidget *parent);

  // dispatcher hooks
  void imageDecoded(esstvMode mode);                       //!< batch: save the received picture
  void drmImageShown();                                          //!< GUI DRM: counts a file saved the normal way
  void drmImageDecoded(const QString &file,const QString &info); //!< batch DRM: keep the received file
  void reportError(const QString &title,const QString &text); //!< batch: message boxes go to stderr

signals:
  //! batch: every file has been processed (or the run was abandoned); the argument is the process exit code
  void finished(int exitCode);

private slots:
  void poll();

private:
  bool startNext();
  void finishFile();
  void printDrmStats();
  void finishAll(bool cancelled);
  void note(int code) {if(code>exitCode) exitCode=code;}
  void pollListen();
  void beginStop();
  QString outputName(const QString &tag,const QString &ext) const;
  QString sourceName() const {return listening ? device : currentFile;}

  bool batch;
  bool listening=false;
  bool stopping=false;       //!< listen: the capture has been stopped, waiting for the receiver to save its last picture
  bool timedOut=false;       //!< listen: the stop was the --timeout, not a signal
  bool restarting=false;
  QString device;
  QElapsedTimer stopTimer;
  QElapsedTimer inputCheck;   //!< listen: when the list of sound inputs was last looked at
  QMediaDevices *mediaDevices=nullptr;
  bool drm=false;
  bool kgstv=false;
  int messagesInFile=0,messagesTotal=0;
  bool verbose=false;
  QString outDir;
  int timeoutSeconds;      //!< 0 = duration of the file + 30 s
  bool running;
  QStringList queue;
  int index;
  QString currentFile;
  double currentSeconds;
  int imagesInFile;
  int filesDone;
  int imagesTotal;
  int exitCode;
  QTimer timer;
  QElapsedTimer fileTimer;
};

extern fileDecoder *fileDecoderPtr;

#endif // FILEDECODER_H
