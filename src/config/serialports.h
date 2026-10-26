#ifndef SERIALPORTS_H
#define SERIALPORTS_H

#include <QList>
#include <QString>

class QComboBox;

//! A serial port to choose from: the name to open and, as a hint, what it is
struct serialPortChoice
{
  QString name;
  QString info;
};

/*!
  The serial ports to offer (GPS, CAT and PTT). On Linux these are the names in /dev/serial/by-id, which stay the same
  when a USB adapter is plugged into another socket (/dev/ttyUSB0 changes); where there are none
  (a computer without USB serial adapters, or without that directory) and elsewhere the ports Qt finds.
*/
QList<serialPortChoice> serialPortList(const QString &byIdDir=QStringLiteral("/dev/serial/by-id"));

//! the same port under another name, for example a by-id link and the /dev/ttyUSB0 it points to
bool samePort(const QString &a,const QString &b);

/*!
  Fill an editable combo box with the ports to offer (the hint as the tooltip of each) and keep the text that is in it:
  a port that is not in the list, or the host of a rig server, stays as it was typed.
*/
void fillPortComboBox(QComboBox *box,const QString &byIdDir=QStringLiteral("/dev/serial/by-id"));

#endif // SERIALPORTS_H
