#ifndef CALIBRATIONNTP_H
#define CALIBRATIONNTP_H

#include "calibrationmethod.h"
#include "linefit.h"
#include "soundbase.h"
#include <vector>

class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;
class QTimer;
class sntpClient;
class ntpPlot;

/*!
  Calibrates the sample rate against NTP time servers (UTC, traceable to NIST), over the internet. No radio needed.

  Two independent measurements against the same local clock (the raw monotonic clock, which the system's own
  time synchronisation does not steer, so its crystal error cancels):
   - the soundcard: frames received against local time, giving frames per local second;
   - the servers: server time minus local time against local time; its slope is how fast the local clock runs
     compared to UTC (see sntpClient).
  The true sample rate is the first divided by (1 + the second). Constant delays (network asymmetry, audio
  buffering) only shift the lines and do not change their slopes. The accuracy grows with the measuring time:
  a few ppm after 5 minutes, about 1 ppm after 30.
*/
class calibrationNtp : public calibrationMethod
{
  Q_OBJECT
public:
  explicit calibrationNtp(QWidget *parent=nullptr);
  ~calibrationNtp();
  QString title() const {return tr("NTP time servers");}
  void stop();
  bool hasResult() const;
  double rxClockResult() const;
  double txClockResult() const;

private slots:
  void slotStartStop();
  void slotTimer();
  void slotSample(int server,double localTime,double theta,double delay);
  void slotStatus(const QString &text);

private:
  QLineEdit *serversEdit;
  QPushButton *startButton;
  QLabel *statusLabel;
  QLabel *rateLabel;
  QLabel *ppmLabel;
  QLabel *detailLabel;
  QCheckBox *applyTxCheck;
  ntpPlot *plot;
  QTimer *timer;
  sntpClient *sntp;
  bool running;
  bool hadResult;
  double streamRate;
  double t0;                         // raw clock at the start of the measurement

  // soundcard: the earliest arrival in every AUDIOWINDOW seconds, (local time since t0, frames)
  std::vector<soundBase::listenStamp> stampBuf;
  std::vector<linePoint> audioPts;
  long currentWindow;
  bool haveWindow;
  linePoint windowBest;
  double windowLate;

  // servers: (local time since t0, server minus local time), group = server
  std::vector<linePoint> ntpPts;
  QString lastStatus;

  lineFitResult audioFit,ntpFit;
  double ppm,ppmError;
  bool valid;

  void start();
  void restartMeasurement();
  void solve();
  void updateDisplay();
};

#endif // CALIBRATIONNTP_H
