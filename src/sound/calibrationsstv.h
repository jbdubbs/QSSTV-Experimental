#ifndef CALIBRATIONSSTV_H
#define CALIBRATIONSSTV_H

#include "calibrationmethod.h"
#include "slantfit.h"
#include "sstvparam.h"
#include <QImage>
#include <vector>

class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QProgressBar;
class slantPreview;

/*!
  Calibrates the sample rate against another SSTV station, for a station that can reach neither WWV nor an NTP
  server. Relative: the result is only as good as the clock of the reference station.

  The reference station sends a picture of a single straight vertical line in a long mode (2 to 3 minutes). The
  station to calibrate receives it with Auto Slant off, so a difference between the two sample rates shows as a
  slant of the line: every line period ends a little earlier or later than the receiver expects. The slope of the
  line, in pixels per transmitted line, divided by the number of pixels in a line period, is the relative rate error.
  The new receive clock is the one for which the line comes out straight.

  The same tab does both jobs: "Send" for the reference station, "Listen" for the station being calibrated.
*/
class calibrationSstv : public calibrationMethod
{
  Q_OBJECT
public:
  explicit calibrationSstv(QWidget *parent=nullptr);
  ~calibrationSstv();
  QString title() const {return tr("Other SSTV station");}
  void stop();
  bool hasResult() const;
  double rxClockResult() const;
  double txClockResult() const;

private slots:
  void slotSendStop();
  void slotListenStop();
  void slotTxFinished();
  void slotImageReceived(int mode);
  void slotLineReceived();
  void slotTxProgress(int percent);

private:
  QComboBox *modeCombo;
  QSpinBox *positionSpin;
  QPushButton *sendButton;
  QPushButton *listenButton;
  QLabel *txStatusLabel;
  QProgressBar *txProgress;
  QLabel *rxStatusLabel;
  QLabel *ppmLabel;
  QLabel *clockLabel;
  QLabel *detailLabel;
  QCheckBox *applyTxCheck;
  slantPreview *preview;
  bool sending;
  bool listening;
  bool savedAutoSlant;
  qint64 lastLivePaint;
  // one entry per accepted picture: the clock that would have made its line straight
  std::vector<double> clocks;
  std::vector<double> ppms;

  QImage makeReferenceImage(esstvMode mode,int percent) const;
  void startListening();
  void stopListening();
  void clearResults();
  void updateDisplay();
};

/** fraction by which the receive clock has to be raised to straighten a line with this slope (negative: lowered) */
double slantToClockError(double slopePerRow,double rowsPerPeriod,double pixelsPerPeriod);

#endif // CALIBRATIONSSTV_H
