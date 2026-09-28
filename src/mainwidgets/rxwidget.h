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
  //! select the RX mode by short name ("PD120", "JB60" ...) or "auto"; false if there is no such mode
  bool setRxModeByName(const QString &name);
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
private slots:
  void slotStart();
  void slotStop();
  void slotResync();
  void slotDecodeFile();
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
  // Repopulates sstvModeComboBox ("Auto" always first) with every mode,
  // rebuilds rxModeList in step, and re-selects whichever mode
  // sstvModeIndexRx previously pointed to if it's still present (else
  // falls back to "Auto").
  void rebuildModeComboBox();
  ftpFunctions ff;
  bool doRemove;
  // Index -> esstvMode map for sstvModeComboBox; index 0 is always the
  // "Auto" sentinel (NOTVALID). See rebuildModeComboBox()'s comment for
  // why a direct index<->enum conversion isn't used.
  QVector<esstvMode> rxModeList;
};
#endif // RXWIDGET_H
