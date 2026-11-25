#ifndef CM108LINES_H
#define CM108LINES_H

#include <QByteArray>
#include <QList>
#include <QString>

/*!
  A USB sound-card interface with a CM108, CM119 or compatible chip (AllScan, Masters Communications and many DIY boards), keyed through
  one of the GPIO pins of the chip (Configuration > CAT > CM108 GPIO PTT). The pin is set with one HID output report; how it reaches the chip
  depends on the system: a /dev/hidrawN device on Linux, the HID stack of Windows, IOKit on macOS. No library is needed for any of them.
  Hamlib has the same keying, but only on Linux and only through a radio that CAT opened, which is why this is a PTT method of its own.
*/
struct cm108Device
{
  QString path;    //!< what open() takes: /dev/hidrawN on Linux, the HID interface path on Windows, DevSrvsID:N on macOS
  QString label;   //!< what it is, for a list: "C-Media USB Audio Device (0d8c:0013) [hidraw3]"
};

class cm108Ptt
{
public:
  cm108Ptt();
  ~cm108Ptt();
  /*!
    Opens the device. \a device is a path of list(), or "auto" (or empty) for the only CM108 that is connected.
    False with \a why when there is none or several (auto), the path is not a CM108, or it cannot be opened.
  */
  bool open(const QString &device,QString &why);
  bool isOpen() const;
  //! drives GPIO \a gpio (1..4) high or low; false with \a why when the report cannot be sent (the device was unplugged, say)
  bool setGpio(int gpio,bool on,QString &why);
  void close();
  //! what to try when the device cannot be opened (the sentence that follows the reason in the message)
  static QString advice();
  //! the CM108 devices that are connected (\a sysRoot: where Linux lists its hidraw devices, a parameter for the tests)
  static QList<cm108Device> list(const QString &sysRoot=QStringLiteral("/sys/class/hidraw"));
  //! the USB ids of the chips that have the GPIO report (CM108, CM108B, CM109, CM119, CM119A/B and SSS1621/23)
  static bool compatible(unsigned vendor,unsigned product);
  //! "auto", or nothing chosen
  static bool isAuto(const QString &device);
  //! the "auto" rule: the path of the only device of \a devices; else "" with \a why (none, or several to choose from)
  static QString pick(const QList<cm108Device> &devices,QString &why);
  //! the 5 bytes that set GPIO \a gpio (1..4): the report number, then the 4 bytes of the CM108 "write GPIO" report
  static QByteArray report(int gpio,bool on);
  //! the ids and the name of a Linux hidraw device from its uevent text ("HID_ID=0003:00000D8C:0000000C", "HID_NAME=...")
  static bool parseUevent(const QString &text,unsigned &vendor,unsigned &product,QString &name);

  struct impl;   // what the system keeps of the open device (in cm108lines.cpp)

private:
  impl *d;
  bool opened=false;
  cm108Ptt(const cm108Ptt &) = delete;
  cm108Ptt &operator=(const cm108Ptt &) = delete;
};

#endif // CM108LINES_H
