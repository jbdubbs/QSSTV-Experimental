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
class QAction;
class QMenu;
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
  void prepareTxComplete(bool ok);
  void writeSettings();
  void readSettings();
  //! Configuration > GPS may have another position format now: the Lat and Long boxes are converted to it (in both directions)
  void positionFormatChanged();
  void updateGpsButton();   //!< colors the GPS button (green: read before each transmission, red: by hand); not colored while the GPS is off
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
  void slotBeaconLive(const QImage &image);
  void slotBeaconFailed(const QString &reason);
  void slotBeaconRecovered();
  void slotGetGps();
  void slotGpsToggle();
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
    void slotSaveSettings();

signals:
  void modeSwitch(int);
  void txStopped();
  void progressChanged(int percent); //!< TX progress 0..100 (calibration dialog)
  void calibrationTxFinished();
  void beaconCountdownChanged(const QString &text,bool error); //!< status bar text for the running webcam beacon (countdown, or the reason it is not transmitting); empty: not running

private:
  void convertPosition(int format);
  //! writes the Transmit tab choices as they are (writeSettings() first reads the boxes and re-applies the template)
  void saveSettings();
  //! a choice on the tab changed: it is written a moment later, so it survives a shutdown, kill or crash as well as a quit
  void scheduleSettingsSave();
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
  //! the live camera picture may replace the Transmit picture now (no picture is on its way to the air or being waited for)
  bool liveAllowed() const;
  //! a transmission is over (or never started): the camera picture is shown live again
  void resumeLive();
  //! the GPS is set up and the GPS button is green: every transmission (Start and the beacon) reads a fresh position first
  bool gpsAutoReads() const;
  //! the picture waited for the position (Start button): sends it now
  void startAfterPosition();
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
  QTimer beaconTimer;           // single shot: next picture (first one, interval, or 30 s retry)
  QTimer beaconTick;            // 1 s, drives the countdown
  int beaconMinutes=0;
  QString beaconError;          // why the last beacon attempt failed; shown instead of the countdown until an attempt succeeds
  QString beaconCameraProblem;  // the camera is lost and being reopened; shown in place of the countdown, the schedule goes on
  bool beaconRunning=false;
  bool beaconHold=false;        // the camera is on and the first picture waits for the 10 s to pass
  beaconCapture beaconCap;      // on for as long as the beacon runs: the live picture on this tab, and the newest frame for each beacon picture
  bool txLocked=false;          // prepareTx() locked the controls: a picture is being prepared or sent, so it must not change (the TX thread reads it)
  bool livePaused=false;        // the user loaded a picture of their own: no live picture until the transmission is over
  qint64 liveKey=0;             // cacheKey() of the TX picture the live picture last put there; another one means the user loaded a picture
  bool beaconPending=false;     // a beacon picture is on its way through prepareTx()/startTxImage()
  bool beaconWaitPosition=false;     // the beacon picture is ready, but the position (the GPS, then the place) is still coming: slotGpsFix() or slotPlaceReady() sends it
  bool startWaitPosition=false;      // the Start button was pressed, the position is still coming: slotGpsFix() or slotPlaceReady() starts the transmission
  bool gpsAutoRead=false;       // the GPS button is green: the GPS is read before each transmission (saved as TX/gpsAutoRead)
  QFont gpsButtonFont;          // the font of the GPS button before it got a style sheet, kept in every sheet
  QString gpsBaseSheet;         // the sheet styleToolButtons() gave the GPS button (empty except on Windows): the sheets of its colors come after it
  QString snapshotBaseSheet;    // the same for the camera / beacon button
  QMenu *gpsMenu=nullptr;               // right-click menu of the GPS button ...
  QAction *gpsReadAction=nullptr;       // ... read the position now
  QAction *gpsAutoAction=nullptr;       // ... and the switch between green and red
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
  float fileSize;

  // Step 17: index -> esstvMode map for the (possibly filtered)
  // sstvModeComboBox, rebuilt by rebuildModeComboBox() -- see that
  // function's comment for why a direct index-to-enum cast no longer
  // works once the list can be filtered.
  QVector<esstvMode> txModeList;
  int gridLayout=0;
  int framing=0;                 // Stretch, Crop, Fit or Free: the default for every picture and grid cell (saved as TX/framing)
  int drmProfileIndex=0;         // the DRM profile that was selected last (saved as TX/drmProfile)
  bool drmParamsSaved=false;     // the settings file has DRM options: they are not replaced by profile 0
  QTimer settingsSaveTimer;      // single shot: writes the settings a moment after the last change
  bool settingsReady=false;      // init() is done: control changes before that are the program setting up, not the user

};

#endif // TXWIDGET_H















