#ifndef RXWIDGET_H
#define RXWIDGET_H
#include "ftpfunctions.h"
#include <QWidget>
#include <QFrame>
#include <QVector>
#include "spectrumwidget.h"
#include "sstvparam.h"
#include "ui_rxwidget.h"


class rxFunctions;
class imageViewer;
class spectrumWidget;
class vuMeter;


namespace Ui {
class rxWidget;
}

class rxWidget : public QWidget
{
  Q_OBJECT
  
public:
  explicit rxWidget(QWidget *parent = 0);
  ~rxWidget();
  void readSettings();
  void writeSettings();
  void startRX(bool st);
  rxFunctions *functionsPtr() {return rxFunctionsPtr;}
  imageViewer *getImageViewerPtr(){ return imageViewerPtr;}
  //  spectrumWidget *fftDisplayPtr() ;
  vuMeter *vMeterPtr();
  vuMeter *sMeterPtr();
  drmConstellationFrame *mscWdg() {return ui->drmMSCWidget;}
  drmConstellationFrame *facWdg() {return ui->drmFACWidget;}
  void setDRMStatusText(QString txt)
  {
    ui->drmStatusLineEdit->clear();
    ui->drmStatusLineEdit->appendPlainText(txt);
  }
  void setOnlineStatus(bool online, QString info="");
  drmStatusFrame *statusWdg() {return ui->drmStatusWidget;}
  //  int getFilterIndex();
  void init();
  void setSSTVStatusText(QString txt);
  void setSettingsTab();
  void changeTransmissionMode(int rxtxMode);
  bool rxBusy();
  // Step 17: called by dispatcher::customEvent() (GUI thread) when RX
  // auto-detected a mode mmsstv-core doesn't implement at all while "Use
  // MMSSTV Core engine" was checked -- see syncprocessor.cpp's
  // createModeBase() for where this gets posted.
  void handleEngineAutoFallback(esstvMode mode);

private slots:
  void slotStart();
  void slotStop();
  void slotResync();
  void slotGetParams();
  void slotEngineChanged(bool checked);
  void slotTransmissionMode(int rxtxMode);
  void slotNewCall(QString);
  void slotResetCall();
  void slotLogCall();
  void slotErase();
  void slotSave();
  void slotWho();
  void slotWhoResult(bool err);

signals:
  void modeSwitch(int);


private:
  Ui::rxWidget *ui;
  rxFunctions *rxFunctionsPtr;
  imageViewer *imageViewerPtr;
  void getParams();
  void setParams();
  // Step 17: repopulates sstvModeComboBox ("Auto" always first) filtered
  // by rxPreferCoreEngine(), rebuilds rxModeList in step, re-selects
  // whichever mode sstvModeIndexRx previously pointed to if it's still
  // present (else falls back to "Auto"), and syncs engineCheckBox's
  // checked state.
  void rebuildModeComboBox();
  ftpFunctions ff;
  bool doRemove;
  // Step 17: index -> esstvMode map for the (possibly filtered)
  // sstvModeComboBox; index 0 is always the "Auto" sentinel (NOTVALID).
  // See rebuildModeComboBox()'s comment for why a direct index<->enum
  // conversion no longer works.
  QVector<esstvMode> rxModeList;
};
#endif // RXWIDGET_H
