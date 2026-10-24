#ifndef GPSCONFIG_H
#define GPSCONFIG_H
#include "baseconfig.h"

extern bool    gpsEnabled;
extern QString gpsPort;
extern int     gpsBaud;
extern bool    gpsBeaconRefresh;   //!< read the GPS before every WA8LMF beacon transmission
extern bool    gpsPlaceLookup;     //!< look up the city and state of a position on the internet (%g)

namespace Ui {
class gpsConfig;
}

class gpsConfig : public baseConfig
{
  Q_OBJECT

public:
  explicit gpsConfig(QWidget *parent = 0);
  ~gpsConfig();
  void readSettings();
  void writeSettings();
  void getParams();
  void setParams();

  //! A serial port to choose from: the name to open and, as a hint, what it is
  struct portChoice
  {
    QString name;
    QString info;
  };
  /*!
    The serial ports to offer. On Linux these are the names in /dev/serial/by-id, which stay the same
    when a USB adapter is plugged into another socket (/dev/ttyUSB0 changes); where there are none
    (a computer without USB serial adapters, or without that directory) and elsewhere the ports Qt finds.
  */
  static QList<portChoice> serialPorts(const QString &byIdDir=QStringLiteral("/dev/serial/by-id"));
  //! the same port under another name, for example a by-id link and the /dev/ttyUSB0 it points to
  static bool samePort(const QString &a,const QString &b);

private slots:
  void slotRescanPorts();
  void slotEnableChanged();
  void slotTest();

private:
  QString portConflict(const QString &port) const;
  Ui::gpsConfig *ui;
};

#endif // GPSCONFIG_H
