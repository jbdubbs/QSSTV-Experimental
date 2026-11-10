// Which Android audio devices are offered (src/android/androiddevicetypes.h): a table of what the system lists and what Settings shows.
#include <cstdio>
#include "androiddevicetypes.h"

static int failures = 0;
static void expect(int type, bool input, bool usable, const char *what)
{
  const bool got = androidDeviceUsable(type, input);
  printf("  %s  %s %s (%s)\n", got == usable ? "ok  " : "FAIL", input ? "input " : "output", what, usable ? "offered" : "left out");
  if (got != usable) failures++;
}

int main()
{
  // inputs
  expect(15, true, true, "built-in microphone");
  expect(3, true, true, "wired headset");
  expect(11, true, true, "USB device");
  expect(22, true, true, "USB headset");
  expect(7, true, true, "Bluetooth SCO (kept, narrow)");
  expect(26, true, true, "Bluetooth LE headset");
  expect(18, true, false, "telephony");
  expect(25, true, false, "remote submix");
  expect(16, true, false, "FM tuner");
  expect(17, true, false, "TV tuner");
  expect(20, true, false, "IP");
  expect(21, true, false, "bus");
  expect(28, true, false, "echo reference");
  expect(23, true, false, "hearing aid");
  expect(0, true, false, "unknown");
  expect(2, true, false, "loudspeaker is not an input");
  // outputs
  expect(2, false, true, "loudspeaker");
  expect(24, false, true, "protected loudspeaker");
  expect(1, false, true, "earpiece");
  expect(4, false, true, "wired headphones");
  expect(3, false, true, "wired headset");
  expect(8, false, true, "Bluetooth A2DP");
  expect(11, false, true, "USB device");
  expect(22, false, true, "USB headset");
  expect(7, false, true, "Bluetooth SCO");
  expect(18, false, false, "telephony");
  expect(25, false, false, "remote submix");
  expect(9, false, false, "HDMI");
  expect(21, false, false, "bus");
  expect(15, false, false, "microphone is not an output");
  printf("%s\n", failures ? "FAILED" : "all ok");
  return failures ? 1 : 0;
}
