#ifndef GPSCONFIG_H
#define GPSCONFIG_H
#include "baseconfig.h"

extern bool    gpsEnabled;
extern QString gpsPort;
extern int     gpsBaud;
extern bool    gpsBeaconRefresh;   //!< read the GPS before every WA8LMF beacon transmission

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

private slots:
  void slotRescanPorts();
  void slotEnableChanged();
  void slotTest();

private:
  QString portConflict(const QString &port) const;
  Ui::gpsConfig *ui;
};

#endif // GPSCONFIG_H
