// A sample rate typed in the clock fields of Configuration > Sound is put back (issue #214): the real soundConfig widget, offscreen.
// Built and run by tests/soundclocks/run.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include "appdefs.h"
#include "soundconfig.h"
#include "deviceclocks.h"

static int passed=0,failed=0;
static void check(bool ok,const QString &what)
{
  printf("  %s %s\n",ok?"ok  ":"FAIL",qPrintable(what));
  ok?passed++:failed++;
}

static int boxes=0;
// closes the message box checkClocks() opens (it would wait for a click)
static void closeBox()
{
  if(QWidget *w=QApplication::activeModalWidget()) {boxes++; w->close();}
  else QTimer::singleShot(20,closeBox);
}

int main(int argc,char **argv)
{
  qputenv("QT_QPA_PLATFORM","offscreen");
  QApplication app(argc,argv);
  QCoreApplication::setOrganizationName("clocktest");
  QCoreApplication::setApplicationName("clocktest");
  soundConfig w;
  w.readSettings();
  QLineEdit *in=w.findChild<QLineEdit*>("inputClockLineEdit");
  QLineEdit *out=w.findChild<QLineEdit*>("outputClockLineEdit");
  check(in && out,"the clock fields exist");
  if(!in || !out) return 1;

  in->setText("48000.5"); out->setText("47990");
  check(w.checkClocks() && boxes==0,"clocks within 0.2 % are accepted without a message");

  in->setText("44100");
  QTimer::singleShot(20,closeBox);
  check(!w.checkClocks(),"44100 in the input clock is refused");
  check(boxes==1,"the user is told once");
  check(fabs(in->text().toDouble()-BASESAMPLERATE)<0.5,"the input field is back at 48000 ("+in->text()+")");
  check(fabs(out->text().toDouble()-47990)<0.5,"the output field, which was fine, is left alone ("+out->text()+")");

  out->setText("96000");
  QTimer::singleShot(20,closeBox);
  check(!w.checkClocks(),"96000 in the output clock is refused");
  check(fabs(out->text().toDouble()-BASESAMPLERATE)<0.5,"the output field is back at 48000 ("+out->text()+")");

  // the save at quit has no message box: getParams() alone must never leave a bad clock in use
  in->setText("44100"); out->setText("44100");
  w.getParams();
  check(clockOk(rxClock) && clockOk(txClock),"getParams() puts back a bad clock silently");
  check(clockOk(48000) && !clockOk(44100) && !clockOk(0) && !clockOk(-48000),"clockOk limits");
  printf("%d passed, %d failed\n",passed,failed);
  return failed?1:0;
}
