#ifndef SETTINGSCONTROLLER_H
#define SETTINGSCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/*!
  What the settings pages of the Qt Quick interface read and write. Each value is the program's own global (the one the classic
  Configuration pages fill) and is saved under the same QSettings key at once: there is no OK and no Cancel. A singleton for QML
  ("SettingsController").
*/
class SettingsController : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  // operator
  Q_PROPERTY(QString callsign READ callsign WRITE setCallsign NOTIFY operatorChanged)
  Q_PROPERTY(QString firstName READ firstName WRITE setFirstName NOTIFY operatorChanged)
  Q_PROPERTY(QString lastName READ lastName WRITE setLastName NOTIFY operatorChanged)
  Q_PROPERTY(QString qth READ qth WRITE setQth NOTIFY operatorChanged)
  Q_PROPERTY(QString operatorSummary READ operatorSummary NOTIFY operatorChanged)   //!< "NT0Y · Jason", or "Not set"
  // pictures
  Q_PROPERTY(QStringList imageFormats READ imageFormats CONSTANT)
  Q_PROPERTY(QString imageFormat READ imageFormat WRITE setImageFormat NOTIFY picturesChanged)
  Q_PROPERTY(bool saveSent READ saveSent WRITE setSaveSent NOTIFY picturesChanged)   //!< keep a copy of what is sent
  Q_PROPERTY(QString picturesSummary READ picturesSummary NOTIFY picturesChanged)
  // position
  Q_PROPERTY(bool gpsOn READ gpsOn WRITE setGpsOn NOTIFY positionChanged)
  Q_PROPERTY(bool placeLookup READ placeLookup WRITE setPlaceLookup NOTIFY positionChanged)   //!< the position is sent to OpenStreetMap to find city and state
  Q_PROPERTY(int coordFormat READ coordFormat WRITE setCoordFormat NOTIFY positionChanged)    //!< 0 decimal degrees, 1 degrees and minutes
  Q_PROPERTY(QString positionSummary READ positionSummary NOTIFY positionChanged)
  Q_PROPERTY(int gpsState READ gpsState NOTIFY positionChanged)                        //!< 0 off, 1 waiting for a position, 2 position known, 3 cannot get one
  Q_PROPERTY(QString gpsReason READ gpsReason NOTIFY positionChanged)                  //!< why not (permission, service off ...)
  Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)            //!< the position now, in the chosen format
  // audio and radio
  Q_PROPERTY(QString audioSummary READ audioSummary NOTIFY audioChanged)
  Q_PROPERTY(QStringList inputDevices READ inputDevices NOTIFY audioChanged)
  Q_PROPERTY(QStringList outputDevices READ outputDevices NOTIFY audioChanged)
  Q_PROPERTY(QString inputDevice READ inputDevice WRITE setInputDevice NOTIFY audioChanged)
  Q_PROPERTY(QString outputDevice READ outputDevice WRITE setOutputDevice NOTIFY audioChanged)
  Q_PROPERTY(int micPreset READ micPreset WRITE setMicPreset NOTIFY audioChanged)   //!< 0 automatic, 1 unprocessed, 2 voice recognition, 3 standard
  Q_PROPERTY(int rxGainDb READ rxGainDb WRITE setRxGainDb NOTIFY audioChanged)       //!< 0 to 30
  Q_PROPERTY(QString routeDetail READ routeDetail NOTIFY audioChanged)                //!< what the sound system really opened
  // identification: CW and the text sent into the waterfall
  Q_PROPERTY(QString cwText READ cwText WRITE setCwText NOTIFY identChanged)
  Q_PROPERTY(int cwTone READ cwTone WRITE setCwTone NOTIFY identChanged)           //!< Hz
  Q_PROPERTY(int cwSpeed READ cwSpeed WRITE setCwSpeed NOTIFY identChanged)        //!< words per minute
  Q_PROPERTY(int wfFontSize READ wfFontSize WRITE setWfFontSize NOTIFY identChanged)
  Q_PROPERTY(bool wfBold READ wfBold WRITE setWfBold NOTIFY identChanged)
  // radio control (CAT and push to talk through a USB adapter)
  Q_PROPERTY(QVariantList adapters READ adapters NOTIFY radioChanged)                 //!< [{name, text}] the serial adapters that are attached
  Q_PROPERTY(QString adapter READ adapter WRITE setAdapter NOTIFY radioChanged)       //!< the name of the one in use, or empty
  Q_PROPERTY(QString adapterText READ adapterText NOTIFY radioChanged)
  Q_PROPERTY(QStringList radioModels READ radioModels CONSTANT)
  Q_PROPERTY(QString radioModel READ radioModel WRITE setRadioModel NOTIFY radioChanged)   //!< a model of the list; empty: no radio control
  Q_PROPERTY(QString radioModelText READ radioModelText NOTIFY radioChanged)
  Q_PROPERTY(int pttMode READ pttMode WRITE setPttMode NOTIFY radioChanged)           //!< 0 none (VOX), 1 RTS line, 2 DTR line, 3 CAT command
  Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY radioChanged)
  Q_PROPERTY(QString radioSummary READ radioSummary NOTIFY radioChanged)
  Q_PROPERTY(bool pttTesting READ pttTesting NOTIFY radioChanged)

public:
  static SettingsController *instance();
  static SettingsController *create(QQmlEngine *,QJSEngine *);

  QString callsign() const;
  void setCallsign(const QString &s);
  QString firstName() const;
  void setFirstName(const QString &s);
  QString lastName() const;
  void setLastName(const QString &s);
  QString qth() const;
  void setQth(const QString &s);
  QString operatorSummary() const;

  QStringList imageFormats() const;
  QString imageFormat() const;
  void setImageFormat(const QString &f);
  bool saveSent() const;
  void setSaveSent(bool on);
  QString picturesSummary() const;

  bool gpsOn() const;
  void setGpsOn(bool on);
  bool placeLookup() const;
  void setPlaceLookup(bool on);
  int coordFormat() const;
  void setCoordFormat(int f);
  QString positionSummary() const;
  int gpsState() const;
  QString gpsReason() const {return m_gpsReason;}
  QString positionText() const;

  QString audioSummary() const;
  QStringList inputDevices() const;
  QStringList outputDevices() const;
  QString inputDevice() const;
  void setInputDevice(const QString &d);
  QString outputDevice() const;
  void setOutputDevice(const QString &d);
  int micPreset() const;
  void setMicPreset(int p);
  int rxGainDb() const;
  void setRxGainDb(int db);
  QString routeDetail() const;

  QString cwText() const;
  void setCwText(const QString &t);
  int cwTone() const;
  void setCwTone(int hz);
  int cwSpeed() const;
  void setCwSpeed(int wpm);
  int wfFontSize() const;
  void setWfFontSize(int size);
  bool wfBold() const;
  void setWfBold(bool on);
  //! the text of one marker sent in the waterfall: startPic, endPic, fix, bsr, startBin, endBin, startRepeater, endRepeater
  Q_INVOKABLE QString waterfallText(const QString &key) const;
  Q_INVOKABLE void setWaterfallText(const QString &key,const QString &text);

  QVariantList adapters() const;
  QString adapter() const;
  void setAdapter(const QString &name);
  QString adapterText() const;
  QStringList radioModels() const;
  QString radioModel() const;
  void setRadioModel(const QString &model);
  QString radioModelText() const;
  int pttMode() const;
  void setPttMode(int mode);
  int baudRate() const;
  void setBaudRate(int baud);
  QString radioSummary() const;
  bool pttTesting() const {return m_pttTesting;}
  Q_INVOKABLE void testPtt();         //!< keys the transmitter for a moment
  Q_INVOKABLE void rescanAdapters();

  //! the classic Configuration dialog (its pages first take over what was changed here)
  Q_INVOKABLE void openClassic();

signals:
  void operatorChanged();
  void picturesChanged();
  void positionChanged();
  void audioChanged();
  void identChanged();
  void radioChanged();

private:
  explicit SettingsController(QObject *parent=nullptr);
  void saveOperator();
  void saveSound(const char *key,const QVariant &value,bool restart);
  void saveRadio();                 //!< writes the rig parameters and starts the rig controller again
  bool m_pttTesting=false;
  int m_trackState=-1;           //!< gpsReader::trackState, or -1 before the first report
  QString m_gpsReason;
  bool m_haveFix=false;
  double m_lat=0,m_lon=0;
};

#endif // SETTINGSCONTROLLER_H
