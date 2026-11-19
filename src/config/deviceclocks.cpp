#include "deviceclocks.h"
#include "appdefs.h"
#include "soundconfig.h"
#include <QSettings>
#include <QVector>
#include <math.h>

namespace
{
struct entry
{
  QString device;
  double clock;
  QString when;
  int how;
};

const char *groupName(bool rx) {return rx ? "DEVICECLOCKS/rx" : "DEVICECLOCKS/tx";}

// the same limit soundConfig::readSettings() puts on the clocks in use
bool clockOk(double clock) {return clock>0 && fabs(1-clock/BASESAMPLERATE)<=0.002;}

QVector<entry> readAll(bool rx)
{
  QVector<entry> list;
  QSettings s;
  const int n=s.beginReadArray(groupName(rx));
  for(int i=0;i<n;i++)
    {
      s.setArrayIndex(i);
      entry e;
      e.device=s.value("device").toString();
      e.clock=s.value("clock").toDouble();
      e.when=s.value("when").toString();
      e.how=s.value("how").toInt();
      if(!e.device.isEmpty() && clockOk(e.clock)) list.append(e);
    }
  s.endArray();
  return list;
}

void writeAll(bool rx,const QVector<entry> &list)
{
  QSettings s;
  s.remove(groupName(rx));   // an array that got shorter must not leave its old tail behind
  s.beginWriteArray(groupName(rx),list.size());
  for(int i=0;i<list.size();i++)
    {
      s.setArrayIndex(i);
      s.setValue("device",list[i].device);
      s.setValue("clock",list[i].clock);
      s.setValue("when",list[i].when);
      s.setValue("how",list[i].how);
    }
  s.endArray();
}

int find(const QVector<entry> &list,const QString &device)
{
  for(int i=0;i<list.size();i++) if(list[i].device==device) return i;
  return -1;
}
}

double deviceClock(bool rx,const QString &device)
{
  const QVector<entry> list=readAll(rx);
  const int i=find(list,device);
  return i<0 ? 0.0 : list[i].clock;
}

QString deviceClockWhen(bool rx,const QString &device,int *how)
{
  const QVector<entry> list=readAll(rx);
  const int i=find(list,device);
  if(how) *how=i<0 ? 0 : list[i].how;
  return i<0 ? QString() : list[i].when;
}

void storeDeviceClock(bool rx,const QString &device,double clock,const QString &when,int how)
{
  if(device.isEmpty() || !clockOk(clock)) return;
  // the nominal clock of a device nobody measured is no entry (a clock typed by hand and left at nominal forgets the old one)
  if(when.isEmpty() && fabs(clock-BASESAMPLERATE)<0.0005) {forgetDeviceClock(rx,device); return;}
  QVector<entry> list=readAll(rx);
  const int i=find(list,device);
  entry e{device,clock,when,how};
  if(i>=0)
    {
      // a clock typed by hand or stored again unchanged keeps what is known about its measurement
      if(when.isEmpty() && fabs(list[i].clock-clock)<1e-9) return;
      if(when.isEmpty()) e.how=0;
      list[i]=e;
    }
  else list.append(e);
  writeAll(rx,list);
}

void forgetDeviceClock(bool rx,const QString &device)
{
  QVector<entry> list=readAll(rx);
  const int i=find(list,device);
  if(i<0) return;
  list.removeAt(i);
  writeAll(rx,list);
}

void loadDeviceClocks()
{
  const double rx=deviceClock(true,inputAudioDevice);
  const double tx=deviceClock(false,outputAudioDevice);
  rxClock=rx>0 ? rx : BASESAMPLERATE;
  txClock=tx>0 ? tx : BASESAMPLERATE;
}

void migrateDeviceClocks()
{
  QSettings s;
  if(s.value("DEVICECLOCKS/migrated",false).toBool()) return;
  // the clocks in use were measured for the devices in use
  if(fabs(rxClock-BASESAMPLERATE)>0.0005) storeDeviceClock(true,inputAudioDevice,rxClock,s.value("Calibration/when").toString(),s.value("Calibration/how").toInt());
  if(fabs(txClock-BASESAMPLERATE)>0.0005) storeDeviceClock(false,outputAudioDevice,txClock,s.value("Calibration/when").toString(),s.value("Calibration/how").toInt());
  s.setValue("DEVICECLOCKS/migrated",true);
}
