#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

class configDialog;
class spectrumWidget;
class ftpThread;

namespace Ui {
  class MainWindow;
  }

class mainWindow : public QMainWindow
{
  Q_OBJECT
  
public:
  explicit mainWindow(QWidget *parent = 0);
  ~mainWindow();
  void init();
  void startRunning(bool startCardRx=true);
  void shutdown(bool interactive);
  void setNewFont();
  void setPTT(bool p);
  void setSSTVDRMPushButton(bool inDRM);
  //! mode (TRXSSTV/TRXDRM) that currently has a TX or RX in progress, or -1 when idle
  int busyMode();
  //! grey out the tabs of the other mode while SSTV or DRM TX/RX is in progress
  void updateModeLock();
  spectrumWidget *spectrumFramePtr;

private slots:
  void slotConfigure();
  void slotSaveWaterfallImage();
  void slotDecodeFromFile();
  void slotExit();
  void slotResetLog();
  void slotLogSettings();
  void slotAboutQt();
  void slotAboutQSSTV();
  void slotFullScreen();
  void slotDocumentation();
  void slotCalibrate();
  void slotModeChange(int);
  void slotSendWFID();
  void slotSendCWID();
  void slotSendBSR();
  void slotSendWfText();
  void slotSetFrequency(int freqIndex);



#ifndef QT_NO_DEBUG
  void slotShowDataScope();
  void slotShowSyncScopeNarrow();
  void slotShowSyncScopeWide();
  void slotScopeOffset();
  void slotClearScope();
  void slotDumpSamplesPerLine();
  void slotTxTestPattern();
#endif

private:
  Ui::MainWindow *ui;
  void closeEvent ( QCloseEvent *e );
  void readSettings();
  void writeSettings();
  void restartSound(bool inStartUp);
  // Shared recovery for an audio device that died out from under RX -- Windows
  // system sleep is the motivating case (see nativeEvent() below), reached via
  // either that or soundBase::deviceLost() (connected in restartSound()). Skips
  // (logging why) while TX is active, since rebuilding the audio device mid-
  // transmission would be actively harmful; otherwise just calls restartSound(),
  // which already knows how to re-arm RX only if it was actually running.
  void recoverSound(const QString &reason);
#ifdef Q_OS_WIN
  // Windows invalidates/suspends audio streams across system sleep with no
  // guarantee the Qt Multimedia backend surfaces that as a stream error (unlike
  // this app's other Linux/macOS targets) -- WM_POWERBROADCAST is the OS's own,
  // authoritative "resumed" notification, so it doesn't depend on what the audio
  // backend does or doesn't report.
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
  void cleanUpCache(QString dirPath);
//  void setupFtp(ftpThread *&ptr, QString idName);
  QComboBox *transmissionModeComboBox;
  QPushButton *wfTextPushButton;
  QPushButton *fixPushButton;
  QPushButton *bsrPushButton;
  QPushButton *idPushButton;
  QPushButton *cwPushButton;
  QComboBox *freqComboBox;
  QLabel pttText;
  QLabel *pttIcon;
  QLabel *freqDisplay;
  void timerEvent(QTimerEvent *);
  QStringList modModeList;
  QStringList modPassBandList;
};

#endif // MAINWINDOW_H
