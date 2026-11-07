#ifndef CALIBRATIONCONTROLLER_H
#define CALIBRATIONCONTROLLER_H

#include <QObject>
#include <QImage>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <vector>
#include "linefit.h"
#include "slantfit.h"
#include "soundbase.h"
#include "wwvtickdetector.h"
#include "wwvtickfit.h"

class QQmlEngine;
class QJSEngine;
class sntpClient;

//! one line of the NTP plot: dots (x in s, y in ms), which of them took part in the fit, and the slope of the fit
struct calTrace
{
  std::vector<double> x;
  std::vector<double> y;
  std::vector<bool> in;
  double slope=0;
  bool valid=false;
};

/*!
  Calibration of the sample rate for the Qt Quick pages (Settings > Advanced > Calibration). The measuring is the one of the desktop dialog
  (calibrationNtp, calibrationWwv, calibrationSstv: the same classes do the maths, the same limits are used); this class is the page's
  state and the glue to the sound device. One method at a time uses the sound device.

  A page is opened with open(method), measuring starts with start() and ends with stop(); leave() ends everything and gives the sound device back
  (the receiver is started again when it was running).
*/
class CalibrationController : public QObject
{
  Q_OBJECT
  QML_NAMED_ELEMENT(Calibration)
  QML_SINGLETON

  // what is in use now
  Q_PROPERTY(QString rxClockText READ rxClockText NOTIFY clocksChanged)      //!< "48,000.62 Hz"
  Q_PROPERTY(QString txClockText READ txClockText NOTIFY clocksChanged)
  Q_PROPERTY(QString rxNote READ rxNote NOTIFY clocksChanged)                //!< "not measured", "+12.9 ppm, fast"
  Q_PROPERTY(QString txNote READ txNote NOTIFY clocksChanged)                //!< "same as receive" ...
  Q_PROPERTY(QString infoText READ infoText NOTIFY clocksChanged)            //!< when, how and for which sound device it was measured
  Q_PROPERTY(QString summary READ summary NOTIFY clocksChanged)             //!< for the row in Advanced
  Q_PROPERTY(bool calibrated READ calibrated NOTIFY clocksChanged)           //!< a clock other than 48,000 Hz is in use
  Q_PROPERTY(QString deviceText READ deviceText NOTIFY clocksChanged)        //!< "phone microphone and speaker": the sound device the clock is for

  // a measurement
  Q_PROPERTY(int method READ method NOTIFY measureChanged)                   //!< 0 none, 1 internet time, 2 radio time signal, 3 another SSTV station
  Q_PROPERTY(bool running READ running NOTIFY measureChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY measureChanged)       //!< the line above the number
  Q_PROPERTY(QString rateText READ rateText NOTIFY measureChanged)           //!< the big number, "48,000.62 Hz", or empty
  Q_PROPERTY(QString ppmText READ ppmText NOTIFY measureChanged)             //!< "+12.9 ppm, the sound device runs fast"
  Q_PROPERTY(QString errorText READ errorText NOTIFY measureChanged)         //!< "±3.4", or empty
  Q_PROPERTY(qreal quality READ quality NOTIFY measureChanged)               //!< the bar: how far the measurement is, 0 to 1
  Q_PROPERTY(qreal qualityMark READ qualityMark NOTIFY measureChanged)      //!< where on the bar the result can be saved, 0 to 1
  Q_PROPERTY(QString qualityLeft READ qualityLeft NOTIFY measureChanged)
  Q_PROPERTY(QString qualityRight READ qualityRight NOTIFY measureChanged)
  Q_PROPERTY(QString detailText READ detailText NOTIFY measureChanged)
  Q_PROPERTY(bool canSave READ canSave NOTIFY measureChanged)
  Q_PROPERTY(QString problem READ problem NOTIFY measureChanged)             //!< a sentence with the reason, or empty
  Q_PROPERTY(int problemAction READ problemAction NOTIFY measureChanged)     //!< 0 none, 1 open the app's settings (the microphone is not allowed)
  Q_PROPERTY(int plotRevision READ plotRevision NOTIFY plotChanged)
  Q_PROPERTY(bool applyTx READ applyTx WRITE setApplyTx NOTIFY measureChanged)   //!< the result is the send clock too
  Q_PROPERTY(qreal level READ level NOTIFY levelChanged)                     //!< the input level while listening for ticks, 0 to 1
  Q_PROPERTY(QString servers READ servers WRITE setServers NOTIFY serversChanged)
  Q_PROPERTY(int station READ station WRITE setStation NOTIFY measureChanged)    //!< 0 WWV (1000 Hz ticks), 1 WWVH (1200 Hz)

  // another SSTV station
  Q_PROPERTY(int sstvRole READ sstvRole WRITE setSstvRole NOTIFY sstvChanged)    //!< 0 receive the picture (calibrate this phone), 1 send it
  Q_PROPERTY(QStringList sstvModes READ sstvModes CONSTANT)
  Q_PROPERTY(int sstvModeIndex READ sstvModeIndex WRITE setSstvModeIndex NOTIFY sstvChanged)
  Q_PROPERTY(int sstvPosition READ sstvPosition WRITE setSstvPosition NOTIFY sstvChanged)   //!< percent across the picture
  Q_PROPERTY(bool sstvSending READ sstvSending NOTIFY sstvChanged)
  Q_PROPERTY(qreal sstvProgress READ sstvProgress NOTIFY sstvChanged)            //!< 0 to 1
  Q_PROPERTY(int sstvPictures READ sstvPictures NOTIFY sstvChanged)              //!< pictures that took part in the average

public:
  static CalibrationController *instance();
  static CalibrationController *create(QQmlEngine *,QJSEngine *);

  QString rxClockText() const;
  QString txClockText() const;
  QString rxNote() const;
  QString txNote() const;
  QString infoText() const;
  QString summary() const;
  bool calibrated() const;
  QString deviceText() const;

  int method() const {return m_method;}
  bool running() const {return m_running;}
  QString statusText() const {return m_status;}
  QString rateText() const;
  QString ppmText() const;
  QString errorText() const;
  qreal quality() const;
  qreal qualityMark() const;
  QString qualityLeft() const;
  QString qualityRight() const;
  QString detailText() const {return m_detail;}
  bool canSave() const;
  QString problem() const {return m_problem;}
  int problemAction() const {return m_problemAction;}
  int plotRevision() const {return m_plotRevision;}
  bool applyTx() const {return m_applyTx;}
  void setApplyTx(bool on);
  qreal level() const {return m_level;}
  QString servers() const {return m_servers;}
  void setServers(const QString &s);
  int station() const {return m_station;}
  void setStation(int s);

  int sstvRole() const {return m_sstvRole;}
  void setSstvRole(int r);
  QStringList sstvModes() const;
  int sstvModeIndex() const {return m_sstvMode;}
  void setSstvModeIndex(int i);
  int sstvPosition() const {return m_sstvPosition;}
  void setSstvPosition(int p);
  bool sstvSending() const {return m_sstvSending;}
  qreal sstvProgress() const {return m_sstvProgress;}
  int sstvPictures() const {return int(m_sstvPpms.size());}

  Q_INVOKABLE void open(int method);      //!< a measuring page is shown
  Q_INVOKABLE void start();               //!< Start / Listen
  Q_INVOKABLE void stop();
  Q_INVOKABLE bool save();                //!< the result becomes the receive (and send) clock; false when there is none
  Q_INVOKABLE void reset();               //!< back to 48,000 Hz
  Q_INVOKABLE void leave();               //!< the page is left: stop everything, the receiver resumes
  Q_INVOKABLE void sendReference();       //!< SSTV: send the picture with the straight line
  Q_INVOKABLE QString defaultServers() const;
  //! the screenshots of the test harness: a page in a state (0 ready, 1 early, 2 good, 3 problem); no sound device, no network
  Q_INVOKABLE void setTestState(int method,int phase);

  // the data of the plot (see CalibrationPlot)
  const calTrace &ntpTrace() const {return m_ntpTrace;}
  const calTrace &audioTrace() const {return m_audioTrace;}
  const wwvTickFit *wwvFit() const {return m_fit;}
  const QImage &sstvImage() const {return m_sstvImage;}
  const slantFitResult &sstvFit() const {return m_sstvFitResult;}

signals:
  void clocksChanged();
  void measureChanged();
  void plotChanged();
  void levelChanged();
  void serversChanged();
  void sstvChanged();
  void saved(const QString &text);        //!< a result was stored (for the snackbar)

private slots:
  void slotTimer();
  void slotSample(int server,double localTime,double theta,double delay);
  void slotNtpStatus(const QString &text);
  void slotSstvImageReceived(int mode);
  void slotSstvLine();
  void slotSstvTxFinished();
  void slotSstvTxFailed(const QString &why);
  void slotSstvTxProgress(int percent);

private:
  explicit CalibrationController(QObject *parent=nullptr);
  ~CalibrationController();

  // the common part
  int m_method=0;
  bool m_running=false;
  bool m_hadResult=false;
  bool m_applyTx=true;
  bool m_rxWasListening=false;
  bool m_soundTaken=false;           // the receiver was stopped for a measurement and not yet given back
  bool m_wasDigital=false;
  bool m_modeSwitched=false;
  QString m_status;
  QString m_detail;
  QString m_problem;
  int m_problemAction=0;
  int m_plotRevision=0;
  qreal m_level=0;
  QString m_servers;
  int m_station=0;
  QTimer m_timer;
  double m_streamRate=48000;
  double m_elapsed=0;                // seconds since the internet time measurement began
  double m_detectorRate=0;
  bool m_test=false;                 // a test state is shown: nothing is measured

  void setStatus(const QString &s);
  void setProblem(const QString &text,int action=0);
  void takeSound();                  // the receiver stops: the sound device is for the measurement alone
  void giveSoundBack();
  void stopMeasuring();
  void updateLevel(const double *s,int n);
  bool startSound();                 // false (with a problem set) when the sound device cannot be used
  void clearResult();
  void apply(double rx,double tx,const QString &how);
  static QString fmtHz(double hz);
  static QString fmtPpm(double ppm);
  void changed();

  // internet time
  sntpClient *m_sntp=nullptr;
  double m_t0=0;
  std::vector<soundBase::listenStamp> m_stampBuf;
  std::vector<linePoint> m_audioPts;
  long m_currentWindow=0;
  bool m_haveWindow=false;
  linePoint m_windowBest;
  double m_windowLate=0;
  std::vector<linePoint> m_ntpPts;
  QString m_lastNtpStatus;
  lineFitResult m_audioFit,m_ntpFit;
  double m_ppm=0,m_ppmError=0;
  bool m_valid=false;
  calTrace m_ntpTrace,m_audioTrace;
  void startNtp();
  void ntpTick();
  void ntpSolve();
  bool ntpHasResult() const;
  void ntpDisplay();

  // radio time signal
  wwvTickDetector *m_detector=nullptr;
  wwvTickFit *m_fit=nullptr;
  unsigned int m_lastOverruns=0;
  void startWwv();
  void wwvTick();
  bool wwvHasResult() const;
  void wwvDisplay();

  // another SSTV station
  int m_sstvRole=0;
  int m_sstvMode=1;
  int m_sstvPosition=50;
  bool m_sstvListening=false;
  bool m_sstvSending=false;
  bool m_savedAutoSlant=false;
  qreal m_sstvProgress=0;
  qint64 m_lastLivePaint=0;
  std::vector<double> m_sstvClocks,m_sstvPpms;
  QImage m_sstvImage;
  slantFitResult m_sstvFitResult;
  QString m_sstvLast;
  void startSstvListening();
  void stopSstvListening();
  void sstvDisplay();
  QImage makeReferenceImage(int mode,int percent) const;
  double sstvMeanClock() const;

  double resultRx() const;               // the clock of the method on the page, 0 if there is none
};

#endif // CALIBRATIONCONTROLLER_H
