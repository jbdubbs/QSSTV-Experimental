#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <QDialog>
#include "uimode.h"

class QStackedWidget;

namespace Ui {
class configDialog;
}

class configDialog : public QDialog, public backAware
{
  Q_OBJECT

public:
  explicit configDialog(QWidget *parent = 0);
  ~configDialog();
  void readSettings();
  void writeSettings();
  void setSoundClocks(double rx,double tx);
  int exec();
  bool backPressed() override;
  bool soundNeedsRestart;
  bool guiNeedsRestart;


signals:
  void bgColorChanged();


private:
  void setupTouch();   //!< touch shell: the sections as a list, a section on a page of its own
  Ui::configDialog *ui;
  QStackedWidget *levels=nullptr;

};

#endif // CONFIGDIALOG_H
