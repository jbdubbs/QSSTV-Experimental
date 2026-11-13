// The AppImage update shows how far it is and restarts into the new version when it is complete, in the REAL main window,
// offscreen, with a fake appimageupdatetool (a shell script on the PATH that prints what the real one prints):
//   app <scratch-dir>
//   - parsing of the tool's lines (progress, "New file created")
//   - a progress window with a value that grows, the "ready, restarting" text at the end, then the new AppImage is started and the
//     program quits (the fake new AppImage leaves a marker file)
//   - a reception in progress: the new version is announced, nothing is started and the program keeps running
//   - a tool that fails: a message with its last line, nothing is started
//   - Cancel: no message, nothing is started, the tool is gone
// Built and run by tests/appupdate/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#define private public
#include "sstvrx.h"
#include "mainwindow.h"
#undef private
#include "appglobal.h"
#include "appimageupdater.h"
#include "dispatcher.h"
#include "rxfunctions.h"
#include "rxwidget.h"
#include "txwidget.h"

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}
[[noreturn]] static void finish()
{
  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);
}
static bool waitFor(std::function<bool()> f, int ms)
{
  QElapsedTimer t; t.start();
  while (t.elapsed() < ms) { if (f()) return true; QTest::qWait(5); }
  return f();
}
static QProgressDialog *progressWindow()
{
  for (QWidget *w : QApplication::topLevelWidgets())
    if (w->isVisible()) if (QProgressDialog *p = qobject_cast<QProgressDialog *>(w)) return p;
  return nullptr;
}
// a message box would hold the test in its own event loop: a timer writes down what each one says and closes it
static QStringList seenBoxes;
static void watchBoxes()
{
  for (QWidget *w : QApplication::topLevelWidgets())
    if (w->isVisible()) if (QMessageBox *m = qobject_cast<QMessageBox *>(w)) {
      seenBoxes << m->text();
      printf("  .. a box says: %s\n", qPrintable(m->text().left(110).replace('\n', ' ')));
      m->close();
    }
}
static QString box(const QString &text)
{
  for (const QString &t : seenBoxes) if (t.contains(text)) return t;
  return QString();
}
static void closeBoxes() { seenBoxes.clear(); }
static void writeScript(const QString &path, const QString &body)
{
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
  f.write(("#!/bin/sh\n" + body).toUtf8());
  f.close();
  f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 2) return 2;
  const QString scratch = argv[1];
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("PIPEWIRE_REMOTE", "qsstv-test-no-such-server");
  qputenv("PULSE_SERVER", "unix:/nonexistent/qsstv-test");
  QCoreApplication::setOrganizationName("qsstv-app-test");
  QCoreApplication::setApplicationName("qsstv-app-test");

  // the fake AppImage that is "running", the fake new one, the fake tool
  const QString oldApp = scratch + "/old.AppImage", newApp = scratch + "/new.AppImage", marker = scratch + "/started";
  writeScript(oldApp, "exit 0\n");
  writeScript(newApp, "echo started > '" + marker + "'\n");
  const QString toolDir = scratch + "/bin";
  QDir().mkpath(toolDir);
  const QString toolMode = scratch + "/mode";
  // mode file: ok | fail | slow
  writeScript(toolDir + "/appimageupdatetool",
              "mode=$(cat '" + toolMode + "')\n"
              "echo 'Starting update...' >&2\n"
              "echo 'Validation warning: AppImage not signed' >&2\n"
              "for p in 5.5 20.25 50 75.1 99.5; do printf '\\r\\033[2K%s%% done (%s of 100.00 MiB)...' $p $p; sleep 0.25; done\n"
              "[ \"$mode\" = slow ] && sleep 30\n"
              "echo\n"
              "if [ \"$mode\" = fail ]; then echo 'Error: could not download the control file' >&2; exit 2; fi\n"
              "echo 'Update successful. New file created: " + newApp + "' >&2\n"
              "exit 0\n");
  qputenv("APPIMAGE", oldApp.toLocal8Bit());
  qputenv("APPDIR", (scratch + "/appdir").toLocal8Bit());
  qputenv("PATH", (toolDir + ":" + qEnvironmentVariable("PATH")).toLocal8Bit());

  QApplication app(argc, argv);
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch + "/settings");
  {
    QSettings qs;
    qs.setValue("update/asked", true);
    qs.setValue("update/autoCheck", false);
    qs.setValue("appimage/integrationAsked", true);
  }

  printf("== the tool's lines\n");
  double p = 0, d = 0, t = 0;
  check(appImageUpdater::parseProgress("45.3% done (46.8 of 103.5 MiB)...", p, d, t) && p == 45.3 && d == 46.8 && t == 103.5, "a progress line");
  check(appImageUpdater::parseProgress("100.00% done (103.46 of 103.46 MiB)...", p, d, t) && p == 100.0, "the last progress line");
  check(!appImageUpdater::parseProgress("zsync2: checksum matches OK", p, d, t), "another line is not progress");
  check(appImageUpdater::parseNewFile("x\nUpdate successful. New file created: /a b/new.AppImage\n") == "/a b/new.AppImage", "the new file, with a space in the path");
  check(appImageUpdater::parseNewFile("Update successful.\n").isEmpty(), "no new file named");

  globalInit();
  mainWindowPtr = new mainWindow;
  QTest::qWait(300);
  mainWindowPtr->init();
  mainWindowPtr->hide();
  mainWindowPtr->startRunning(false);
  QTest::qWait(300);
  rxFunctions *rxf = rxWidgetPtr->functionsPtr();
  QTimer watcher; QObject::connect(&watcher, &QTimer::timeout, watchBoxes); watcher.start(30);
  auto setMode = [&](const char *m) { QFile f(toolMode); if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(m); };
  auto reset = [&] { QFile::remove(marker); closeBoxes(); };

  printf("\n== a download that works: progress, then the new version starts\n");
  reset(); setMode("ok");
  mainWindowPtr->startAppImageUpdate();
  check(waitFor([&] { return progressWindow() != nullptr; }, 5000), "a progress window opens");
  QProgressDialog *dlg = progressWindow();
  int last = -1; bool grew = false, texted = false;
  waitFor([&] {
    if (QProgressDialog *w = progressWindow()) {
      if (w->value() > last && last >= 0) grew = true;
      if (w->value() >= 0) last = w->value();
      if (w->labelText().contains("%")) texted = true;
    }
    return last >= 99;
  }, 10000);
  check(grew, QString("the bar grows (now %1 of 100)").arg(last));
  check(texted, "the text says how many percent");
  check(waitFor([&] { QProgressDialog *w = progressWindow(); return w && w->labelText().contains("Restarting"); }, 10000), "the window says the new version is ready and restarting");
  check(!mainWindowPtr->quitting, "the program is not quit in the same moment (the text can be read)");
  check(waitFor([&] { return mainWindowPtr->quitting; }, 8000), "the program quits like Quit does (no event loop runs here to finish it)");
  check(waitFor([&] { return QFile::exists(marker); }, 8000), "and the new AppImage is started");
  (void)dlg;

  // the program is still alive in this test (no event loop runs to end it): put it back
  mainWindowPtr->quitting = false;

  printf("\n== a reception is in progress: nothing is started\n");
  reset(); setMode("ok");
  rxf->sstvRxPtr->SSTVState = sstvRx::PROCESSING;
  check(mainWindowPtr->busyMode() >= 0, "the receiver counts as busy");
  QTimer::singleShot(0, [] {});
  mainWindowPtr->startAppImageUpdate();
  check(waitFor([&] { return !box("not started because").isEmpty(); }, 10000), "a message says the new version is ready and was not started");
  check(!box("new.AppImage").isEmpty(), "and names the file");
  QTest::qWait(3000);
  check(!QFile::exists(marker) && !mainWindowPtr->quitting, "nothing was started and the program runs");
  rxf->sstvRxPtr->SSTVState = sstvRx::HUNTING;
  reset();

  printf("\n== the tool fails\n");
  reset(); setMode("fail");
  mainWindowPtr->startAppImageUpdate();
  check(waitFor([&] { return !box("The update failed").isEmpty(); }, 10000), "a message says the update failed");
  const QString m = box("The update failed");
  check(m.contains("could not download the control file"), "with the last line of the tool");
  check(m.contains("releases"), "and where to get it by hand");
  QTest::qWait(1500);
  check(!QFile::exists(marker) && !mainWindowPtr->quitting, "nothing was started");
  reset();

  printf("\n== Cancel\n");
  reset(); setMode("slow");
  mainWindowPtr->startAppImageUpdate();
  check(waitFor([&] { QProgressDialog *w = progressWindow(); return w && w->value() > 40; }, 10000), "the bar is under way");
  progressWindow()->findChild<QPushButton *>()->click();   // the Cancel button
  QTest::qWait(500);
  check(!mainWindowPtr->updater->running(), "the tool was stopped");
  check(box("failed").isEmpty() && box("ready").isEmpty(), "no message");
  QTest::qWait(1500);
  check(!QFile::exists(marker) && !mainWindowPtr->quitting, "nothing was started");

  finish();
}
