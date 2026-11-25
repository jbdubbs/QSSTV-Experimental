// The CM108 GPIO PTT (src/rig/cm108lines.cpp): the report bytes, the chip ids, the Linux hidraw list from a fake /sys tree, the "auto" rule,
// and an open that must fail loudly for something that is not a CM108. No hardware is touched.
//   app
// Built and run by tests/cm108/app.sh (Qt Core and the one source file, no application objects).
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include "cm108lines.h"

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

static QString hex(const QByteArray &b) { return QString::fromLatin1(b.toHex(' ')); }

static void fakeHidraw(const QString &root, int n, const QString &id, const QString &name)
{
  QDir().mkpath(QString("%1/hidraw%2/device").arg(root).arg(n));
  QFile f(QString("%1/hidraw%2/device/uevent").arg(root).arg(n));
  if (!f.open(QIODevice::WriteOnly)) return;
  f.write(QString("DRIVER=hid-generic\nHID_ID=%1\nHID_NAME=%2\nHID_PHYS=usb-0000:00:14.0-1/input3\n").arg(id, name).toUtf8());
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  printf("== the report (Hamlib's cm108_ptt_set)\n");
  check(hex(cm108Ptt::report(3, true)) == "00 00 04 04 00", "GPIO 3 on: 00 00 04 04 00 (the default pin of Hamlib, bit 2)");
  check(hex(cm108Ptt::report(3, false)) == "00 00 00 04 00", "GPIO 3 off: 00 00 00 04 00 (still an output, driven low)");
  check(hex(cm108Ptt::report(1, true)) == "00 00 01 01 00", "GPIO 1 on");
  check(hex(cm108Ptt::report(2, true)) == "00 00 02 02 00", "GPIO 2 on");
  check(hex(cm108Ptt::report(4, true)) == "00 00 08 08 00", "GPIO 4 on");
  check(hex(cm108Ptt::report(9, true)) == "00 00 08 08 00" && hex(cm108Ptt::report(0, true)) == "00 00 01 01 00",
        "a pin out of 1..4 is held to the nearest one, never a shifted-out report");

  printf("\n== the chips\n");
  check(cm108Ptt::compatible(0x0d8c, 0x0013) && cm108Ptt::compatible(0x0d8c, 0x000c) && cm108Ptt::compatible(0x0d8c, 0x0012)
        && cm108Ptt::compatible(0x0d8c, 0x013a) && cm108Ptt::compatible(0x0d8c, 0x013c), "CM108/CM119 ids of C-Media");
  check(cm108Ptt::compatible(0x0c76, 0x1605) && cm108Ptt::compatible(0x0c76, 0x1607) && cm108Ptt::compatible(0x0c76, 0x160b), "SSS1621/23 ids");
  check(!cm108Ptt::compatible(0x0d8c, 0x8810) && !cm108Ptt::compatible(0x0d8c, 0x0007) && !cm108Ptt::compatible(0x046d, 0xc07e)
        && !cm108Ptt::compatible(0x0c76, 0x1606), "a C-Media sound card without the GPIO report, a mouse, a neighbouring id: no");

  printf("\n== the uevent of a hidraw device\n");
  unsigned v = 0, p = 0;
  QString name;
  check(cm108Ptt::parseUevent("DRIVER=hid-generic\nHID_ID=0003:00000D8C:00000013\nHID_NAME=C-Media Electronics Inc.       USB Audio Device\n", v, p, name)
        && v == 0x0d8c && p == 0x0013 && name == "C-Media Electronics Inc. USB Audio Device", "ids and the name, its blanks squeezed");
  check(!cm108Ptt::parseUevent("DRIVER=x\n", v, p, name), "no HID_ID: not a device");

  printf("\n== the list (a fake /sys/class/hidraw)\n");
  QTemporaryDir dir;
  const QString root = dir.path();
  check(cm108Ptt::list(root).isEmpty() && cm108Ptt::list(root + "/missing").isEmpty(), "nothing there: an empty list, no crash");
  fakeHidraw(root, 10, "0003:00000D8C:0000000C", "C-Media Electronics Inc. USB Audio Device");
  fakeHidraw(root, 2, "0003:0000046D:0000C07E", "Logitech Gaming Mouse G402");
  fakeHidraw(root, 3, "0003:00000D8C:00008810", "Speed Dragon USB Advanced Audio Device");
  fakeHidraw(root, 9, "0003:00000C76:00001605", "SSS1621 USB Audio");
  QDir().mkpath(root + "/notahidraw/device");
  const QList<cm108Device> l = cm108Ptt::list(root);
  check(l.count() == 2, QString("only the CM108 family is listed (%1)").arg(l.count()));
  check(l.count() == 2 && l[0].path == "/dev/hidraw9" && l[1].path == "/dev/hidraw10", "in the order of the numbers (9 before 10)");
  check(l.count() == 2 && l[1].label == "C-Media Electronics Inc. USB Audio Device (0d8c:000c) [hidraw10]", "the label says what and where");

  printf("\n== the \"auto\" rule\n");
  check(cm108Ptt::isAuto("") && cm108Ptt::isAuto("auto") && cm108Ptt::isAuto(" Auto ") && !cm108Ptt::isAuto("/dev/hidraw3"), "auto, or nothing chosen");
  QString why;
  check(cm108Ptt::pick({}, why).isEmpty() && why.contains("no CM108"), "none connected: it says so");
  why.clear();
  check(cm108Ptt::pick({l[0]}, why) == "/dev/hidraw9" && why.isEmpty(), "one: that one");
  why.clear();
  check(cm108Ptt::pick(l, why).isEmpty() && why.contains("2 CM108") && why.contains("/dev/hidraw9") && why.contains("/dev/hidraw10"),
        "several: none is picked, the message lists them");

  printf("\n== open\n");
  cm108Ptt dev;
  check(!dev.isOpen(), "closed to begin with");
  why.clear();
  check(!dev.setGpio(3, true, why) && !why.isEmpty(), "keying a device that is not open fails with a reason");
  why.clear();
  check(!dev.open("/dev/qsstv-no-such-device", why) && !why.isEmpty() && !dev.isOpen(), "a path that does not exist: false, with a reason");
#ifdef Q_OS_LINUX
  why.clear();
  check(!dev.open("/dev/null", why) && why.contains("not a CM108") && !dev.isOpen(), "a device that is not a CM108 (/dev/null) is refused");
#endif
  check(!cm108Ptt::advice().isEmpty(), "there is advice for the message");

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
