#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

class configDialog;
class spectrumWidget;

namespace Ui {
  class MainWindow;
  }

class bottomNav;
class QBoxLayout;
class QScrollArea;

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
  //! switch between SSTV and DRM exactly as clicking the tab does (headless decode / encode)
  void switchMode(int rxtxMode) {slotModeChange(rxtxMode);}
  //! true once the quit is confirmed: settings are written for the last time and nothing may open a message box
  bool isQuitting() const {return quitting;}
  void askMicrophone();   //!< Android: the system asks for the microphone (once); the receiver starts when it is given
  void soundSettingsChanged() {restartSound(false);}   //!< the Qt Quick settings pages changed the device or the microphone setting
  //! --encode (set before init()): the SSTV / DRM tab it switches to is not remembered, the next start (or the next --encode)
  //! must not begin in it, and no first-run question box is armed
  void setHeadlessEncode(bool on) {headlessEncode=on;}
  spectrumWidget *spectrumFramePtr;
  //! opens the built-in Users Guide, at `page` (e.g. "pages/template-editor.html") when it exists
  void openUserGuide(const QString &page);
  //! shows the Transmit page (a picture was sent to it), if it can be used just now
  void showTransmitPage();

private slots:
  void slotConfigure();
  void slotSaveWaterfallImage();
  void slotDecodeFromFile();
  void slotExit();
  void slotResetLog();
  void slotLogSettings();
  void slotAboutQt();
  void slotDesktopIntegration();
  void slotCheckForUpdates();
  void slotCheckForUpdatesWeb();
  void slotAutoUpdateToggled(bool on);
  void offerUpdateCheck();
  void applySafeArea();     //!< touch shell: margins for the system bars Android draws over the window
  void arrangeRail(bool on);   //!< touch shell, a phone on its side: navigation at the left, spectrum and settings beside the picture
  void arrangePages();      //!< touch shell: let each page arrange itself for the width of the window and size the tabs to fill it
  void setupTouchShell();   //!< the shell for a touch screen (uimode.h): tabs at the bottom, the menu bar's items on a More page
  void offerDesktopIntegration();
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
#endif
  void slotTxTestPattern();

private:
  Ui::MainWindow *ui;
  bool quitting=false; // set once quit is confirmed; QApplication::quit() re-sends closeEvent
  bool headlessEncode=false; // --encode run: the mode switch is not written to the settings
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
  bool recovering=false;      // guards recoverSound() against re-entry (issue #73)
  bool rxWantedActive=false;  // RX was running when the sound device was last torn down
#ifdef Q_OS_ANDROID
  bool micAsked=false;               // the system has been asked for the microphone in this run
  bool micDenied=false;              // the user refused the microphone: start the receiver when it is given later, in the settings
  QString inputProblemShown;         // the microphone problem now in the status bar
#endif
#ifdef Q_OS_WIN
  // Windows invalidates/suspends audio streams across system sleep with no
  // guarantee the Qt Multimedia backend surfaces that as a stream error (unlike
  // this app's other Linux/macOS targets) -- WM_POWERBROADCAST is the OS's own,
  // authoritative "resumed" notification, so it doesn't depend on what the audio
  // backend does or doesn't report.
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
  void cleanUpCache(QString dirPath);
  QPushButton *wfTextPushButton;
  QPushButton *fixPushButton;
  QPushButton *idPushButton;
  QPushButton *cwPushButton;
  QComboBox *freqComboBox;
  QLabel pttText;
  QLabel *pttIcon;
  QLabel *freqDisplay;
  QLabel *beaconCountdownLabel;
  bool beaconCountdownError=false;
  void updateBeaconCountdownVisibility();
  void timerEvent(QTimerEvent *);
  void resizeEvent(QResizeEvent *e) override;
  void showEvent(QShowEvent *e) override;
  bool eventFilter(QObject *o,QEvent *e) override;
  void goBack();   //!< touch shell: what the Back key does
  bool safeAreaConnected=false;
  QBoxLayout *shellLayout=nullptr;        // touch shell: the pages and the navigation
  bottomNav *nav=nullptr;
  QScrollArea *rxScroll=nullptr;          // the Receive page
  QWidget *rxSide=nullptr;                // beside it on a phone on its side: spectrum above the settings
  QScrollArea *rxSideScroll=nullptr;
  bool rail=false;
  QStringList modModeList;
  QStringList modPassBandList;
};

#endif // MAINWINDOW_H
