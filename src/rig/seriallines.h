#ifndef SERIALLINES_H
#define SERIALLINES_H

#include <QString>

/*!
  The RTS and DTR lines of a serial port, for keying a transmitter with them (Configuration > CAT > Special Serial Port).
  A port of the system (QSerialPort) on a desktop; on a phone a USB adapter ("usb:..." names, src/android/usbportname.h),
  which has no device node to open.
*/
class serialLines
{
public:
  virtual ~serialLines() = default;
  //! opens the port; false with \a why
  virtual bool open(QString &why) = 0;
  virtual void setDtr(bool on) = 0;
  virtual void setRts(bool on) = 0;
  //! what to try when the port cannot be opened (the sentence that follows the reason in the message)
  virtual QString advice() const = 0;
  //! the right kind for \a portName
  static serialLines *create(const QString &portName);
};

#endif // SERIALLINES_H
