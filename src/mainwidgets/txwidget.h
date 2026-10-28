#ifndef TXWIDGET_H
#define TXWIDGET_H


#include "imageviewer.h"
#include "sstvparam.h"
#include "txfunctions.h"
#include "drmtransmitter.h"
#include "ui_txwidget.h"
#include "ftpfunctions.h"

#include "qglobal.h"

#include <QWidget>
#include <QVector>
#include <QTimer>
#include "beaconcapture.h"

enum etxMode {TXUPLOAD,TXBINARY,TXNORMAL};

#define NOTIFYCHECKINTERVAL 15*1000
#define NUMBEROFNOTIFYCHECKS 4

//#define DRMMAXSIZE 100000
//#define DRMMINSIZE 3000



class drmTransmitter;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;
namespace Ui {
class txWidget;
}

class txWidget : public QWidget
{
  Q_OBJECT

public:
  explicit txWidget(QWidget *parent = 0);
  ~txWidget();
  void init();
  //! only the tab of mode "busy" stays selectable (-1: all tabs)
  void setModeLock(int busy);
//  void startTX(bool st, bool check=true);
  void prepareTx();
  void prepareTxComplete(bool ok,quint64 ticket=0);
  void writeSettings();
  void readSettings();
  //! Configuration > GPS may have another position format now: the Lat and Long boxes are converted to it (in both directions)
  void positionFormatChanged();
  //  void repeat(QImage *im,esstvMode sm);
  void sendRepeaterImage(esstvMode rxMode=NOTVALID);
  void setImage(QImage *ima);
  void setImage(QString fn);
  void setImageToSegment(QString fn,int seg);   // seg -1: first empty grid segment (else 0)
  int txGridSegments() const;
  void setImages(const QStringList &files);   // multi-select from the gallery: fills the TX grid segments
  void setProgress(uint prg);
  void setupTemplatesComboBox();
  void setPreviewWidget(QString fn);
  void refreshReplay();
  void setSettingsTab();
  txFunctions *functionsPtr() {return txFunctionsPtr;}
  imageViewer *getImageViewerPtr(){ return imageViewerPtr;}
  QString getPreviewFilename();
  void txTestPattern(etpSelect sel);
  void startNotifyCheck(QString tmask);
  //! calibration: send a generated picture in the given mode through the normal SSTV TX path; the user's own TX picture, mode and template setting are put back when it ends (calibrationTxFinished) or on abortCalibrationTx()
  bool sendCalibrationImage(esstvMode mode,const QImage &image);
  //! headless --encode --drm: jp2Bytes (0 = the size slider setting) is the size the picture is compressed to; send the picture as a DRM transmission with these parameters (combo-box indices); ends like a calibration transmit (calibrationTxFinished)
  bool sendDrmTestImage(const drmTxParams &params,const QImage &image,uint jp2Bytes=0,int encoding=-1);   // encoding: -1 keeps the setting, else imageViewer::drmEncodingType
  void abortCalibrationTx();
  bool calibrationTxActive() const {return calTxActive;}

  void setDRMNotifyText(QString txt)
  {
    ui->txNotificationList->setPlainText(txt);
  }
  void appendDRMNotifyText(QString txt)
  {
    ui->txNotificationList->appendPlainText(txt);
  }

  //  bool prepareHybrid(QString fn);
  void copyProfile(drmTxParams d);
  void setProfileNames();
  void reloadProfiles();
  void changeTransmissionMode(int rxtxMode);


  //  void test();
  //  void sendFIX();
  void sendBSR();
  void sendWfText();
  void sendWFID();
  void sendCWID();

public slots:
  void slotGetTXParams();
  void slotGetParams();
  void slotStart();
  void slotUpload();
  void slotStop();
  //  void slotDisplayStatusMessage(QString);
  void slotGenerateSignal();
  void slotSweepSignal();
  void slotGenerateRepeaterTone();
  void slotEdit();
  void slotEditTemplate();
  void slotNewTemplate();
  //  void slotReplay();
  void slotRepeaterTimer();
  void slotFileOpen();
  void slotSnapshot();
  void slotBeaconTick();
  void slotBeaconTimer();
  void slotBeaconFrame(const QImage &image);
  void slotBeaconFailed(const QString &reason);
  void slotGetGps();
  void slotGpsFix(bool ok,double lat,double lon,const QString &reason);
  void slotPlaceReady(bool ok,const QString &place,const QString &reason);
  void slotPositionEdited();
  void slotSize(int fsize);
  void slotSizeApply();
  void slotSizeReleased();
  void slotCompressionReady();
  void slotTransmissionMode(int rxtxMode);
  void slotProfileChanged(int );

  void slotModeChanged(int);
  void slotResizeChanged(int);
  void slotGridChanged(int);
  void slotBinary();
  void slotHybridToggled();
  void slotNotifyTimeout();
  void slotListingDone(bool err);


private slots:
  void slotRepaterDelay();
    void slotImageChanged();
    void slotEncodingChanged(int enc);

signals:
  void modeSwitch(int);
  void txStopped();
  void progressChanged(int percent); //!< TX progress 0..100 (calibration dialog)
  void calibrationTxFinished();
  void beaconCountdownChanged(const QString &text,bool error); //!< status bar text for the running webcam beacon (countdown, or the reason it is not transmitting); empty: not running

private:
  void convertPosition(int format);
  void initView();
  void setParams();
  void applyTemplate();
  void updateTxTime();
  void rebuildModeComboBox();
  void updateGridControl();
  void startTxImage();
  void restoreAfterCalibrationTx();
  void enableButtons(bool enable);
  //! a scheduled beacon could not transmit: report it, and retry in 30 s
  void beaconSkipped(const QString &reason);
  void beaconRestartInterval();
  void beaconTransmitLoaded();
  bool beaconUsesGps() const;
  //! asks the GPS for a position; the result comes back in slotGpsFix(). false: the GPS is busy with another request
  bool requestGps(bool manual);
  //! asks for the city and state of the position in the Lat and Long boxes, when that lookup is on; the result comes back in slotPlaceReady()
  bool requestPlace(bool manual);
  void startBeacon(int minutes);
  void stopBeacon();
  void updateBeaconUi();

  Ui::txWidget *ui;
  bool calTxActive=false;
  bool replayStale=false;   // a new RX picture arrived while the transmitter was busy: the replay slot is redrawn when it is done
  QImage calSavedImage;
  bool calSavedValid=false;
  QString calSavedFile;
  esstvMode calSavedMode=NOTVALID;
  bool calSavedUseTemplate=false;
  txFunctions *txFunctionsPtr;

  QTimer *repeaterTimer;
  QTimer beaconTimer;           // single shot: next capture (first one, interval, or 30 s retry)
  QTimer beaconTick;            // 1 s, drives the countdown
  int beaconMinutes=0;
  QString beaconError;          // why the last beacon attempt failed; shown instead of the countdown until an attempt succeeds
  bool beaconRunning=false;
  bool beaconHold=false;        // a picture is loaded and waiting for the 10 s to pass
  beaconCapture beaconCap;
  bool beaconPending=false;     // a beacon picture is on its way through prepareTx()/startTxImage()
  bool beaconWaitPosition=false;     // the beacon picture is ready, but the position (the GPS, then the place) is still coming: slotGpsFix() or slotPlaceReady() sends it
  bool gpsAwaiting=false;       // this widget has a GPS request in flight (the Test button of Configuration > GPS does not count)
  bool gpsManual=false;         // ... and the Get GPS button made it, so a failure is shown in a message box
  bool placeAwaiting=false;     // a place lookup of this widget is in flight (the Test button of Configuration > GPS does not count)
  bool placeManual=false;       // ... and the Get GPS button started it
  QString placeKey;             // the Lat and Long text that the last lookup was for: a box that is left unchanged asks nothing
  int positionFormat=0;         // the format (coordinateFormat) that the Lat and Long boxes were last written in
  QTimer repeaterTxDelayTimer;
  int repeaterIndex;
  QImage resultImage;

  imageViewer *imageViewerPtr;
  uint maxSize;
  uint compressedSize;
  bool sizeChanged;
  QTimer sizeApplyTimer;
  etxMode doTx;
  ftpFunctions ff;
  QTimer notifyTimer;
  int numberOfNotifyChecks;
  QString mask;
  esstvMode txMode;
  bool notifyBusy;
  bool repeaterIdleImage;
  QComboBox *kgstvModulation;
  QSpinBox *kgstvQuality;
  QCheckBox *kgstvFec;
  QPlainTextEdit *kgstvText;
  QPushButton *kgstvSendText,*kgstvRequest,*kgstvRepair;
  quint64 kgstvTicket=0;
  void kgstvAction(int action);
  float fileSize;

  // Step 17: index -> esstvMode map for the (possibly filtered)
  // sstvModeComboBox, rebuilt by rebuildModeComboBox() -- see that
  // function's comment for why a direct index-to-enum cast no longer
  // works once the list can be filtered.
  QVector<esstvMode> txModeList;
  int gridLayout=0;

};

#endif // TXWIDGET_H















