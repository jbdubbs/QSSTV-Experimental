#ifndef CALIBRATIONWWV_H
#define CALIBRATIONWWV_H

#include "calibrationmethod.h"
#include "wwvtickdetector.h"
#include "wwvtickfit.h"
#include <vector>

class QComboBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QTimer;
class wwvWaterfall;

/*!
  Calibrates the sample rate against the 1 second time ticks of WWV/WWVH (NIST), received on a radio. The
  time ticks are the external reference: the number of samples between ticks is the true sample rate. See
  wwvTickDetector and wwvTickFit for the measurement; this class is the page that drives them from the
  soundcard and shows the tick waterfall (a straight vertical line means the clock is right).
*/
class calibrationWwv : public calibrationMethod
{
  Q_OBJECT
public:
  explicit calibrationWwv(QWidget *parent=nullptr);
  ~calibrationWwv();
  QString title() const {return tr("WWV time ticks");}
  void stop();
  bool hasResult() const;
  double rxClockResult() const;
  double txClockResult() const;

private slots:
  void slotStartStop();
  void slotTimer();
  void slotToneChanged();

private:
  QComboBox *stationCombo;
  QPushButton *startButton;
  QLabel *statusLabel;
  QLabel *rateLabel;
  QLabel *ppmLabel;
  QLabel *detailLabel;
  QCheckBox *applyTxCheck;
  wwvWaterfall *waterfall;
  QTimer *timer;
  bool running;
  double streamRate;
  wwvTickDetector *detector;
  wwvTickFit *fit;
  unsigned int lastOverruns;
  bool hadResult;
  void start();
  void restartMeasurement();
  void updateDisplay();
};

#endif // CALIBRATIONWWV_H
