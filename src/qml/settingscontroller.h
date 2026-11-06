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
  // audio and radio (read only for now)
  Q_PROPERTY(QString audioSummary READ audioSummary NOTIFY audioChanged)

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

  QString audioSummary() const;

  //! the classic Configuration dialog (its pages first take over what was changed here)
  Q_INVOKABLE void openClassic();

signals:
  void operatorChanged();
  void picturesChanged();
  void positionChanged();
  void audioChanged();

private:
  explicit SettingsController(QObject *parent=nullptr);
  void saveOperator();
};

#endif // SETTINGSCONTROLLER_H
