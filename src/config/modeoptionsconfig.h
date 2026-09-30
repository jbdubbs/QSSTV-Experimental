#ifndef MODEOPTIONSCONFIG_H
#define MODEOPTIONSCONFIG_H

#include "baseconfig.h"

namespace Ui
{
class modeOptionsConfig;
}

class modeOptionsConfig : public baseConfig
{
  Q_OBJECT

public:
  explicit modeOptionsConfig(QWidget *parent = 0);
  ~modeOptionsConfig();
  void readSettings();
  void writeSettings();
  void getParams();
  void setParams();

private:
  Ui::modeOptionsConfig *ui;
};

#endif // MODEOPTIONSCONFIG_H
