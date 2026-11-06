// The names of USB serial ports on Android (src/android/usbportname.h): made, read back, and not mistaken for anything else.
#include <QCoreApplication>
#include <cstdio>
#include "usbportname.h"

static int failures = 0;
static void check(bool ok, const char *what) {printf("  %s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) failures++;}

int main()
{
  const QString n = usbPortName(0x10c4, 0xea70, 1);
  check(n == "usb:10c4:ea70:1", "the name of the second port of a CP2105 (Digirig)");
  int v = 0, p = 0, port = -1;
  check(parseUsbPortName(n, v, p, port) && v == 0x10c4 && p == 0xea70 && port == 1, "it reads back");
  check(usbPortName(0x403, 0x6001, 0) == "usb:0403:6001:0", "numbers are padded to four digits");
  check(isUsbPortName("usb:0403:6001:0") && !isUsbPortName("/dev/ttyUSB0") && !isUsbPortName("COM3") && !isUsbPortName(""), "only these are USB names");
  check(!parseUsbPortName("usb:0403:6001", v, p, port), "a name without the port is not one");
  check(!parseUsbPortName("usb:04:6001:0", v, p, port), "nor one with a short number");
  check(!parseUsbPortName("usb:zzzz:6001:0", v, p, port), "nor one that is not hexadecimal");
  check(!parseUsbPortName("usb:0403:6001:-1", v, p, port), "nor a negative port");
  check(!parseUsbPortName("/dev/ttyUSB0", v, p, port), "nor a path");
  printf("%s\n", failures ? "FAILED" : "all ok");
  return failures ? 1 : 0;
}
