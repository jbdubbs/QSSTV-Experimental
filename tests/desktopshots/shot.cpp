// Screenshots of the desktop interface, offscreen, for comparing two builds (this branch and main) picture by picture:
//   shot <scratch-dir> <out-dir> <width> <height>
// the three main tabs (and the settings tab of the Receive page, every page of the Gallery) and every tab of the Configuration dialog.
// It uses nothing the Android work added, so it builds against either tree. Built and run by tests/desktopshots/app.sh.
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include "appglobal.h"
#include "configparams.h"
#include "dispatcher.h"
#include "mainwindow.h"
#include "configdialog.h"

static void save(QWidget *w, const QString &dir, const QString &name)
{
  QTest::qWait(250);
  if (!w->grab().save(QString("%1/%2.png").arg(dir, name))) fprintf(stderr, "cannot save %s\n", qPrintable(name));
  else printf("  ok    %s\n", qPrintable(name));
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 5) {fprintf(stderr, "usage: shot scratch outdir width height\n"); return 2;}
  const QString scratch = argv[1], outDir = argv[2];
  const int width = atoi(argv[3]), height = atoi(argv[4]);
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("PIPEWIRE_REMOTE", "qsstv-test-no-such-server");
  qputenv("PULSE_SERVER", "unix:/nonexistent/qsstv-test");
  QCoreApplication::setOrganizationName("qsstv-desktopshots");
  QCoreApplication::setApplicationName("qsstv-desktopshots");
  QApplication app(argc, argv);
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch + "/settings");
  {
    QSettings qs;   // the first-run questions would block an offscreen run
    qs.setValue("update/asked", true);
    qs.setValue("update/autoCheck", false);
    qs.setValue("appimage/integrationAsked", true);
  }
  QDir().mkpath(outDir);
  globalInit();
  mainWindowPtr = new mainWindow;
  QTest::qWait(300);
  mainWindowPtr->init();
  mainWindowPtr->startRunning(false);   // no sound card receiver
  mainWindowPtr->resize(width, height);
  mainWindowPtr->show();
  QTest::qWait(800);
  mainWindowPtr->resize(width, height);
  QTest::qWait(300);
  QTabWidget *tabs = mainWindowPtr->findChild<QTabWidget *>("maintabWidget");
  if (!tabs) {fprintf(stderr, "no main tab widget\n"); return 1;}
  printf("desktop %dx%d: main window minimum %dx%d, is %dx%d\n", width, height, mainWindowPtr->minimumSizeHint().width(),
         mainWindowPtr->minimumSizeHint().height(), mainWindowPtr->width(), mainWindowPtr->height());
  const QStringList names = {"rx", "tx", "gallery"};
  for (int i = 0; i < tabs->count() && i < names.size(); i++)
    {
      tabs->setCurrentIndex(i);
      save(mainWindowPtr, outDir, names[i]);
    }
  // the settings tabs of the Receive page (SSTV, DRM)
  tabs->setCurrentIndex(0);
  if (QTabWidget *rxs = tabs->widget(0)->findChild<QTabWidget *>("settingsTableWidget"))
    for (int i = 0; i < rxs->count(); i++)
      {
        rxs->setCurrentIndex(i);
        save(mainWindowPtr, outDir, QString("rx-settings-%1").arg(i));
      }
  // every page of the Gallery
  tabs->setCurrentIndex(2);
  if (QTabWidget *gt = tabs->widget(2)->findChild<QTabWidget *>("tabWidget"))
    for (int i = 0; i < gt->count(); i++)
      {
        gt->setCurrentIndex(i);
        save(mainWindowPtr, outDir, QString("gallery-%1").arg(i));
      }
  // the Configuration dialog, every tab
  configDialog *cd = configDialogPtr;
  cd->resize(900, 650);
  cd->show();
  QTest::qWait(500);
  if (QTabWidget *ct = cd->findChild<QTabWidget *>("configTabWidget"))
    for (int i = 0; i < ct->count(); i++)
      {
        ct->setCurrentIndex(i);
        save(cd, outDir, QString("config-%1").arg(i));
      }
  cd->hide();
  fflush(stdout);
  _exit(0);   // no shutdown: the pages and threads are not worth unwinding here
}
