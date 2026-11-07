// Screenshots of the REAL main window, offscreen, in the desktop shell, and the size checks of the classic Configuration dialog as a phone
// shows it (QSSTV_UI=phone|tablet), at a given size:
//   shot <scratch-dir> <out-dir> desktop|phone|tablet <width> <height> <page>...
// pages: rx tx gallery settings. Each page is saved as <out-dir>/<shell>-<w>x<h>-<page>.png and its minimum size is printed.
// Exit code 1 when a section of the dialog is wider than the screen. Built and run by tests/uishots/app.sh (links the objects of build-cmake).
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include "appglobal.h"
#include "configparams.h"
#include "dispatcher.h"
#include "mainwindow.h"
#include "configdialog.h"
#include "uimode.h"
#include "touchpage.h"
#include "reflow.h"
#include "gallerywidget.h"
#include "imagematrix.h"
#include <QScroller>
#include <QScrollerProperties>
#include "spectrumwidget.h"

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 7) {fprintf(stderr, "usage: shot scratch outdir desktop|phone|tablet width height page...\n"); return 2;}
  const QString scratch = argv[1], outDir = argv[2], shell = argv[3];
  const int width = atoi(argv[4]), height = atoi(argv[5]);
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("PIPEWIRE_REMOTE", "qsstv-test-no-such-server");
  qputenv("PULSE_SERVER", "unix:/nonexistent/qsstv-test");
  if (shell != "desktop")
    {
      qputenv("QSSTV_UI", shell.toLocal8Bit());
      qputenv("QSSTV_UI_WIDTH", QByteArray::number(width));   // the screen of the phone being imitated
      qputenv("QSSTV_UI_SHORT", QByteArray::number(std::min(width, height)));
    }
  QCoreApplication::setOrganizationName("qsstv-uishots");
  QCoreApplication::setApplicationName("qsstv-uishots");
  QApplication app(argc, argv);
  if (uiMode::touch())   // as in main.cpp
    {
      QApplication::setStyle(new touchStyle(QApplication::style()->name()));
      if (qEnvironmentVariableIsSet("SHOT_DARK")) QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
      applyTouchPalette();
    }
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
  mainWindowPtr->resize(width, height);   // the pages have arranged themselves for the screen: the window may get as small as it was asked for now
  QTest::qWait(300);
  QTabWidget *tabs = mainWindowPtr->findChild<QTabWidget *>("maintabWidget");
  if (qEnvironmentVariableIsSet("SHOT_FFT"))
    {
      // what the receiver sends the spectrum display about twelve times a second: a tone and some noise
      std::vector<double> samples(8192);
      for (int frame = 0; frame < 40; frame++)
        {
          for (size_t i = 0; i < samples.size(); i++)
            samples[i] = 3000 * sin(2 * M_PI * 1500.0 * i / 12000.0) + (rand() % 2001 - 1000);
          mainWindowPtr->spectrumFramePtr->realFFT(samples.data());
          QTest::qWait(10);
        }
    }
  int exitCode = 0;
  int settingsOverflow = 0;
  printf("%s %dx%d: main window minimum %dx%d, is %dx%d\n", qPrintable(shell), width, height,
         mainWindowPtr->minimumSizeHint().width(), mainWindowPtr->minimumSizeHint().height(), mainWindowPtr->width(), mainWindowPtr->height());
  for (int i = 6; i < argc; i++)
    {
      const QString page = argv[i];
      if (page == "settings")
        {
          // every section of Settings (touch shells): how wide its page wants to be, and whether it scrolls sideways at this width
          configDialog *cd = configDialogPtr;
          cd->setProperty("touchPrepared", true);   // not maximized (offscreen has its own screen size): the size of the phone
          cd->resize(width, height);
          cd->show();
          QTest::qWait(300);
          cd->resize(width, height);
          QTest::qWait(300);
          QTabWidget *ct = cd->findChild<QTabWidget *>("configTabWidget");
          if (!ct) {printf("  settings (not in this shell)\n"); continue;}
          // the list of sections is dragged like the pages: a tap on an entry (held until the finger is lifted, then delivered) opens it
          if (QListWidget *lw = cd->findChild<QListWidget *>())
            if (QStackedWidget *st = qobject_cast<QStackedWidget *>(ct->parentWidget()))
              {
                st->setCurrentIndex(0);
                QTest::qWait(200);
                QTest::mouseClick(lw->viewport(), Qt::LeftButton, Qt::NoModifier, lw->visualItemRect(lw->item(3)).center());
                QTest::qWait(200);
                printf("  %s  a tap on the fourth entry of the list opens that section\n", (st->currentIndex() == 1 && ct->currentIndex() == 3) ? "ok  " : "FAIL");
                if (!(st->currentIndex() == 1 && ct->currentIndex() == 3)) exitCode = 1;
              }
          if (QStackedWidget *st = qobject_cast<QStackedWidget *>(ct->parentWidget())) st->setCurrentIndex(1);   // a section, not the list
          QTest::qWait(200);
          printf("  settings dialog %dx%d (minimum %dx%d)\n", cd->width(), cd->height(), cd->minimumSizeHint().width(), cd->minimumSizeHint().height());
          for (int k = 0; k < ct->count(); k++)
            {
              ct->setCurrentIndex(k);
              QTest::qWait(150);
              QScrollArea *sa = qobject_cast<QScrollArea *>(ct->widget(k));
              const QString title = ct->tabText(k);
              const QString file = QString("%1/%2-%3x%4-settings-%5.png").arg(outDir, shell).arg(width).arg(height).arg(title);
              cd->grab().save(file);
              const int over = sa ? sa->horizontalScrollBar()->maximum() : -1;
              printf("    %-12s min %4d x %4d, viewport %d wide, sideways scroll %d%s\n", qPrintable(title),
                     sa && sa->widget() ? sa->widget()->minimumSizeHint().width() : -1, sa && sa->widget() ? sa->widget()->minimumSizeHint().height() : -1,
                     sa ? sa->viewport()->width() : -1, over, over > 0 ? "   <-- too wide" : "");
              if (over > 0 && shell != "desktop") settingsOverflow++;
              if (sa && sa->widget() && qEnvironmentVariableIsSet("SHOT_DUMP"))
                for (const QString &o : reflowOffenders(sa->widget(), sa->viewport()->width())) printf("        %s\n", qPrintable(o));
            }
          cd->hide();
          continue;
        }
      const QStringList names = {"rx", "tx", "gallery"};
      const int index = names.indexOf(page);
      if (index < 0) {fprintf(stderr, "unknown page %s\n", argv[i]); continue;}
      if (!tabs || index >= tabs->count()) {printf("  %-8s (not in this shell)\n", argv[i]); continue;}
      tabs->setCurrentIndex(index);
      QTest::qWait(400);
      QWidget *w = tabs->currentWidget();
      const QString file = QString("%1/%2-%3x%4-%5.png").arg(outDir, shell).arg(width).arg(height).arg(page);
      mainWindowPtr->grab().save(file);
      if (qEnvironmentVariableIsSet("SHOT_DUMP"))
        for (QWidget *c : w->findChildren<QWidget *>())
          if (c->isVisible() && (c->objectName() == "imageFrame" || c->objectName() == "settingsTableWidget" || c->objectName() == "rxWindow" || c->objectName() == "txWindow" || c->objectName() == "syncWidget"))
            printf("      %-22s geo %d,%d %dx%d  min %dx%d hint %dx%d\n", qPrintable(c->objectName()), c->mapTo(mainWindowPtr, QPoint(0, 0)).x(), c->mapTo(mainWindowPtr, QPoint(0, 0)).y(), c->width(), c->height(), c->minimumSizeHint().width(), c->minimumSizeHint().height(), c->sizeHint().width(), c->sizeHint().height());
      if (qEnvironmentVariableIsSet("SHOT_DUMP"))
        for (QScrollArea *a : mainWindowPtr->findChildren<QScrollArea *>())
          if (a->isVisible() && a->widget())
            printf("      scroll area: viewport %dx%d, content %dx%d (min %dx%d), vertical scroll range %d, hfw %d\n", a->viewport()->width(), a->viewport()->height(), a->widget()->width(), a->widget()->height(), a->widget()->minimumSizeHint().width(), a->widget()->minimumSizeHint().height(), a->verticalScrollBar()->maximum(), int(a->widget()->layout() && a->widget()->layout()->hasHeightForWidth()));
      printf("  %-8s page minimum %dx%d, saved %s\n", argv[i], w->minimumSizeHint().width(), w->minimumSizeHint().height(), qPrintable(QFileInfo(file).fileName()));
    }
  if (settingsOverflow)
    {
      printf("FAIL %d section(s) of Settings are wider than the screen\n", settingsOverflow);
      exitCode = 1;
    }
  fflush(stdout);
  _exit(exitCode);   // nothing is saved here
}
