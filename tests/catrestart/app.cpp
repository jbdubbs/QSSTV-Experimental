// CAT restart (issue #211): a start that fails (wrong radio model, a port the radio does not answer on) must let go of everything, so a
// corrected start works at once and no handle or file descriptor is left behind. The REAL rigControl, no window, Hamlib's dummy rig for
// the good start and a pseudo terminal as the serial port of a real model for the bad one.
//   app
// Built and run by tests/catrestart/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "appglobal.h"
#include "rigcontrol.h"

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

static int openFds()
{
  int n = 0;
  if (DIR *d = opendir("/proc/self/fd")) { while (readdir(d)) n++; closedir(d); }
  return n;
}

static QString entryFor(rigControl &rc, const QString &mfg, const QString &model)
{
  QComboBox cb;
  rc.getRadioList(&cb);
  for (int i = 0; i < cb.count(); i++)
    if (cb.itemText(i).endsWith(" " + mfg + "," + model)) return cb.itemText(i);
  return QString();
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  rigControl *rc = new rigControl(0);
  const QString dummy = entryFor(*rc, "Hamlib", "Dummy");
  const QString icom = entryFor(*rc, "Icom", "IC-7300");
  check(!dummy.isEmpty() && !icom.isEmpty(), "the radio list has the dummy rig and the IC-7300");
  if (dummy.isEmpty() || icom.isEmpty()) return 2;

  // a pseudo terminal stands for the serial port of a radio that does not answer
  int master = posix_openpt(O_RDWR | O_NOCTTY);
  grantpt(master); unlockpt(master);
  const QString pty = QString::fromLocal8Bit(ptsname(master));

  scatParams *cp = rc->params();
  cp->enableCAT = true;
  cp->baudrate = 19200; cp->databits = 8; cp->stopbits = 1; cp->parity = "None"; cp->handshake = "None";
  cp->pttType = RIG_PTT_NONE; cp->enableXMLRPC = false; cp->enableSerialPTT = false;

  cp->radioModel = dummy;
  cp->serialPort = pty;
  check(rc->init() && rc->enabled(), "the dummy rig starts");
  double f = 0;
  check(rc->getFrequency(f), "and answers");
  printf("\n== a radio that does not answer: nothing is left open\n");
  cp->radioModel = icom;
  cp->civAddress = "94";
  // a new terminal for each try: a terminal that was opened once stays exclusive for as long as its master is open
  QStringList ptys;
  for (int i = 0; i < 3; i++)
    {
      const int m = posix_openpt(O_RDWR | O_NOCTTY);
      grantpt(m); unlockpt(m);
      ptys << QString::fromLocal8Bit(ptsname(m));
    }
  const int before = openFds();
  for (int i = 0; i < 3; i++)
    {
      cp->serialPort = ptys.at(i);
      const bool ok = rc->init();
      printf("  (try %d: %s %s)\n", i + 1, ok ? "opened" : "failed", qPrintable(rc->initError));
    }
  const int after = openFds();
  check(after <= before, QString("no descriptor is left behind (%1 before, %2 after)").arg(before).arg(after));
  cp->radioModel = dummy;
  check(rc->init() && rc->enabled(), "the dummy rig works again afterwards");

  printf("\n== a radio that is not in the list (Hamlib itself would end the program)\n");
  cp->radioModel = "99999 Nobody,Nothing";
  check(!rc->init(), "an unknown model fails");
  check(!rc->enabled() && !rc->initError.isEmpty(), "CAT is off and the reason is kept");
  cp->radioModel = dummy;
  check(rc->init() && rc->enabled(), "the corrected start (dummy rig) works at once");
  check(rc->initError.isEmpty(), "the old reason is gone");


  printf("\n== switching CAT off, and quitting\n");
  rc->disable();
  check(!rc->enabled(), "disable() turns it off");
  rc->disable();
  check(!rc->enabled(), "a second disable() does nothing");
  delete rc;   // used to clean up the handle a second time
  check(true, "the destructor after disable() survives");

  rc = new rigControl(0);
  rc->params()->enableCAT = true;
  rc->params()->radioModel = "99999 Nobody,Nothing";
  rc->init();
  delete rc;
  check(true, "the destructor after a failed start survives");

  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);
}
