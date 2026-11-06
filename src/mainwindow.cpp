/**************************************************************************
*   Copyright (C) 2000-2019 by Johan Maes                                 *
*   on4qz@telenet.be                                                      *
*   https://www.qsl.net/o/on4qz                                           *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#ifdef Q_OS_WIN
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QFileDialog>
#include <QDialog>
#include <QPushButton>
#include <QTextBrowser>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFile>
#include <windows.h> // WM_POWERBROADCAST / PBT_APMRESUME* -- see nativeEvent() below
#endif
#include "applog.h"
#include "mainwindow.h"
#include "appimageintegration.h"
#include "updatecheck.h"
#include "guideserver.h"
#include <QDateTime>
#include <QMessageBox>
#include <QCheckBox>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include "appglobal.h"
#include "logging.h"
#include "dispatch/dispatcher.h"
#include "dispatch/filedecoder.h"
#include "ui_mainwindow.h"
#include "soundqtmultimedia.h"
#include "soundbackend.h"
#include "platformpermissions.h"
#include "platformservice.h"
#include "uimode.h"
#include "qmlui.h"
#include "touchpage.h"
#ifdef Q_OS_ANDROID
#include "qtlocationsource.h"
#endif
#ifdef Q_OS_ANDROID
#include "androidaudio.h"
#include <android/log.h>
#endif
#include "configdialog.h"
#include "configparams.h"
#include "rigcontrol.h"
#include "gpsreader.h"
#include "placelookup.h"
#include "waterfalltext.h"
#include "calibration.h"
#include "frequencyselectwidget.h"
#include "hexconvertor.h"
#ifndef QT_NO_DEBUG
#include "scope/scopeview.h"
#endif
#include "scope/scopeoffset.h"
#include "rxfunctions.h"
#include "logbook.h"
#include "testpatternselection.h"
#include <QFont>
#include <QCloseEvent>
#include <QMessageBox>
#include <QScreen>
#include <QTimer>
#include <QElapsedTimer>
#include <QApplication>
#include "filewatcher.h"
#include "ftpfunctions.h"


/**
 * @brief
 *
 * @param parent
 */
mainWindow::mainWindow(QWidget *parent) : QMainWindow(parent),  ui(new Ui::MainWindow)
{
  inStartup=true;
  QApplication::instance()->thread()->setObjectName("qsstv_main");
  wfTextPushButton=new QPushButton("WF Text",this);
  setObjectName("mainThread");
  freqComboBox=new QComboBox(this);
  idPushButton=new QPushButton("WF ID",this);
  cwPushButton=new QPushButton("CW ID",this);
  QFont f;

  freqDisplay = new QLabel(this);
  f.setBold(true);
  f.setPixelSize(15);
  freqDisplay->setFont(f);
  freqDisplay->setObjectName("freqDisplay");
  freqDisplay->setMinimumSize(QSize(100, 0));
  freqDisplay->setMaximumSize(QSize(100, 16777215));
  freqDisplay->setFrameShape(QFrame::Box);
  freqDisplay->setFrameShadow(QFrame::Raised);
  freqDisplay->setLineWidth(2);
  pttText.setText("   PTT");
  pttIcon=new QLabel(this);
  pttIcon->setFixedSize(16,16);
  pttIcon->setPixmap(*greenPXMPtr);
  pttIcon->setFrameShape(QFrame::Panel);
  pttIcon->setFrameShadow(QFrame::Raised);
  pttIcon->setLineWidth(2);
  rigControllerPtr=new rigControl(1); // must precede configDialog construction
  gpsReaderPtr=new gpsReader(this);   // the GPS configuration page and the Transmit tab use it
#ifdef Q_OS_ANDROID
  gpsReaderPtr->setLocationSource(new qtLocationSource(gpsReaderPtr));   // the phone's own position: no receiver on a cable, no port
#endif
  placeLookupPtr=new placeLookup(this);
  configDialogPtr=new configDialog(this);
  configDialogPtr->readSettings();

  ui->setupUi(this);
  setWindowTitle(qsstvVersion);
  setWindowIcon(QPixmap(":/icons/qsstv.png"));
  ui->maintabWidget->setCurrentIndex(0);
  ui->statusBar->addPermanentWidget(freqDisplay);
  ui->statusBar->addPermanentWidget(freqComboBox);
  ui->statusBar->addPermanentWidget(wfTextPushButton);
  ui->statusBar->addPermanentWidget(idPushButton);
  ui->statusBar->addPermanentWidget(cwPushButton);
  ui->statusBar->addPermanentWidget(&pttText);
  ui->statusBar->addPermanentWidget(pttIcon);
  statusBarPtr=statusBar(); // must be after setup UI
  // webcam beacon countdown: a normal (left) status bar widget, so Qt hides it while a status message is showing
  beaconCountdownLabel=new QLabel(this);
  beaconCountdownLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
  beaconCountdownLabel->hide();
  ui->statusBar->addWidget(beaconCountdownLabel);
  spectrumFramePtr=ui->spectrumFrame;
  if(uiMode::touch() && !uiMode::qml()) setupTouchShell();   // the Qt Quick shell (src/qml) shows its own screens


  // setting up pointers
  soundIOPtr=nullptr;
  fileWatcherPtr=nullptr;


  rxWidgetPtr=ui->rxWindow;
  txWidgetPtr=ui->txWindow;
  galleryWidgetPtr=ui->galleryWindow;
  readSettings();
  soundIOPtr=newSoundBackend();
  dispatcherPtr=new dispatcher;
  fileDecoderPtr=new fileDecoder(this);
  waterfallPtr=new waterfallText;
  xmlIntfPtr=new xmlInterface;
  logBookPtr=new logBook;

  // setup connections

  connect(ui->actionSaveWaterfallImage,SIGNAL(triggered()),this, SLOT(slotSaveWaterfallImage()));
  connect(ui->actionDecodeFromFile,SIGNAL(triggered()),this, SLOT(slotDecodeFromFile()));
  connect(ui->actionExit,SIGNAL(triggered()),this, SLOT(slotExit()));
  connect(ui->actionConfigure,SIGNAL(triggered()),this, SLOT(slotConfigure()));
  connect(ui->actionCalibrate,SIGNAL(triggered()),this, SLOT(slotCalibrate()));
  connect(ui->actionAboutQSSTV, SIGNAL(triggered()),SLOT(slotAboutQSSTV()));
  connect(ui->actionAboutQt, SIGNAL(triggered()),SLOT(slotAboutQt()));
  connect(ui->actionUsersGuide, SIGNAL(triggered()),SLOT(slotDocumentation()));
#ifdef Q_OS_ANDROID
  ui->actionUsersGuide->setShortcut(Qt::Key_F1);   // a keyboard (or the test harness) has no menu bar to click here
  ui->actionConfigure->setShortcut(Qt::Key_F2);
#endif
#ifndef Q_OS_ANDROID   // an Android app is installed and updated by the store (and Google Play does not allow its own update prompt)
  if(appImageIntegration::available())
    {
      // AppImage-only Help entries: desktop integration and updating
      ui->menuHelp->addSeparator();
      QAction *integ=ui->menuHelp->addAction(tr("Desktop integration..."));
      connect(integ,&QAction::triggered,this,&mainWindow::slotDesktopIntegration);
      QAction *upd=ui->menuHelp->addAction(tr("Check for updates..."));
      connect(upd,&QAction::triggered,this,&mainWindow::slotCheckForUpdates);
    }
  else
    {
      // other builds (Windows): look at the GitHub releases ourselves and point the user at the download
      ui->menuHelp->addSeparator();
      QAction *upd=ui->menuHelp->addAction(tr("Check for updates..."));
      connect(upd,&QAction::triggered,this,&mainWindow::slotCheckForUpdatesWeb);
      QAction *autoUpd=ui->menuHelp->addAction(tr("Check for updates at startup"));
      autoUpd->setCheckable(true);
      autoUpd->setChecked(QSettings().value("update/autoCheck",false).toBool());
      connect(autoUpd,&QAction::toggled,this,&mainWindow::slotAutoUpdateToggled);
    }
#endif
  connect(idPushButton, SIGNAL(clicked()), this, SLOT(slotSendWFID()));
  connect(cwPushButton, SIGNAL(clicked()), this, SLOT(slotSendCWID()));
  connect(rxWidgetPtr,SIGNAL(fixFileRequested()),this,SLOT(slotSendBSR()));
  connect(freqComboBox,SIGNAL(activated(int)),SLOT(slotSetFrequency(int)));
  connect(wfTextPushButton, SIGNAL(clicked()), this, SLOT(slotSendWfText()));
  connect(rxWidgetPtr,SIGNAL(modeSwitch(int)),this, SLOT(slotModeChange(int)));
  connect(txWidgetPtr,SIGNAL(modeSwitch(int)),this, SLOT(slotModeChange(int)));
  // The beacon countdown/error is a normal status bar widget. It must never share the line with a status/hover
  // message, so its visibility is decided in one place from (beacon text present) && (no status message showing).
  connect(txWidgetPtr,&txWidget::beaconCountdownChanged,this,[this](const QString &t,bool error)
          {
            if(error!=beaconCountdownError)
              {
                beaconCountdownError=error;
                beaconCountdownLabel->setStyleSheet(error ? "color: #c00000;" : "");
              }
            beaconCountdownLabel->setText(t);
            updateBeaconCountdownVisibility();
          });
  connect(statusBarPtr,&QStatusBar::messageChanged,this,[this](const QString &){updateBeaconCountdownVisibility();});

  QAction *fs = new QAction(this);
  fs->setShortcut(Qt::Key_F | Qt::CTRL);
  connect(fs, SIGNAL(triggered()), this, SLOT(slotFullScreen()));
  this->addAction(fs);

#ifdef ENABLELOGGING
  connect(ui->actionLogSettings, SIGNAL(triggered()),SLOT(slotLogSettings()));
  connect(ui->actionResetLog, SIGNAL(triggered()),SLOT(slotResetLog()));
#else
  ui->menuOptions->removeAction(ui->actionLogSettings);
  ui->menuOptions->removeAction(ui->actionResetLog);
#endif
#ifndef QT_NO_DEBUG

  connect(ui->actionShowDataScope, SIGNAL(triggered()),SLOT(slotShowDataScope()));
  connect(ui->actionShowSyncScopeNarrow, SIGNAL(triggered()),SLOT(slotShowSyncScopeNarrow()));
  connect(ui->actionShowSyncScopeWide, SIGNAL(triggered()),SLOT(slotShowSyncScopeWide()));
  connect(ui->actionScopeOffset,SIGNAL(triggered()),this, SLOT(slotScopeOffset()));
  connect(ui->actionClearScope,SIGNAL(triggered()),this, SLOT(slotClearScope()));
  connect(ui->actionDumpSamplesPerLine,SIGNAL(triggered()),this, SLOT(slotDumpSamplesPerLine()));

#else
  ui->menuOptions->removeAction(ui->actionDumpSamplesPerLine);
  ui->menuScope->removeAction(ui->actionShowDataScope);
  ui->menuScope->removeAction(ui->actionShowSyncScopeNarrow);
  ui->menuScope->removeAction(ui->actionShowSyncScopeWide);

  ui->menuScope->removeAction(ui->actionScopeOffset);
  ui->menuScope->menuAction()->setVisible(false);

#endif
  connect(ui->actionTxTestPattern,SIGNAL(triggered()),this, SLOT(slotTxTestPattern()));



}

/**
 * @brief
 *
 */
mainWindow::~mainWindow()
{
  delete ui;
}


/**
 * @brief initialize sound device and dispatcher
 *
 */
#ifdef Q_OS_ANDROID
// Android 13 and later: without the notification permission the foreground service still runs, but its notification (the only
// sign that the program is listening with the screen off) is not shown. Asked once; the system does not repeat a refusal anyway.
static void askForNotifications()
{
  QSettings qs;
  if(qs.value("android/notificationsAsked",false).toBool()) return;
  qs.setValue("android/notificationsAsked",true);
  androidRequestNotificationPermission();
}
#endif

/*!
  The shell for a touch screen. The pages (Receive, Transmit, Gallery) are the same widgets as on the desktop, they only get a new
  frame: large tabs at the bottom, a finger-scrolled page around Receive and Transmit, the spectrum at the top of the Receive page,
  and a More page with what the menu bar and the status bar's buttons hold. How the pages arrange themselves for a narrow
  window is in the pages (rxWidget::arrange(), txWidget::arrange()).
*/
void mainWindow::setupTouchShell()
{
  menuBar()->hide();
  // the status bar keeps its message line, the frequency and the PTT light; its buttons are on the More page
  for(QWidget *w:{static_cast<QWidget *>(freqComboBox),static_cast<QWidget *>(wfTextPushButton),static_cast<QWidget *>(idPushButton),
                  static_cast<QWidget *>(cwPushButton),static_cast<QWidget *>(&pttText)})
    w->hide();

  QTabWidget *tabs=ui->maintabWidget;
  // the frequency is in the system's font as well: the status bar is a strip, and a phone on its side has no height to spare
  QFont statusFont=QApplication::font();
  statusFont.setBold(true);
  freqDisplay->setFont(statusFont);
  freqDisplay->setLineWidth(1);
  freqDisplay->setMaximumWidth(120);
  ui->statusBar->setSizeGripEnabled(false);

  // the forms carry a font of their own (a small fixed-pitch one, 8 and 9 pt): the system's font is what a phone reads best
  ui->rxWindow->setFont(QApplication::font());
  ui->txWindow->setFont(QApplication::font());

  // Receive: the spectrum on top, the page under it scrolls
  ui->verticalLayout_2->removeWidget(ui->rxWindow);
  rxScroll=makeTouchScrollPage2D(ui->rxWindow,ui->rxTab);   // sideways too, should a page ever need more than the screen is wide
  ui->horizontalLayout->removeWidget(ui->spectrumFrame);
  ui->spectrumFrame->setMinimumWidth(0);
  ui->spectrumFrame->setMaximumWidth(QWIDGETSIZE_MAX);
  ui->spectrumFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
  ui->spectrumFrame->setFixedHeight(170);
  ui->verticalLayout_2->addWidget(ui->spectrumFrame);
  ui->verticalLayout_2->addWidget(rxScroll,1);

  // Transmit
  ui->verticalLayout_3->removeWidget(ui->txWindow);
  ui->verticalLayout_3->addWidget(makeTouchScrollPage2D(ui->txWindow,ui->txTab),1);

  // More: the menu bar
  morePage *more=new morePage(this);
  more->addHeading(tr("Receive"));
  more->addButton(tr("Decode a recording..."),[this]{ui->actionDecodeFromFile->trigger();});
  more->addButton(tr("Save the waterfall picture"),[this]{ui->actionSaveWaterfallImage->trigger();});
  more->addHeading(tr("Transmit"));
  more->addButton(tr("Send a test pattern"),[this]{ui->actionTxTestPattern->trigger();});
  more->addButton(tr("Send the waterfall text"),[this]{wfTextPushButton->click();});
  more->addButton(tr("Send the waterfall ID"),[this]{idPushButton->click();});
  more->addButton(tr("Send the CW ID"),[this]{cwPushButton->click();});
  more->addHeading(tr("Program"));
  more->addButton(tr("Settings"),[this]{ui->actionConfigure->trigger();});
  more->addButton(tr("User's Guide"),[this]{ui->actionUsersGuide->trigger();});
  more->addButton(tr("About"),[this]{ui->actionAboutQSSTV->trigger();});
  more->addButton(tr("Exit"),[this]{ui->actionExit->trigger();});
  tabs->addTab(more,tr("More"));

  // the navigation under the pages
  ui->horizontalLayout->removeWidget(tabs);
  QWidget *shell=new QWidget(ui->centralwidget);
  QBoxLayout *column=new QBoxLayout(QBoxLayout::TopToBottom,shell);
  column->setContentsMargins(0,0,0,0);
  column->setSpacing(0);
  nav=new bottomNav(tabs,shell);
  column->addWidget(tabs,1);
  column->addWidget(nav);
  shellLayout=column;
  ui->horizontalLayout->addWidget(shell);
  qApp->installEventFilter(this);   // the Back key
  qApp->installEventFilter(new touchFlick(this));   // a finger scrolls the pages, whatever it starts on
  arrangePages();
}

/*!
  A phone on its side is short (about 400 dp): the navigation moves to the left edge, and on the Receive page the spectrum and the
  settings stand in a column beside the picture, so the picture keeps the height.
*/
void mainWindow::arrangeRail(bool on)
{
  if(on==rail || !nav) return;
  rail=on;
  nav->setRail(on);
  shellLayout->removeWidget(nav);
  shellLayout->insertWidget(on ? 0 : 1,nav);
  shellLayout->setDirection(on ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);

  QBoxLayout *rx=ui->verticalLayout_2;
  rx->removeWidget(ui->spectrumFrame);
  rx->removeWidget(rxScroll);
  if(on)
    {
      if(!rxSide)
        {
          rxSide=new QWidget(ui->rxTab);
          QVBoxLayout *sl=new QVBoxLayout(rxSide);
          sl->setContentsMargins(0,0,0,0);
          sl->setSpacing(2);
        }
      QVBoxLayout *sl=static_cast<QVBoxLayout *>(rxSide->layout());
      QWidget *settings=ui->rxWindow->takeSettings();
      settings->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);   // as wide as the column
      rxSideScroll=makeTouchScrollPage(settings,rxSide);
      sl->addWidget(ui->spectrumFrame);
      sl->addWidget(rxSideScroll,1);
      ui->spectrumFrame->setFixedHeight(120);
      rx->setDirection(QBoxLayout::LeftToRight);
      rx->addWidget(rxScroll,3);
      rx->addWidget(rxSide,2);
      rxSide->show();
    }
  else
    {
      QVBoxLayout *sl=static_cast<QVBoxLayout *>(rxSide->layout());
      sl->removeWidget(ui->spectrumFrame);
      sl->removeWidget(rxSideScroll);
      rxSideScroll->takeWidget();
      rxSideScroll->deleteLater();
      rxSideScroll=nullptr;
      ui->rxWindow->giveBackSettings();
      rx->removeWidget(rxSide);
      rxSide->hide();
      rx->setDirection(QBoxLayout::TopToBottom);
      ui->spectrumFrame->setFixedHeight(170);
      rx->addWidget(ui->spectrumFrame);
      rx->addWidget(rxScroll,1);
    }
}

void mainWindow::arrangePages()
{
  arrangeRail(width()>height() && height()<480);
  const int w=uiMode::availableWidth(this)-(rail ? 96 : 0);   // the pages have what the navigation rail leaves
  const bool narrow=uiMode::narrow(w);
  ui->rxWindow->arrange(narrow,w);
  ui->txWindow->arrange(narrow,w);
}

void mainWindow::goBack()
{
  // some phones deliver a Back gesture twice in quick succession (a key press and the system's own callback): the second one
  // would close what the first one only stepped out of (a Settings section, then the whole of Settings)
  static QElapsedTimer last;
  if(last.isValid() && last.elapsed()<250) return;
  last.restart();
  QWidget *front=QApplication::activePopupWidget();
  if(!front) front=QApplication::activeModalWidget();
  if(!front)
    {
      QWidget *w=QApplication::activeWindow();
      if(w && w!=this) front=w;   // a viewer or another window of its own
    }
  if(front)
    {
      backAware *levels=dynamic_cast<backAware *>(front);
      if(!levels || !levels->backPressed())
        {
          // reject(), not close(): a message box with no cancel button (About...) ignores being closed
          if(QDialog *d=qobject_cast<QDialog *>(front)) d->reject();
          else front->close();
        }
    }
  else if(galleryWidgetPtr && galleryWidgetPtr->leaveSelectMode()) return;   // Select off before anything else
  else if(ui->maintabWidget->currentIndex()!=0) ui->maintabWidget->setCurrentIndex(0);
  else platformService::moveToBackground();
}

//! touch shell: the Back key
bool mainWindow::eventFilter(QObject *o,QEvent *e)
{
  if((e->type()==QEvent::KeyPress || e->type()==QEvent::KeyRelease) && static_cast<QKeyEvent *>(e)->key()==Qt::Key_Back)
    {
      // Android's Back key (and gesture): the program never ends by it, it closes what is in front, then goes back to Receive,
      // then to the background
      if(e->type()==QEvent::KeyPress && !static_cast<QKeyEvent *>(e)->isAutoRepeat()) goBack();
      e->accept();
      return true;
    }
  if(e->type()==QEvent::Polish && o->isWidgetType()) prepareTouchWindow(static_cast<QWidget *>(o));
  if(e->type()==QEvent::Show && o->isWidgetType())
    {
      // a maximized window of its own is under the status bar and the navigation bar, like the main window
      QWidget *w=static_cast<QWidget *>(o);
      if(w->isWindow() && w->property("touchMaximized").toBool() && w->windowHandle() && !w->property("safeAreaSet").toBool())
        {
          w->setProperty("safeAreaSet",true);
          w->setContentsMargins(w->windowHandle()->safeAreaMargins());
          connect(w->windowHandle(),&QWindow::safeAreaMarginsChanged,w,[w]{w->setContentsMargins(w->windowHandle()->safeAreaMargins());});
        }
    }
  return QMainWindow::eventFilter(o,e);
}

void mainWindow::resizeEvent(QResizeEvent *e)
{
  QMainWindow::resizeEvent(e);
  if(uiMode::touch()) arrangePages();
}

void mainWindow::showEvent(QShowEvent *e)
{
  QMainWindow::showEvent(e);
  if(uiMode::touch() && windowHandle() && !safeAreaConnected)
    {
      // Android 15 and later draw an app under the status bar and the navigation bar: keep the content inside what is visible
      safeAreaConnected=true;
      connect(windowHandle(),&QWindow::safeAreaMarginsChanged,this,&mainWindow::applySafeArea);
    }
  applySafeArea();
}

void mainWindow::applySafeArea()
{
  if(uiMode::touch() && windowHandle()) setContentsMargins(windowHandle()->safeAreaMargins());
}

void mainWindow::askMicrophone()
{
#ifdef Q_OS_ANDROID
  if(micAsked || platformPermissions::microphoneGranted()) return;
  micAsked=true;
  platformPermissions::requestMicrophone(this,[this](bool granted)
    {
      if(granted)
        {
          rxWantedActive=true;
          restartSound(false);
          QTimer::singleShot(2000,this,[]{askForNotifications();});
        }
      else
        {
          micDenied=true;
          if(uiMode::qml())
            {
              qmlUi::micDenied(true);   // the Receive screen says so, with a button to the system settings
            }
          else
            {
              QMessageBox *box=new QMessageBox(QMessageBox::Warning,tr("Microphone"),
                                               tr("QSSTV-Experimental needs the microphone permission to receive. Allow it in the system settings "
                                                  "(Permissions, Microphone); the receiver starts when you come back."),QMessageBox::Ok,this);
              QPushButton *settings=box->addButton(tr("Open settings"),QMessageBox::ActionRole);
              connect(settings,&QPushButton::clicked,this,[]{androidOpenAppSettings();});
              box->setAttribute(Qt::WA_DeleteOnClose);
              box->setWindowModality(Qt::NonModal);
              box->show();
            }
        }
    });
#endif
}

void mainWindow::init()
{
  if(!fileDecoderPtr->isBatch()) // a headless decode must not prune the user's image caches
    {
      cleanUpCache(rxSSTVImagesPath);
      cleanUpCache(rxDRMImagesPath);
      cleanUpCache(txSSTVImagesPath);
      cleanUpCache(txDRMImagesPath);
    }
  //start rx and tx threads
  rxWidgetPtr->functionsPtr()->start();
  txWidgetPtr->functionsPtr()->start();
#ifdef Q_OS_ANDROID
  if(!platformPermissions::microphoneGranted())
    {
      // Android asks the user first; the receiver starts as soon as the answer is yes (rxWantedActive is what restartSound() reads).
      // On the very first start the Qt Quick setup explains the sound first and asks then (AppModel::askMicrophone)
      if(!qmlUi::firstRunPending()) askMicrophone();
    }
  else
    {
      restartSound(true);
      QTimer::singleShot(4000,this,[]{askForNotifications();});
    }
#else
  restartSound(true);
#endif
  dispatcherPtr->init();
  galleryWidgetPtr->init();
  txWidgetPtr->init();
  waterfallPtr->init();
  rigControllerPtr->init();
  rxWidgetPtr->init();
  if(!rigControllerPtr->initError.isEmpty())
    {
      splashMessage(rigControllerPtr->initError);
    }
  startTimer(1000);
  QTimer *lockTimer=new QTimer(this);
  connect(lockTimer,&QTimer::timeout,this,&mainWindow::updateModeLock);
  lockTimer->start(250);
  if(fileWatcherPtr==nullptr) fileWatcherPtr=new fileWatcher;
  fileWatcherPtr->init();
  // the first-run questions are modal boxes (and the update check goes to GitHub): not in a headless run, nobody could answer them (#157)
  const bool headless=fileDecoderPtr->isBatch() || headlessEncode;
  if(!headless) txWidgetPtr->gpsSettingsChanged();   // a GPS button that was left on starts following the GPS (a headless run never opens the port)
#ifndef Q_OS_ANDROID   // no AppImage menu entry and no GitHub update question on Android
  if(!headless && appImageIntegration::available())
    QTimer::singleShot(4000,this,&mainWindow::offerDesktopIntegration);
  if(!headless && !appImageIntegration::available())
    QTimer::singleShot(6000,this,&mainWindow::offerUpdateCheck);
#endif
#ifdef Q_OS_ANDROID
  // Android may end the process while the app is in the background, with no clean exit (and so no writeSettings() from
  // shutdown()): save the settings whenever the app leaves the foreground
  androidVolumeKeysChangeMedia();   // the phone's volume keys set the volume the picture is sent at, also before it starts
  connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state)
    {
      if(state!=Qt::ApplicationActive) writeSettings();
      else
        {
          dispatcherPtr->appResumed();
          androidVolumeKeysChangeMedia();   // the window may be a new one (the activity is recreated after the system ended it)
        }
      if(state==Qt::ApplicationActive && micDenied && platformPermissions::microphoneGranted())
        {
          micDenied=false;   // given in the system settings since: the receiver was waiting for it
          qmlUi::micDenied(false);
          rxWantedActive=true;
          restartSound(false);
        }
    });
#endif
}

/*!
  First run from an AppImage: offer to add the menu entry and icons (once). If it is already integrated but the
  AppImage was moved or renamed, quietly re-point the entry at the new location.
*/
void mainWindow::offerDesktopIntegration()
{
  QString msg;
  if(appImageIntegration::isInstalled())
    {
      if(!appImageIntegration::isCurrent()) appImageIntegration::install(msg);
      return;
    }
  if(appImageIntegration::installedByOtherTool()) return;
  QSettings qs;
  if(qs.value("appimage/integrationAsked",false).toBool()) return;
  QMessageBox box(QMessageBox::Question,tr("Desktop integration"),
                  tr("Add QSSTV-Experimental to your application menu?\n\nThis copies a menu entry and icons into your home directory. "
                     "You can remove them later from Help > Desktop integration or with --uninstall."),
                  QMessageBox::Yes|QMessageBox::No,this);
  QCheckBox *dont=new QCheckBox(tr("Don't ask again"),&box);
  box.setCheckBox(dont);
  int r=box.exec();
  if(r==QMessageBox::Yes)
    {
      bool ok=appImageIntegration::install(msg);
      QMessageBox::information(this,tr("Desktop integration"),msg);
      if(ok) return;
    }
  if(dont->isChecked() || r==QMessageBox::No) qs.setValue("appimage/integrationAsked",true);
}

void mainWindow::slotDesktopIntegration()
{
  QString msg;
  if(appImageIntegration::isInstalled())
    {
      const QString target=appImageIntegration::installedTarget();
      if(appImageIntegration::isCurrent())
        {
          if(QMessageBox::question(this,tr("Desktop integration"),tr("QSSTV-Experimental is in your application menu. Remove it?"))==QMessageBox::Yes)
            {
              appImageIntegration::uninstall(msg);
              QMessageBox::information(this,tr("Desktop integration"),msg);
            }
          return;
        }
      // the entry belongs to another file or release: say so, and let the user move it here or remove it
      QMessageBox box(QMessageBox::Question,tr("Desktop integration"),
                      tr("The application menu entry launches a different AppImage:\n%1\n\nThis one is:\n%2")
                        .arg(target.isEmpty() ? tr("(unknown)") : target,qEnvironmentVariable("APPIMAGE")),
                      QMessageBox::Cancel,this);
      QPushButton *move=box.addButton(tr("Use this AppImage"),QMessageBox::AcceptRole);
      QPushButton *remove=box.addButton(tr("Remove"),QMessageBox::DestructiveRole);
      box.exec();
      if(box.clickedButton()==move) appImageIntegration::install(msg);
      else if(box.clickedButton()==remove) appImageIntegration::uninstall(msg);
      else return;
      QMessageBox::information(this,tr("Desktop integration"),msg);
    }
  else
    {
      appImageIntegration::install(msg);
      QMessageBox::information(this,tr("Desktop integration"),msg);
    }
}

static void openUrlOrWarn(QWidget *parent,const QUrl &url)
{
  if(!appImageIntegration::openUrl(url))
    QMessageBox::warning(parent,QObject::tr("Open web page"),QObject::tr("Could not start a web browser. Please open this address yourself:\n%1").arg(url.toString()));
}

void mainWindow::slotCheckForUpdates()
{
  QString msg;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  int r=appImageIntegration::checkForUpdate(msg);
  QApplication::restoreOverrideCursor();
  if(r==0)
    {
      QMessageBox::information(this,tr("Check for updates"),tr("You are running the latest version."));
    }
  else if(r==1)
    {
      if(QMessageBox::question(this,tr("Check for updates"),tr("A newer version is available. Download and replace this AppImage now?\n(Restart QSSTV-Experimental afterwards.)"))==QMessageBox::Yes)
        {
          if(!appImageIntegration::runUpdate()) QMessageBox::warning(this,tr("Check for updates"),tr("Could not start appimageupdatetool."));
        }
    }
  else
    {
      if(QMessageBox::question(this,tr("Check for updates"),
                               tr("Could not check automatically (%1).\nOpen the releases page in your browser?").arg(msg))==QMessageBox::Yes)
        openUrlOrWarn(this,QUrl("https://github.com/jbdubbs/QSSTV-Experimental/releases"));
    }
}

static void showUpdateResult(QWidget *parent,const updateChecker::Result &r,bool quietIfCurrent)
{
  QSettings qs;
  if(!r.ok)
    {
      if(quietIfCurrent) return;
      if(QMessageBox::question(parent,QObject::tr("Check for updates"),
                               QObject::tr("Could not check for updates (%1).\nOpen the releases page in your browser?").arg(r.error))==QMessageBox::Yes)
        openUrlOrWarn(parent,updateChecker::releasesPage());
      return;
    }
  if(!r.newer)
    {
      if(!quietIfCurrent) QMessageBox::information(parent,QObject::tr("Check for updates"),QObject::tr("You are running the latest version."));
      return;
    }
  if(quietIfCurrent && qs.value("update/skipTag").toString()==r.latestTag) return;
  QMessageBox box(QMessageBox::Question,QObject::tr("Check for updates"),
                  QObject::tr("Version %1 is available (you have %2).\nOpen the download page?").arg(r.latestTag,updateChecker::currentVersion()),
                  QMessageBox::Yes|QMessageBox::No,parent);
  QCheckBox *skip=nullptr;
  if(quietIfCurrent)
    {
      skip=new QCheckBox(QObject::tr("Don't remind me about this version"),&box);
      box.setCheckBox(skip);
    }
  if(box.exec()==QMessageBox::Yes) openUrlOrWarn(parent,r.pageUrl);
  else if(skip!=nullptr && skip->isChecked()) qs.setValue("update/skipTag",r.latestTag);
}

void mainWindow::slotCheckForUpdatesWeb()
{
  updateChecker *uc=new updateChecker(this);
  QApplication::setOverrideCursor(Qt::WaitCursor);
  connect(uc,&updateChecker::finished,this,[this,uc](const updateChecker::Result &r)
  {
    QApplication::restoreOverrideCursor();
    QSettings().setValue("update/lastCheck",QDateTime::currentDateTime());
    showUpdateResult(this,r,false);
    uc->deleteLater();
  });
  uc->start();
}

void mainWindow::slotAutoUpdateToggled(bool on)
{
  QSettings qs;
  qs.setValue("update/autoCheck",on);
  qs.setValue("update/asked",true);
}

/*!
  Non-AppImage builds: ask once whether to look for new versions at startup, then (if enabled) check at most
  once a day and only speak up when something newer is out.
*/
void mainWindow::offerUpdateCheck()
{
  QSettings qs;
  if(!qs.value("update/asked",false).toBool())
    {
      int r=QMessageBox::question(this,tr("Check for updates"),
                                  tr("Check GitHub for new versions of QSSTV-Experimental when it starts?\n\nYou can change this later in the Help menu."));
      qs.setValue("update/asked",true);
      qs.setValue("update/autoCheck",r==QMessageBox::Yes);
      for(QAction *a:ui->menuHelp->actions())
        if(a->isCheckable() && a->text()==tr("Check for updates at startup")) a->setChecked(r==QMessageBox::Yes);
      if(r!=QMessageBox::Yes) return;
    }
  if(!qs.value("update/autoCheck",false).toBool()) return;
  QDateTime last=qs.value("update/lastCheck").toDateTime();
  if(last.isValid() && last.secsTo(QDateTime::currentDateTime())<24*3600) return;
  updateChecker *uc=new updateChecker(this);
  connect(uc,&updateChecker::finished,this,[this,uc](const updateChecker::Result &r)
  {
    if(r.ok) QSettings().setValue("update/lastCheck",QDateTime::currentDateTime());
    showUpdateResult(this,r,true);
    uc->deleteLater();
  });
  uc->start();
}

void mainWindow::restartSound(bool inStartUp)
{
  // Issue #11: a device change applied via slotConfigure() used to leave RX stuck
  // idle -- the new soundIOPtr below is opened but never told to startCapture(),
  // and nothing downstream of restartSound() noticed. Remember whether RX was
  // actually running *before* tearing down the old device (reading this any later
  // risks racing the RX thread's own starved-buffer self-demotion to RXIDLE, which
  // the old device's teardown below triggers), so it can be put back the same way
  // once the new device is up. At startup rxFunctionsPtr hasn't run yet (ctor
  // default is RXIDLE), so this is a no-op there -- startRunning()'s own
  // dispatcherPtr->startRX() call still does the real first-start.
  // rxWantedActive carries the intent across a failed (re)start: with no working device
  // RX is idle, but it should come back once the user picks one that works (issue #73).
  bool rxWasActive=!rxWidgetPtr->functionsPtr()->isIdle() || rxWantedActive;
  // a headless run (--batch, --listen) has nobody to click a message box away; a --listen run also must not take
  // the receiver being idle meanwhile for a lost device
  const bool headless=fileDecoderPtr->isBatch();
  struct restartGuard
  {
    bool on;
    explicit restartGuard(bool h) : on(h) {if(on) fileDecoderPtr->soundRestarting(true);}
    ~restartGuard() {if(on) fileDecoderPtr->soundRestarting(false);}
  } guard(headless);
  //first check if sound
  if(soundIOPtr!=nullptr)
    {
      soundIOPtr->stopSoundThread();
      delete soundIOPtr;
      soundIOPtr=nullptr;
    }
  soundIOPtr=newSoundBackend();
  // soundIOPtr is destroyed and recreated above every call, so this connection has
  // to be redone every time too -- see soundBase::deviceLost()'s doc comment and
  // recoverSound() for why.
  connect(soundIOPtr,&soundBase::deviceLost,this,&mainWindow::recoverSound);
  connect(soundIOPtr,&soundBase::audioNotice,this,[this](const QString &message){statusBarPtr->showMessage(message,10000);});
  bool soundOK=soundIOPtr->init(BASESAMPLERATE);
  if(!soundOK)
    {
      if(inStartUp)
        {
          splashMessage(QString("Soundcard error: %1").arg(soundIOPtr->getLastError()));
        }
      else if(headless)
        {
          fileDecoderPtr->soundFailed(soundIOPtr->getLastError());
        }
      else
        {
          if(uiMode::qml()) qmlUi::problem(tr("Soundcard error"),soundIOPtr->getLastError());
          else QMessageBox::critical(this, tr("Soundcard error"),soundIOPtr->getLastError());
        }
    }
  soundIOPtr->start();
  rxWantedActive=rxWasActive && !soundOK;
  if(!soundOK) return; // leave RX idle; fixing the device in Settings calls restartSound() again
  // Re-arm RX on the new device the same way every other resume path already does
  // (rxwidget.cpp's Start button, txwidget.cpp's post-TX handoff, filedecoder.cpp's
  // post-decode resume) -- dispatcher::startRX() calls soundIOPtr->startCapture()
  // and rxFunctionsPtr->startRX() together. Gated on rxWasActive so a device change
  // made while RX was deliberately stopped doesn't turn it back on.
  if(rxWasActive) dispatcherPtr->startRX();
}

// Windows-sleep follow-up to issue #11: an audio device that dies out from under RX
// (most notably a Windows system sleep/resume cycle) needs the exact same recovery
// as a user-initiated device change in Settings -- rebuild soundIOPtr and re-arm RX
// if it was running. Reached from two independent detectors: nativeEvent() below
// (WM_POWERBROADCAST, Windows-only, authoritative regardless of what the audio
// backend reports) and soundBase::deviceLost() (connected in restartSound(),
// cross-platform fallback/diagnostic for whatever the backend does report).
void mainWindow::recoverSound(const QString &reason)
{
  APPLOG("qsstv-tx","recoverSound: %s",qPrintable(reason));
  // Rebuilding the audio device out from under an active PTT-keyed transmission
  // would be actively harmful -- leave it alone and let the user recover manually
  // (Stop/Start) if a transmission genuinely got stuck across a sleep. Not handled
  // here; see the plan's accepted-risk note.
  if(txWidgetPtr->functionsPtr()->getTXState()!=txFunctions::TXIDLE)
    {
      addToLog(QString("Sound device recovery skipped (TX active): %1").arg(reason),LOGSOUND);
      return;
    }
  // deviceLost() is emitted from inside the QAudioSource/Sink that restartSound() deletes,
  // so never rebuild synchronously; drop repeats that arrive while one is pending.
  if(recovering) return;
  recovering=true;
  addToLog(QString("Recovering sound device: %1").arg(reason),LOGSOUND);
  QTimer::singleShot(0,this,[this]()
    {
      restartSound(false);
      recovering=false;
    });
}

#ifdef Q_OS_WIN
bool mainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
  if(eventType=="windows_generic_MSG")
    {
      MSG *msg=static_cast<MSG*>(message);
      if(msg->message==WM_POWERBROADCAST
         && (msg->wParam==PBT_APMRESUMEAUTOMATIC || msg->wParam==PBT_APMRESUMESUSPEND))
        {
          recoverSound("Windows resumed from sleep");
        }
    }
  return QMainWindow::nativeEvent(eventType,message,result);
}
#endif


void mainWindow::startRunning(bool startCardRx)
{
  inStartup=false;
  if(startCardRx) dispatcherPtr->startRX();
}

void mainWindow::slotDecodeFromFile()
{
  fileDecoderPtr->chooseAndDecode(this);
}


void mainWindow::readSettings()
{


  QSettings qSettings;
  qSettings.beginGroup("MAIN");
  int windowWidth = qSettings.value("windowWidth", 460 ).toInt();
  int windowHeight = qSettings.value("windowHeight", 530 ).toInt();
  int windowX = qSettings.value( "windowX", -1 ).toInt();
  int windowY = qSettings.value( "windowY", -1 ).toInt();
  resize( windowWidth, windowHeight );
  if ( windowX != -1 || windowY != -1 )
    {
      move( windowX, windowY );
    }
  transmissionModeIndex=static_cast<etransmissionMode>(qSettings.value("transmissionModeIndex",0).toInt());
  qSettings.endGroup();
  ui->spectrumFrame->readSettings();
  //  configDialogPtr->readSettings();
}

void mainWindow::writeSettings()
{
  QSettings qSettings;
  qSettings.beginGroup("MAIN");
  qSettings.setValue( "windowWidth", width() );
  qSettings.setValue( "windowHeight", height() );
  qSettings.setValue( "windowX", x() );
  qSettings.setValue( "windowY", y() );
  qSettings.setValue("transmissionModeIndex",static_cast<int>(transmissionModeIndex));
  galleryWidgetPtr->writeSettings();
  rxWidgetPtr->writeSettings();
  txWidgetPtr->writeSettings();
  configDialogPtr->writeSettings();
  qSettings.endGroup();
  ui->spectrumFrame->writeSettings();
}


/**
 *\todo fontselection
 */
void mainWindow::setNewFont()
{
  //  QFont fnt;
  //  fnt.fromString(fontString);
  //  setFont(fnt);
  //  galMW->setFont(fnt);
  //  rxMW->setFont(fnt);
  //  txMW->setFont(fnt);
}

void mainWindow::slotSaveWaterfallImage()
{
  QImage *wf = ui->spectrumFrame->getImage();
  if(!wf)
    {
      QMessageBox::information(this,tr("Save Waterfall Image"),tr("There is no waterfall image to save yet."));
      return;
    }
  double freq=0;
  QDateTime now=QDateTime::currentDateTime();
  QImage im(wf->width()+2, wf->height()+22, QImage::Format_RGB32);
  {
    QPainter p(&im);
    im.fill(Qt::black);
    p.setPen(Qt::lightGray);
    p.drawImage(2,20,*wf);
    p.drawRect(0,0,wf->width()+1,wf->height()+21);

    rigControllerPtr->getFrequency(freq);
    if (freq>0)
      p.drawText(1,15, QString("%1 MHz").arg(freq/1000000.0,1,'f',6));
    else
      p.drawText(1,15, "no freq");
  }
  delete wf;

  // ask where to save: the old silent save into the working directory was easy to miss
  QString dflt=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if(dflt.isEmpty()) dflt=QDir::homePath();
  QString fn=QFileDialog::getSaveFileName(this,tr("Save Waterfall Image"),
                                          dflt+"/waterfall-"+now.toString("yyyyMMddhhmmss")+".jpg",
                                          tr("Images (*.jpg *.png)"));
  if(fn.isEmpty()) return;
  if(QFileInfo(fn).suffix().isEmpty()) fn+=".jpg";
  if(im.save(fn))
    {
      statusBarPtr->showMessage("Saved "+fn,10000);
    }
  else
    {
      QMessageBox::warning(this,tr("Save Waterfall Image"),tr("Unable to save image to:\n%1").arg(fn));
    }
}

void mainWindow::slotExit()
{
  if(quitting) return;
  int exit=QMessageBox::Yes;
  if(confirmClose)
    {
      // "QSSTV" here is this fork's continued reference to the original QSSTV 9.5.11 by
      // Johan Maes, ON4QZ (https://www.qsl.net/o/on4qz), renamed QSSTV-Experimental below.
      exit=QMessageBox::question(nullptr, tr("Quit..."),tr("Do you really want to quit QSSTV-Experimental?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    }

  if(exit==QMessageBox::Yes)
    {
      if(!dispatcherPtr->closeTemplateEditor()) return;   // unsaved template: the user chose to go back
      quitting=true;
      shutdown(true);
      QApplication::quit();
    }
}

/*!
  Stop the receiver, transmitter and sound threads. interactive==false (the headless batch decode) leaves the
  settings file and the FTP "offline" notice alone.
*/
void mainWindow::shutdown(bool interactive)
{
  statusBarPtr->showMessage("Cleaning up...");
  dispatcherPtr->idleAll();
  if(interactive) rxWidgetPtr->setOnlineStatus(false);
  rxWidgetPtr->functionsPtr()->stopThread();
  txWidgetPtr->functionsPtr()->stopThread();
  if(soundIOPtr) soundIOPtr->stopSoundThread();
  if(interactive) writeSettings();
}

void  mainWindow::closeEvent ( QCloseEvent *e )
{
  if(quitting)
    {
      // quit() closes every window first; accept so it can complete (shutdown already ran)
      e->accept();
      return;
    }
  slotExit();
  e->ignore();
}

void mainWindow::slotConfigure()
{

  if(configDialogPtr->exec()==QDialog::Accepted)
    {
      if(configDialogPtr->soundNeedsRestart)
        {
          restartSound(false);
        }
      dispatcherPtr->init();
      txWidgetPtr->positionFormatChanged();
      txWidgetPtr->gpsSettingsChanged();   // the GPS may have been enabled or disabled, or another port or baud rate chosen
    }
}

void mainWindow::slotLogSettings()
{
  logFilePtr->maskSelect(this);
  logFilePtr->writeSettings();

}

void mainWindow::slotResetLog()
{
  logFilePtr->reset();
}

void mainWindow::slotDocumentation()
{
  openUserGuide(QString());
}

void mainWindow::openUserGuide(const QString &page)
{
  // A user-set URL wins; otherwise open the User's Guide shipped with the app
  if(!docURL.trimmed().isEmpty())
    {
      openUrlOrWarn(this,QUrl::fromUserInput(docURL.trimmed()));
      return;
    }
  const QString appDir=QCoreApplication::applicationDirPath();
  const QStringList candidates={
#ifdef Q_OS_ANDROID
                                QStringLiteral(":/userguide"),   // compiled into the app
#endif
                                appDir+"/userguide",
                                appDir+"/../share/doc/qsstv/userguide",
                                appDir+"/../Resources/userguide",   // macOS .app bundle
                                QStringLiteral(QSSTV_USERGUIDE_SRC_DIR)};
  for(const QString &dir:candidates)
    {
      const QString index=QDir(dir).filePath("index.html");
      if(QFileInfo::exists(index))
        {
          // served over http on localhost: a file:// URL breaks in sandboxed (Flatpak/Snap) browsers,
          // which only get the single file and 404 on every link to another page or image
          QUrl url=guideServer::instance().urlFor(dir);
#ifdef Q_OS_ANDROID
          __android_log_print(ANDROID_LOG_INFO,"qsstv-files","user's guide: %s from %s",qPrintable(url.toString()),qPrintable(dir));
#endif
          if(url.isEmpty()) url=QUrl::fromLocalFile(QFileInfo(index).canonicalFilePath());
          if(!page.isEmpty() && QFileInfo::exists(QDir(dir).filePath(page))) url=url.resolved(QUrl(page));
          openUrlOrWarn(this,url);
          return;
        }
    }
  if(uiMode::qml()) qmlUi::problem(tr("Users Guide"),tr("The built-in Users Guide was not found.\nSee https://github.com/jbdubbs/QSSTV-Experimental"));
  else QMessageBox::warning(this,tr("Users Guide"),tr("The built-in Users Guide was not found.\nSee https://github.com/jbdubbs/QSSTV-Experimental"));
}



void mainWindow::slotAboutQSSTV()
{
  // QSSTV-Experimental is built upon the original QSSTV 9.5.11 by Johan Maes, ON4QZ
  // (https://www.qsl.net/o/on4qz) -- credited below, ahead of this fork's own info.
  QString temp=tr("QSSTV-Experimental\nVersion ") + MAJORVERSION + MINORVERSION;
  temp += "\nhttps://github.com/jbdubbs/QSSTV-Experimental\nJason Weisberger - NT0Y";
  temp += "\n\nBuilt upon the original:\nQSSTV\nVersion 9.5.11\nhttps://www.qsl.net/o/on4qz\nCopyright 2000-2019 - Johan Maes - ON4QZ\nHAMDRM Software based on RX/TXAMADRM - PA0MBO";
  temp += "\n\nLicensed under the GNU GPL v3. Includes code from Dream (DRM), Phil Karn KA9Q, MiXViews, Maia XML-RPC, the Papirus icons and others; see Third-party licenses.";
  auto showLicenses=[this]()
  {
    QFile f(":/THIRD_PARTY_LICENSES.md");
    QString text;
    if(f.open(QIODevice::ReadOnly)) text=QString::fromUtf8(f.readAll());
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Third-party licenses"));
    QVBoxLayout *lay=new QVBoxLayout(&dlg);
    QTextBrowser *tb=new QTextBrowser(&dlg);
    tb->setOpenExternalLinks(true);
    tb->setMarkdown(text);
    lay->addWidget(tb);
    dlg.resize(900,600);
    dlg.exec();
  };
  if(uiMode::touch())
    {
      // Android's own message box ignores the Back key when it has a button of ours, so the About box is a dialog of the program
      QDialog dlg(this);
      dlg.setWindowTitle(tr("About..."));
      QVBoxLayout *lay=new QVBoxLayout(&dlg);
      QTextBrowser *tb=new QTextBrowser(&dlg);
      tb->setPlainText(temp);
      lay->addWidget(tb,1);
      QHBoxLayout *row=new QHBoxLayout;
      QPushButton *licButton=new QPushButton(tr("Third-party licenses..."),&dlg);
      QPushButton *okButton=new QPushButton(tr("OK"),&dlg);
      bool licenses=false;
      connect(licButton,&QPushButton::clicked,&dlg,[&dlg,&licenses]{licenses=true;dlg.accept();});
      connect(okButton,&QPushButton::clicked,&dlg,&QDialog::accept);
      row->addWidget(licButton);
      row->addStretch(1);
      row->addWidget(okButton);
      lay->addLayout(row);
      dlg.resize(800,1000);   // the whole screen (prepareTouchWindow)
      dlg.exec();
      if(licenses) showLicenses();
      return;
    }
  QMessageBox box(QMessageBox::Information,tr("About..."),temp,QMessageBox::Ok,this);
  box.setIconPixmap(QPixmap(":/icons/qsstv.png").scaled(64,64,Qt::KeepAspectRatio,Qt::SmoothTransformation));
  QPushButton *licButton=box.addButton(tr("Third-party licenses..."),QMessageBox::ActionRole);
  box.exec();
  if(box.clickedButton()==licButton) showLicenses();
}

void mainWindow::slotAboutQt()
{
  QMessageBox::aboutQt(this,tr("About..."));

}
void mainWindow::setPTT(bool p)
{
  if(p) pttIcon->setPixmap(*redPXMPtr);
  else pttIcon->setPixmap(*greenPXMPtr);
}

void mainWindow::setSSTVDRMPushButton(bool inDRM)
{
  int i;
  QString modeStr;
  if(rxWidgetPtr) rxWidgetPtr->setFixFileEnabled(inDRM);
  freqComboBox->clear();
  if(inDRM) modeStr="DRM";
  else modeStr="SSTV";
  modModeList.clear();
  modPassBandList.clear();
  for(i=0;i<freqList.count();i++)
    {
      if(modeList.at(i)==modeStr)
        {
          freqComboBox->addItem(freqList.at(i));
          modModeList.append(sbModeList.at(i));
          modPassBandList.append(passBandList.at(i));
        }
    }
}

void mainWindow::slotCalibrate()
{
  // the calibration needs the soundcard to itself; resume receive afterwards if it was running
  bool rxWasActive=soundIOPtr->isCapturing();
  calibration calib(this);
  auto applyClocks=[this](double newRx,double newTx)
  {
    // a method that did not measure a clock returns 0: keep the current value
    double rx=newRx>0? newRx:rxClock;
    double tx=newTx>0? newTx:txClock;
    // also updates the Configuration > Sound fields, which writeSettings() reads back over the globals
    configDialogPtr->setSoundClocks(rx,tx);
    writeSettings();
  };
  connect(&calib,&calibration::clocksSaved,this,applyClocks);
  if(calib.exec()==QDialog::Accepted) applyClocks(calib.getRXClock(),calib.getTXClock());
  if(rxWasActive) dispatcherPtr->startRX();
}

int mainWindow::busyMode()
{
  if(txWidgetPtr==nullptr || rxWidgetPtr==nullptr) return -1;
  if(txWidgetPtr->functionsPtr()->txBusy() || rxWidgetPtr->rxBusy()) return transmissionModeIndex;
  return -1;
}

void mainWindow::updateModeLock()
{
  int busy=busyMode();
  txWidgetPtr->setModeLock(busy);
  rxWidgetPtr->setModeLock(busy);
  rxWidgetPtr->setFixFileEnabled((busy<0)&&(transmissionModeIndex==TRXDRM));
}

void mainWindow::updateBeaconCountdownVisibility()
{
  bool show=!beaconCountdownLabel->text().isEmpty() && statusBarPtr->currentMessage().isEmpty();
  if(show!=beaconCountdownLabel->isVisibleTo(this)) beaconCountdownLabel->setVisible(show);
}

void mainWindow::slotModeChange(int rxtxMode)
{
  int busy=busyMode();
  if((busy>=0) && (rxtxMode!=busy))
    {
      // never interrupt a running transmission/reception: put both tab bars back
      txWidgetPtr->changeTransmissionMode(busy);
      rxWidgetPtr->syncModeTab(busy);
      updateModeLock();
      return;
    }
  txWidgetPtr->changeTransmissionMode(rxtxMode);
  rxWidgetPtr->changeTransmissionMode(rxtxMode);
  qmlUi::notifyModeChanged();
  lastReceivedCall.clear();
  if(!fileDecoderPtr->isBatch() && !headlessEncode)
    {
      // the SSTV / DRM tab is remembered at once, not only when the program quits cleanly
      QSettings qSettings;
      qSettings.setValue("MAIN/transmissionModeIndex",static_cast<int>(transmissionModeIndex));
    }
}

void mainWindow::slotSendBSR()
{
  txWidgetPtr->sendBSR();
}


void mainWindow::slotSendWFID()
{
  txWidgetPtr->sendWFID();
}

void mainWindow::slotSendCWID()
{
  txWidgetPtr->sendCWID();
}

void mainWindow::slotSendWfText()
{
  txWidgetPtr->sendWfText();
}

void mainWindow::slotSetFrequency(int freqIndex)
{
  QByteArray ba;
  QString freqStr,mode,passBand;
  freqStr=freqComboBox->itemText(freqIndex);
  mode=modModeList.at(freqIndex);
  passBand=modPassBandList.at(freqIndex);
  if(freqStr.isEmpty()) return;
  double fr=freqStr.toDouble()*1000000.;
  rigControllerPtr->setFrequency(fr);
  rigControllerPtr->setMode(mode,passBand);
  QString s=additionalCommand;
  //FEF7AE01A060101FD
  if(!s.isEmpty() && !rigControllerPtr->params()->enableXMLRPC)
    {
      if(hexFromString(s,ba,additionalCommandHex))
        {
          rigControllerPtr->rawCommand(ba);
        }
      else
        {
          QMessageBox::critical(this,"Cat Error","Advanced command invalid");
        }
    }
}




void mainWindow::timerEvent(QTimerEvent *)
{
  double fr;
  if(rigControllerPtr->getFrequency(fr))
    {
      fr/=1000000.;
      if(fr>1) freqDisplay->setText(QString::number(fr,'f',6));
    }
  else
    {
      freqDisplay->setText("No Rig");
    }
#ifdef Q_OS_ANDROID
  // the foreground service that keeps the microphone working in the background runs exactly while the sound backend
  // is receiving or sending (driverOK(): the permission was given and the device opened)
  platformService::setActive(soundIOPtr && soundIOPtr->driverOK() && (soundIOPtr->isCapturing() || soundIOPtr->isPlaying()));
  // a muted, blocked or overloaded microphone: say so while it lasts (soundBase::inputProblem())
  const QString problem=soundIOPtr ? soundIOPtr->inputProblem() : QString();
  if(problem!=inputProblemShown)
    {
      inputProblemShown=problem;
      if(problem.isEmpty()) statusBarPtr->clearMessage();
      else statusBarPtr->showMessage(problem);
    }
#endif
}

void mainWindow::cleanUpCache(QString dirPath)
{
  int i,j;
  bool found;
  QFileInfoList orgFileList;
  QFileInfoList cacheFileList;
  QFileInfoList removeFileList;
  QDir dirOrg(dirPath);
  dirOrg.setFilter(QDir::Files | QDir::NoSymLinks);
  orgFileList = dirOrg.entryInfoList();
  // get a orgiriginal filelist
  QDir dirCache(dirPath+"/cache");
  dirCache.setFilter(QDir::Files | QDir::NoSymLinks);
  cacheFileList = dirCache.entryInfoList();
  // get a orgiriginal filelist
  for(i=0;i<cacheFileList.count();i++)
    {
      found=false;
      for(j=0;j<orgFileList.count();j++)
        {
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
          if(cacheFileList.at(i).baseName()==QString(orgFileList.at(j).baseName()+orgFileList.at(j).created().toString()))
#else
          if(cacheFileList.at(i).baseName()==QString(orgFileList.at(j).baseName()+orgFileList.at(j).birthTime().toString()))
#endif
            {
              found=true;
              break;
            }
        }
      if(!found)
        {
          removeFileList.append(cacheFileList.at(i));
        }
    }
  for(i=0;i<removeFileList.count();i++)
    {
      QFile fi(removeFileList.at(i).absoluteFilePath());
      fi.remove();
    }
}

void mainWindow::slotFullScreen()
{
  if(isFullScreen())
    {
      this->setWindowState(Qt::WindowMaximized);
    }
  else
    {
      this->setWindowState(Qt::WindowFullScreen);
    }
}



#ifndef QT_NO_DEBUG

void mainWindow::slotShowDataScope()
{
  scopeViewerData->show(true,true,true,true);
}
void mainWindow::slotShowSyncScopeNarrow()
{
  scopeViewerSyncNarrow->show(true,true,true,true);
}

void mainWindow::slotShowSyncScopeWide()
{
  scopeViewerSyncWide->show(true,true,true,true);
}

void mainWindow::slotScopeOffset()
{
  dataScopeOffset=rxWidgetPtr->functionsPtr()->setOffset(dataScopeOffset,true);
}



void mainWindow::slotDumpSamplesPerLine()
{
  dumpSamplesPerLine();
}

void mainWindow::slotClearScope()
{
  scopeViewerData->clear();
  scopeViewerSyncNarrow->clear();
  scopeViewerSyncWide->clear();
}


#endif

void mainWindow::showTransmitPage()
{
  const int txIdx=ui->maintabWidget->indexOf(ui->txTab);
  if(ui->maintabWidget->isTabEnabled(txIdx)) ui->maintabWidget->setCurrentIndex(txIdx);
}

void mainWindow::slotTxTestPattern()
{
  etpSelect sel;
  testPatternSelection tpsel;
  if(tpsel.exec()==QDialog::Accepted)
    {
      sel=tpsel.getSelection();
      txWidgetPtr->txTestPattern(sel);
      // show the loaded pattern
      int txIdx=ui->maintabWidget->indexOf(ui->txTab);
      if(ui->maintabWidget->isTabEnabled(txIdx)) ui->maintabWidget->setCurrentIndex(txIdx);
    }
}
