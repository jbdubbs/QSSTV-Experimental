// Screenshots and size checks of the REAL main window, offscreen, in the desktop shell or the touch shells at a given size:
//   shot <scratch-dir> <out-dir> desktop|phone|tablet <width> <height> <page>...
// pages: rx tx gallery more (the More page exists in the touch shells only). Each page is saved as <out-dir>/<shell>-<w>x<h>-<page>.png and
// its minimum size is printed: a window that cannot get as small as the screen is what squeezed and clipped the desktop layout on a phone.
// Exit code 1 when a touch shell's window is wider than asked for. Built and run by tests/uishots/app.sh (links the objects of build-cmake).
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
      if (page == "galleryselect")
        {
          // the Gallery's Select mode (touch shells): six pictures, choose two by tapping, All, then Back turns Select off
          if (!tabs || tabs->count() < 3 || shell == "desktop") {printf("  galleryselect (not in this shell)\n"); continue;}
          QDir().mkpath(rxSSTVImagesPath);
          for (int k = 0; k < 6; k++)
            {
              QImage im(160, 120, QImage::Format_RGB32);
              im.fill(QColor::fromHsv(k * 50, 200, 220));
              im.save(QString("%1/test%2.png").arg(rxSSTVImagesPath).arg(k));
            }
          galleryWidgetPtr->changedMatrix(imageViewer::RXSSTVTHUMB);
          tabs->setCurrentIndex(2);
          QTest::qWait(1500);
          imageMatrix *m = nullptr;
          for (imageMatrix *c : galleryWidgetPtr->findChildren<imageMatrix *>()) if (c->isVisible()) {m = c; break;}
          auto check = [&](bool ok, const char *what) {printf("  %s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) exitCode = 1;};
          if (!m) {check(false, "a visible picture matrix"); continue;}
          QWidget *pg = m->parentWidget();   // the tab page: the matrix puts its pictures and buttons on it
          QPushButton *sel = pg->findChild<QPushButton *>("selectButton");
          check(sel && sel->isVisible(), "a Select button in the Gallery");
          if (!sel) continue;
          QList<imageViewer *> thumbs;
          for (imageViewer *v : pg->findChildren<imageViewer *>()) if (!v->getFilename().isEmpty()) thumbs << v;
          std::sort(thumbs.begin(), thumbs.end(), [](imageViewer *a, imageViewer *b) {return a->getFilename() < b->getFilename();});
          check(thumbs.size() == 6, "six pictures in the matrix");
          auto selectedCount = [&]() {int n = 0; for (imageViewer *v : thumbs) n += v->isSelected(); return n;};
          auto labelText = [&]() {QString t; for (QLabel *l : pg->findChildren<QLabel *>()) if (l->isVisible() && (l->text().contains("select"))) t = l->text(); return t;};
          QTest::mouseClick(sel, Qt::LeftButton);
          QTest::qWait(100);
          check(m->selecting(), "Select is on after tapping it");
          check(labelText().contains("Tap pictures"), "the bar says to tap pictures");
          QTest::mouseClick(thumbs[0], Qt::LeftButton, Qt::NoModifier, thumbs[0]->rect().center());
          QTest::qWait(60);
          QTest::mouseClick(thumbs[2], Qt::LeftButton, Qt::NoModifier, thumbs[2]->rect().center());
          QTest::qWait(60);
          check(selectedCount() == 2 && thumbs[0]->isSelected() && thumbs[2]->isSelected(), "two taps choose two pictures (the first stays chosen)");
          check(labelText().startsWith("2 "), "the bar counts them");
          QTest::mouseClick(thumbs[0], Qt::LeftButton, Qt::NoModifier, thumbs[0]->rect().center());
          QTest::qWait(300);
          check(selectedCount() == 1 && !thumbs[0]->isSelected(), "a second tap takes a picture away again");
          QPushButton *all = nullptr;
          for (QPushButton *b : pg->findChildren<QPushButton *>()) if (b->text() == "All" && b->isVisible()) all = b;
          check(all != nullptr, "an All button");
          if (all) QTest::mouseClick(all, Qt::LeftButton);
          QTest::qWait(60);
          check(selectedCount() == 6, "All chooses every picture of the page");
          mainWindowPtr->grab().save(QString("%1/%2-%3x%4-galleryselect.png").arg(outDir, shell).arg(width).arg(height));
          check(galleryWidgetPtr->leaveSelectMode(), "Back (leaveSelectMode) turns Select off");
          QTest::qWait(60);
          check(!m->selecting() && selectedCount() == 0, "and drops the choice");
          continue;
        }
      const QStringList names = {"rx", "tx", "gallery", "more"};
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
  if (shell != "desktop" && mainWindowPtr->width() > width)
    {
      printf("FAIL the window is %d wide, %d asked for: something in it cannot get narrower\n", mainWindowPtr->width(), width);
      exitCode = 1;
    }
  fflush(stdout);
  _exit(exitCode);   // nothing is saved here
}
