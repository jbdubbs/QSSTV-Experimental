// The clock per sound device (issue #194): store, load, forget, the limit, the nominal clock, names with / and \ , the migration.
// Built and run by tests/deviceclocks/build.sh (only deviceclocks.cpp is compiled; the globals are the test's own).
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>
#include "deviceclocks.h"
#include "appdefs.h"

double rxClock=BASESAMPLERATE;
double txClock=BASESAMPLERATE;
QString inputAudioDevice;
QString outputAudioDevice;

static int failures=0;
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n",__LINE__,#c); failures++; } }while(0)
static bool near(double a,double b) {return a>b-1e-6 && a<b+1e-6;}

int main(int argc,char **argv)
{
  QCoreApplication app(argc,argv);
  QCoreApplication::setOrganizationName("deviceclockstest");
  QCoreApplication::setApplicationName("deviceclockstest");
  QTemporaryDir dir;
  QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,dir.path());
  QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
  QSettings::setDefaultFormat(QSettings::IniFormat);

  const QString mic="USB audio: Digirig/Mobile \\ 1";   // a slash and a backslash in a name
  const QString spk="Phone speaker";

  // migration: the clocks in use belong to the devices in use, once
  inputAudioDevice=mic; outputAudioDevice=spk;
  rxClock=48001.5; txClock=47999.0;
  { QSettings s; s.setValue("Calibration/when","2026-11-01"); s.setValue("Calibration/how",2); }
  migrateDeviceClocks();
  CHECK(near(deviceClock(true,mic),48001.5));
  CHECK(near(deviceClock(false,spk),47999.0));
  int how=0;
  CHECK(deviceClockWhen(true,mic,&how)=="2026-11-01" && how==2);
  rxClock=48010; migrateDeviceClocks();   // a second time does nothing
  CHECK(near(deviceClock(true,mic),48001.5));

  // another device: its own clock, nominal when never measured
  inputAudioDevice="Phone microphone"; outputAudioDevice=spk;
  loadDeviceClocks();
  CHECK(near(rxClock,BASESAMPLERATE));
  CHECK(near(txClock,47999.0));       // the speaker is the same
  storeDeviceClock(true,inputAudioDevice,48020.25,"2026-11-19",1);
  inputAudioDevice=mic; loadDeviceClocks();
  CHECK(near(rxClock,48001.5));
  inputAudioDevice="Phone microphone"; loadDeviceClocks();
  CHECK(near(rxClock,48020.25));
  CHECK(deviceClockWhen(true,inputAudioDevice,&how)=="2026-11-19" && how==1);

  // the same clock stored again without a date keeps when and how; another one drops them
  storeDeviceClock(true,inputAudioDevice,48020.25);
  CHECK(deviceClockWhen(true,inputAudioDevice,&how)=="2026-11-19" && how==1);
  storeDeviceClock(true,inputAudioDevice,48030.0);
  CHECK(deviceClockWhen(true,inputAudioDevice,&how).isEmpty() && how==0);
  CHECK(near(deviceClock(true,inputAudioDevice),48030.0));

  // the limit, and the nominal clock is no entry
  storeDeviceClock(true,"Bad",49000.0);
  CHECK(deviceClock(true,"Bad")==0.0);
  storeDeviceClock(true,inputAudioDevice,BASESAMPLERATE);
  CHECK(deviceClock(true,inputAudioDevice)==0.0);
  CHECK(near(deviceClock(true,mic),48001.5));   // the others stay

  // forget only the one device; the table shrinks without leaving a tail
  forgetDeviceClock(true,mic);
  CHECK(deviceClock(true,mic)==0.0);
  CHECK(near(deviceClock(false,spk),47999.0));
  { QSettings s; CHECK(s.value("DEVICECLOCKS/rx/size").toInt()==0); }

  printf(failures ? "%d FAILED\n" : "all passed\n",failures);
  return failures?1:0;
}
