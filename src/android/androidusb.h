#ifndef ANDROIDUSB_H
#define ANDROIDUSB_H

#include <QList>
#include <QString>

// USB serial adapters on Android (a Digirig, a radio cable): src/android/androidusb.cpp calls
// packaging/android/src/.../UsbSerialHelper.java. Ports are named as in usbportname.h.

struct androidUsbPort
{
  QString name;          // "usb:10c4:ea70:1"
  QString description;   // what the adapter calls itself (and the port, when it has several)
  bool permitted;        // the program may use it already
};

QList<androidUsbPort> androidUsbPorts();

//! The program may use the adapter: when it may not yet the system asks the person (a box over the program), and this waits
//! (the event loop goes on) up to \a seconds for the answer. false with \a why when it is not given.
bool androidUsbAllow(const QString &name,int seconds,QString &why);

//! The TCP port that is the serial port (the address for Hamlib is "127.0.0.1:" and this), or 0 with \a why
//! (stopBits 1 or 2; parity 0 none, 1 odd, 2 even; flow 0 none, 1 hardware, 2 XON/XOFF)
int androidUsbStartBridge(const QString &name,int baud,int dataBits,int stopBits,int parity,int flow,QString &why);

//! Opens the port for the keying lines
bool androidUsbOpenLines(const QString &name,QString &why);

//! RTS and DTR of an open port: -1 leaves a line as it is, 0 off, 1 on
bool androidUsbSetLines(const QString &name,int rts,int dtr);

//! Stops the bridge and closes the port
void androidUsbClose(const QString &name);

#endif // ANDROIDUSB_H
