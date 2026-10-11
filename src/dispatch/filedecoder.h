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
#include <QStringList>
#include <QTimer>
#include "sstvparam.h"

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
  //! batch: receive DRM (digital) instead of SSTV; the picture file goes to outDir as <input>_<n>_drm.<ext>
  void setDrm(bool on) {drm=on;}
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

  bool batch;
  bool drm=false;
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
