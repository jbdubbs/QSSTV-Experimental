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
class QComboBox;
class QPlainTextEdit;
class QLabel;
class QCheckBox;


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
  //! command line --min-complete: "Save if Complete (%)" for this run (0 to 100; the window's own box stops at 99)
  void setMinCompletion(int percent);
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
  void setKgstvBusy(bool busy);
  //! only the tab of mode "busy" stays selectable (-1: all tabs)
  void setModeLock(int busy);
  //! put the tab bar back on mode m without triggering a mode change
  void syncModeTab(int m);
  void setFixFileEnabled(bool enable);
  //! same as the Clear toolbar button
  void clearImage() {slotErase();}
private slots:
  void slotKgstvImage(QImage image,int received,bool complete,quint64 epoch);
  void slotKgstvText(QString sender,QString text);
  void slotKgstvStatus(QString status);
  void slotKgstvLocalText(QString text,bool sent);
  void slotStart();
  void slotStop();
  void slotResync();
  void slotSyncSettingChanged();
  void slotDecodeFile();
  void slotGetParams();
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
  void fixFileRequested();


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
  QComboBox *kgstvModulation;
  QCheckBox *kgstvAutoSave;
  QPlainTextEdit *kgstvHistory;
  QLabel *kgstvStatus;
  bool kgstvSaved=false;
  // Index -> esstvMode map for sstvModeComboBox; index 0 is always the
  // "Auto" sentinel (NOTVALID). See rebuildModeComboBox()'s comment for
  // why a direct index<->enum conversion isn't used.
  QVector<esstvMode> rxModeList;
};
#endif // RXWIDGET_H
