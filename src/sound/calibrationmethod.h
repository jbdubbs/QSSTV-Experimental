#ifndef CALIBRATIONMETHOD_H
#define CALIBRATIONMETHOD_H

#include <QWidget>
#include <QString>

/*!
  One way of calibrating the sample rate of the soundcard, shown as a tab in the calibration dialog (see
  calibration). A method is a self contained page: it has its own controls, measures against an external
  time reference, and reports the clocks it found.

  To add a method: derive from this class and register it in calibration::init(). The dialog holds up to
  MAXCALIBRATIONMETHODS of them.
*/

#define MAXCALIBRATIONMETHODS 3

class calibrationMethod : public QWidget
{
  Q_OBJECT
public:
  explicit calibrationMethod(QWidget *parent=nullptr) : QWidget(parent) {}
  virtual ~calibrationMethod() {}
  /** name shown on the tab */
  virtual QString title() const=0;
  /** stop measuring and release the soundcard; called when the tab is left and when the dialog closes */
  virtual void stop()=0;
  /** true if there is a result that can be accepted */
  virtual bool hasResult() const=0;
  /** measured receive clock in Hz; 0 if this method did not measure it */
  virtual double rxClockResult() const=0;
  /** measured transmit clock in Hz; 0 if this method did not measure it (the old value is kept) */
  virtual double txClockResult() const=0;

signals:
  /** emitted whenever hasResult() or the results change */
  void resultChanged();
};

#endif // CALIBRATIONMETHOD_H
