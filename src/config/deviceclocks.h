#ifndef DEVICECLOCKS_H
#define DEVICECLOCKS_H

#include <QString>

/*!
  The measured sound clock of each sound device (issue #194).

  A sound device counts samples a little fast or slow. The receive clock belongs to the input device and the transmit clock to the
  output device. The globals rxClock and txClock (config/soundconfig.h) stay the clocks in use; this table remembers what was
  measured for each device name, so choosing a device brings its clock back. QSettings arrays DEVICECLOCKS/rx and DEVICECLOCKS/tx
  (device, clock, when, how): an array, because a device name may hold characters a settings key cannot.
*/

//! true if a clock is within the limit (0.2 %) it may differ from BASESAMPLERATE: the limit of the clocks in use and of the stored ones
bool clockOk(double clock);
//! the stored clock of this device in Hz, 0 if none (or one outside the limit a clock may differ from BASESAMPLERATE)
double deviceClock(bool rx,const QString &device);
//! when (ISO date) and how (the calibration method number) it was measured, empty and 0 if not stored
QString deviceClockWhen(bool rx,const QString &device,int *how=nullptr);
//! remembers the clock of this device; clocks outside the limit are not stored
void storeDeviceClock(bool rx,const QString &device,double clock,const QString &when=QString(),int how=0);
//! forgets the clock of this device
void forgetDeviceClock(bool rx,const QString &device);
//! sets rxClock and txClock from the devices now selected (inputAudioDevice, outputAudioDevice): BASESAMPLERATE where none is stored
void loadDeviceClocks();
//! once, when the table is new: the clocks in use belong to the devices now selected
void migrateDeviceClocks();

#endif
