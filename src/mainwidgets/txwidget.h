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

enum etxMode {TXUPLOAD,TXBINARY,TXNORMAL};

#define NOTIFYCHECKINTERVAL 15*1000
#define NUMBEROFNOTIFYCHECKS 4

//#define DRMMAXSIZE 100000
//#define DRMMINSIZE 3000



class drmTransmitter;
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
  void setSettingsTab();
  txFunctions *functionsPtr() {return txFunctionsPtr;}
  imageViewer *getImageViewerPtr(){ return imageViewerPtr;}
  QString getPreviewFilename();
  void txTestPattern(etpSelect sel);
  void startNotifyCheck(QString tmask);
  //! calibration: send a generated picture in the given mode through the normal SSTV TX path; the user's own TX picture, mode and template setting are put back when it ends (calibrationTxFinished) or on abortCalibrationTx()
  bool sendCalibrationImage(esstvMode mode,const QImage &image);
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
  bool prepareText(QString txt);
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
  //  void slotReplay();
  void slotRepeaterTimer();
  void slotFileOpen();
  void slotSnapshot();
  void slotSize(int fsize);
  void slotSizeApply();
  void slotTransmissionMode(int rxtxMode);
  void slotProfileChanged(int );

  void slotModeChanged(int);
  void slotResizeChanged(int);
  void slotGridChanged(int);
  void slotEngineChanged(bool);
  void slotBinary();
  void slotHybridToggled();
  void slotNotifyTimeout();
  void slotListingDone(bool err);


private slots:
  void slotRepaterDelay();
    void slotImageChanged();

signals:
  void modeSwitch(int);
  void calibrationTxFinished();

private:
  void initView();
  void setParams();
  void sendHybrid(QString fn);
  void applyTemplate();
  void updateTxTime();
  void rebuildModeComboBox();
  void updateGridControl();
  void startTxImage();
  void restoreAfterCalibrationTx();
  void enableButtons(bool enable);

  Ui::txWidget *ui;
  bool calTxActive=false;
  QImage calSavedImage;
  bool calSavedValid=false;
  QString calSavedFile;
  esstvMode calSavedMode=NOTVALID;
  bool calSavedUseTemplate=false;
  txFunctions *txFunctionsPtr;

  QTimer *repeaterTimer;
  QTimer repeaterTxDelayTimer;
  int repeaterIndex;
  QImage origImage;
  QImage resultImage;

  imageViewer *imageViewerPtr;
  etransmissionMode currentTXMode;
  uint maxSize;
  uint compressedSize;
  bool sizeChanged;
  int drmProfileIdx;
  QString previewFilename;
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
  // Persisted checked-state of "Use MMSSTV Core engine", independent of
  // any specific mode's selectedEngine() value -- see engineCheckBox's
  // handlers for why.
  bool useMmsstvCoreEngine;
  int gridLayout=0;

};

#endif // TXWIDGET_H















