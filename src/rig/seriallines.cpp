#include "seriallines.h"

#include <QSerialPort>

#ifdef Q_OS_ANDROID
#include "androidusb.h"
#include "usbportname.h"
#endif

namespace
{

class portLines : public serialLines
{
public:
  explicit portLines(const QString &name) : port(name) {}
  bool open(QString &why) override
  {
    if(port.open(QIODevice::ReadWrite)) return true;
    why=port.errorString();
    return false;
  }
  void setDtr(bool on) override {port.setDataTerminalReady(on);}
  void setRts(bool on) override {port.setRequestToSend(on);}
  QString advice() const override
  {
    return "Check that you have read and write permission on it, or switch Serial PTT off if you have no such port.";
  }
private:
  QSerialPort port;   // closes the port too, if still open
};

#ifdef Q_OS_ANDROID
class usbLines : public serialLines
{
public:
  explicit usbLines(const QString &name) : portName(name) {}
  ~usbLines() override {if(opened) androidUsbClose(portName);}
  bool open(QString &why) override
  {
    if(!androidUsbAllow(portName,30,why)) return false;
    opened=androidUsbOpenLines(portName,why);
    return opened;
  }
  void setDtr(bool on) override {androidUsbSetLines(portName,-1,on ? 1 : 0);}
  void setRts(bool on) override {androidUsbSetLines(portName,on ? 1 : 0,-1);}
  QString advice() const override
  {
    return "Check that the adapter is plugged in and that the program may use it (the system asks), or switch Serial PTT off.";
  }
private:
  QString portName;
  bool opened=false;
};
#endif

}

serialLines *serialLines::create(const QString &portName)
{
#ifdef Q_OS_ANDROID
  if(isUsbPortName(portName)) return new usbLines(portName);
#endif
  return new portLines(portName);
}
