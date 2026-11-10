// The wait cursor helper on its own (src/utils/busycursor.h, issue #195). Built twice by app.sh:
//   as it is:                     it sets the wait cursor and gives it back, and only what it set itself
//   with -DQSSTV_NO_WAIT_CURSOR:  the macOS behaviour (Qt's wait cursor crashes there): it never touches the cursor
// Needs no application objects, only QtGui (offscreen).
#include <QGuiApplication>
#include <QCursor>
#include <cstdio>
#include "busycursor.h"

static int passed = 0, failed = 0;
static void check(bool ok, const char *what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
  ok ? passed++ : failed++;
}
static bool waiting() { return QGuiApplication::overrideCursor() && QGuiApplication::overrideCursor()->shape() == Qt::WaitCursor; }

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
#ifdef QSSTV_NO_WAIT_CURSOR
  printf("busyCursor without the wait cursor (the macOS behaviour)\n");
  {
    busyCursor b;
    check(QGuiApplication::overrideCursor() == nullptr, "no override cursor is set");
    busyCursor c(true);
    check(QGuiApplication::overrideCursor() == nullptr, "not by a second one either");
    b.release();
    c.release();
  }
  check(QGuiApplication::overrideCursor() == nullptr && busyCursor::timesSet() == 0, "nothing was set, nothing was counted");
#else
  printf("busyCursor\n");
  check(QGuiApplication::overrideCursor() == nullptr, "no override cursor to begin with");
  {
    busyCursor b;
    check(waiting(), "the wait cursor is set while the object lives");
  }
  check(QGuiApplication::overrideCursor() == nullptr, "and given back when it goes");
  check(busyCursor::timesSet() == 1, "it was counted once");

  {
    busyCursor b(false);
    check(QGuiApplication::overrideCursor() == nullptr && busyCursor::timesSet() == 1, "wanted=false sets and counts nothing");
  }

  // somebody else's override cursor must survive: only what it set itself is given back
  QGuiApplication::setOverrideCursor(Qt::CrossCursor);
  {
    busyCursor none(false);
    none.release();
    check(QGuiApplication::overrideCursor() && QGuiApplication::overrideCursor()->shape() == Qt::CrossCursor, "a busyCursor that set nothing leaves the other cursor alone");
    busyCursor b;
    check(waiting(), "a set one is on top of it");
    b.release();
    check(QGuiApplication::overrideCursor() && QGuiApplication::overrideCursor()->shape() == Qt::CrossCursor, "release() brings the other cursor back");
    b.release();
    check(QGuiApplication::overrideCursor() && QGuiApplication::overrideCursor()->shape() == Qt::CrossCursor, "a second release() takes nothing more away");
  }
  check(QGuiApplication::overrideCursor() && QGuiApplication::overrideCursor()->shape() == Qt::CrossCursor, "nor does the destructor after a release()");
  QGuiApplication::restoreOverrideCursor();
  check(QGuiApplication::overrideCursor() == nullptr, "(the other cursor is gone again)");
#endif
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
