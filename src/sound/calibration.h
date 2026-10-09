#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <QDialog>
#include <QList>

class calibrationMethod;

namespace Ui {
  class calibration;
  }

/*!
  Options > Calibrate. A tabbed dialog with one tab per calibration method (see calibrationMethod, at most
  MAXCALIBRATIONMETHODS). Each method measures the sample rate of the soundcard against an external time
  reference. OK, or the Save button of a tab, takes the clocks of the method on the active tab.
*/
class calibration : public QDialog
{
  Q_OBJECT

public:
  explicit calibration(QWidget *parent = 0);
  ~calibration();
  /** receive clock chosen by the accepted method in Hz, 0 if it did not measure it */
  double getRXClock() {return rxCardClock;}
  /** transmit clock chosen by the accepted method in Hz, 0 if it did not measure it */
  double getTXClock() {return txCardClock;}

public slots:
  void accept();
  void reject();

signals:
  /** a method's Save button was pressed and the dialog stays open: apply these clocks now (0 = not measured) */
  void clocksSaved(double rx,double tx);

private slots:
  void slotSaveRequested();
  void slotTabChanged(int index);
  void slotResultChanged();

private:
  Ui::calibration *ui;
  QList<calibrationMethod *> methods;
  double rxCardClock;
  double txCardClock;
  void init();
  void addMethod(calibrationMethod *m);
  void stopAll();
  calibrationMethod *activeMethod();
};

#endif // CALIBRATION_H
