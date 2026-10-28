#ifndef DISPATCHER_H
#define DISPATCHER_H
#include "dispatchevents.h"
#include "appglobal.h"
#include <QByteArray>
#include "textdisplay.h"
#include "txfunctions.h"
#include "ftpfunctions.h"
#include <QPointer>
class TemplateEditor;
class imageViewer;


#include <QProgressDialog>


/**
@author Johan Maes
*/
class dispatcher : public QObject
{
  Q_OBJECT

public:

  dispatcher();
  ~dispatcher();
  void init();
  void idleAll();
  void startRX();
  bool startTX(txFunctions::etxState state);
  void readSettings();
  void writeSettings();
  void customEvent( QEvent * e );
  void startDRMFIXTx(QByteArray ba);
  void startDRMTxBinary();
  //  void startDRMHybridTx(QString fn);
  //  void startDRMHybridText(QString txt);
  //  void sendSweepTone(double duration,double lowerFreq,double upperFreq);
  void saveImage(QString fileName, QString infotext);
  void uploadToRXServer(QString remoteDir, QString fn);
  void logSSTV(QString call, bool fromFSKID);
  void showOffLine();
  /** opens the template editor (or raises the open one) and loads fileName into it; an empty name starts a new template */
  void openTemplateEditor(const QString &fileName);
  /** closes the template editor if it is open, asking about unsaved changes; false if the user cancelled (the program should not quit then) */
  bool closeTemplateEditor();
  /** calibration: while set, a received SSTV picture is announced with sstvImageReceived() and not saved */
  void setCalibrationRx(bool on) {calibrationRx=on;}
//  eftpResult notifyRXDone;
//  eftpResult hybridTxDone;
//  eftpResult hybridRxDone;
//  eftpResult notifyTxDone;


signals:
  void sstvLineReceived();            //!< a line was drawn in the RX picture; only while setCalibrationRx(true)
  void sstvImageReceived(int mode);   //!< a picture was completed (NOTVALID if too few lines); only while setCalibrationRx(true)

private slots:
  void slotTemplateSaved();
//  void slotRXNotification(QString info);
  void slotRenameListing(bool err);

private:
  bool radioTransition=false;
  void saveRxSSTVImage(esstvMode mode);
  bool inList(QList <QUrlInfo> lst,QString fn);
  void timerEvent(QTimerEvent *event);
  bool calibrationRx=false;
  QPointer<TemplateEditor> templateEditor;   // clears itself when the window is closed
  int txTimeCounter;
  int prTimerIndex;
  textDisplay *infoTextPtr;
  QMessageBox *mbox;
  QProgressDialog *progressFTP;
  QString lastFileName;
  QString lastCallsign;
  QDateTime saveTimeStamp;
  ftpFunctions ff;
  QString uploadSourceFile;
};
#endif

