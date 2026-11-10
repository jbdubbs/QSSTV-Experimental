#ifndef USBPORTNAME_H
#define USBPORTNAME_H

#include <QString>

// The name of a USB serial port on a phone, as it is kept in the settings and shown in the lists of ports: "usb:VVVV:PPPP:N",
// the vendor and product number of the adapter (hex) and the number of the port in it (a CP2105 has two). No Android headers:
// tests/androiddevices checks the rules on the desktop.

inline bool isUsbPortName(const QString &name)
{
  return name.startsWith("usb:");
}

//! the name of port \a port of the adapter \a vendor:\a product
inline QString usbPortName(int vendor,int product,int port)
{
  return QString("usb:%1:%2:%3").arg(vendor,4,16,QChar('0')).arg(product,4,16,QChar('0')).arg(port);
}

//! the parts of a name made by usbPortName(), false for any other
inline bool parseUsbPortName(const QString &name,int &vendor,int &product,int &port)
{
  const QStringList f=name.split(':');
  if(f.size()!=4 || f[0]!="usb") return false;
  bool a=false,b=false,c=false;
  vendor=f[1].toInt(&a,16);
  product=f[2].toInt(&b,16);
  port=f[3].toInt(&c,10);
  return a && b && c && f[1].size()==4 && f[2].size()==4 && port>=0;
}

#endif // USBPORTNAME_H
