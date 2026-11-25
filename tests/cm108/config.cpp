// The CM108 GPIO PTT on the CAT page (Configuration > CAT > CM108 GPIO PTT): the settings go to the file and come back, a changed
// setting counts as a change (save-on-change, #149), and it excludes the serial PTT and XML-RPC and they exclude it. The REAL rigConfig
// page and rigControl parameters, offscreen, with a throw-away HOME; no hardware is touched.
//   config
// Built and run by tests/cm108/config.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <unistd.h>
#include <cstdio>
#include "appglobal.h"
#include "rigconfig.h"
#include "rigcontrol.h"

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  rigControl *rc = new rigControl(1);
  rigConfig *cfg = new rigConfig;
  cfg->attachRigController(rc);
  scatParams *cp = rc->params();

  printf("== defaults\n");
  QSettings().clear();
  cfg->readSettings();
  check(!cp->enableCM108PTT && cp->cm108Device == "auto" && cp->cm108Gpio == 3, "off, auto, GPIO 3 when nothing is saved");

  QCheckBox *enable = cfg->findChild<QCheckBox *>("enableCM108CheckBox");
  QComboBox *device = cfg->findChild<QComboBox *>("cm108DeviceComboBox");
  QComboBox *gpio = cfg->findChild<QComboBox *>("cm108GpioComboBox");
  QCheckBox *serial = cfg->findChild<QCheckBox *>("enablePTTCheckBox");
  QCheckBox *xmlrpc = cfg->findChild<QCheckBox *>("enableXMLRPCCheckBox");
  check(enable && device && gpio && serial && xmlrpc, "(the widgets)");
  if (!(enable && device && gpio && serial && xmlrpc)) return 2;
  check(device->itemText(0) == "Auto" && device->currentText() == "Auto", "the device list starts with Auto");
  check(gpio->count() == 4 && gpio->currentText() == "3", "GPIO 1 to 4, 3 chosen");

  printf("\n== the settings go to the file and come back\n");
  device->setCurrentText("/dev/qsstv-no-such-device");
  gpio->setCurrentText("2");
  QTest::mouseClick(enable, Qt::LeftButton);
  check(cp->enableCM108PTT && cp->cm108Device == "/dev/qsstv-no-such-device" && cp->cm108Gpio == 2, "clicking the box takes the settings over");
  cfg->writeSettings();
  cp->enableCM108PTT = false; cp->cm108Device = "x"; cp->cm108Gpio = 4;
  cfg->readSettings();
  check(cp->enableCM108PTT && cp->cm108Device == "/dev/qsstv-no-such-device" && cp->cm108Gpio == 2, "read back from the file");
  check(enable->isChecked() && device->currentText() == "/dev/qsstv-no-such-device" && gpio->currentText() == "2", "and shown");
  device->setCurrentText("  AUTO ");
  cfg->getParams();
  check(cp->cm108Device == "auto", "Auto, however it is typed, is saved as auto");
  cfg->setParams();
  check(device->currentText() == "Auto", "and shown as Auto");

  printf("\n== a change is a change\n");
  cfg->getParams();
  check(!cfg->hasChanged(), "nothing changed");
  gpio->setCurrentText("4");
  cfg->getParams();
  check(cfg->hasChanged(), "another GPIO pin");
  cfg->getParams();
  device->setCurrentText("/dev/hidraw99");
  cfg->getParams();
  check(cfg->hasChanged(), "another device");
  cfg->getParams();
  QTest::mouseClick(enable, Qt::LeftButton);
  check(cfg->hasChanged() && !cp->enableCM108PTT, "switching it off");

  printf("\n== it excludes the serial PTT and XML-RPC, and they exclude it\n");
  QTest::mouseClick(serial, Qt::LeftButton);
  check(serial->isChecked() && !enable->isChecked(), "serial PTT on: CM108 off");
  QTest::mouseClick(enable, Qt::LeftButton);
  check(enable->isChecked() && !serial->isChecked(), "CM108 on: serial PTT off");
  QTest::mouseClick(xmlrpc, Qt::LeftButton);
  check(xmlrpc->isChecked() && !enable->isChecked(), "XML-RPC on: CM108 off");
  QTest::mouseClick(enable, Qt::LeftButton);
  check(enable->isChecked() && !xmlrpc->isChecked(), "CM108 on: XML-RPC off");
  cfg->getParams();
  check(cp->enableCM108PTT && !cp->enableSerialPTT && !cp->enableXMLRPC, "and the parameters follow");

  printf("\n%d passed, %d failed\n", passed, failed);
  _exit(failed ? 1 : 0);
}
