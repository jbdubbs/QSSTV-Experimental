#include "settingscontroller.h"
#include "appglobal.h"
#include "appmodel.h"
#include "configdialog.h"
#include "configparams.h"
#include "directoriesconfig.h"
#include "gpsconfig.h"
#include "guiconfig.h"
#include "nmeaparser.h"
#include "operatorconfig.h"
#include "rxwidget.h"
#include "txcontroller.h"
#include "txwidget.h"
#include <QComboBox>
#include <QJSEngine>
#include <QQmlEngine>
#include <QSettings>

static SettingsController *theSettings=nullptr;

SettingsController::SettingsController(QObject *parent):QObject(parent) {}

SettingsController *SettingsController::instance()
{
  if(!theSettings) theSettings=new SettingsController;
  return theSettings;
}

SettingsController *SettingsController::create(QQmlEngine *,QJSEngine *)
{
  QJSEngine::setObjectOwnership(instance(),QJSEngine::CppOwnership);
  return instance();
}

// the globals hold "NOCALL" and the like until the operator fills them in: the page shows those as empty fields
static QString shown(const QString &s,const char *placeholder) {return s==QLatin1String(placeholder) ? QString() : s;}

QString SettingsController::callsign() const {return shown(myCallsign,"NOCALL");}
QString SettingsController::firstName() const {return shown(myFirstname,"NOFIRSTNAME");}
QString SettingsController::lastName() const {return shown(myLastname,"NONAME");}
QString SettingsController::qth() const {return shown(myQth,"NOWHERE");}

void SettingsController::saveOperator()
{
  QSettings qSettings;
  qSettings.beginGroup("PERSONAL");
  qSettings.setValue("callsign",myCallsign);
  qSettings.setValue("qth",myQth);
  qSettings.setValue("lastname",myLastname);
  qSettings.setValue("firstname",myFirstname);
  qSettings.endGroup();
  emit operatorChanged();
  emit TxController::instance()->detailsChanged();   // the Transmit screen shows the call sign in its details line
}

void SettingsController::setCallsign(const QString &s)
{
  const QString v=s.trimmed().toUpper();
  myCallsign=v.isEmpty() ? QStringLiteral("NOCALL") : v;
  saveOperator();
}

void SettingsController::setFirstName(const QString &s)
{
  const QString v=s.trimmed();
  myFirstname=v.isEmpty() ? QStringLiteral("NOFIRSTNAME") : v;
  saveOperator();
}

void SettingsController::setLastName(const QString &s)
{
  const QString v=s.trimmed();
  myLastname=v.isEmpty() ? QStringLiteral("NONAME") : v;
  saveOperator();
}

void SettingsController::setQth(const QString &s)
{
  const QString v=s.trimmed();
  myQth=v.isEmpty() ? QStringLiteral("NOWHERE") : v;
  saveOperator();
}

QString SettingsController::operatorSummary() const
{
  QStringList parts;
  if(!callsign().isEmpty()) parts<<callsign();
  if(!firstName().isEmpty()) parts<<firstName();
  return parts.isEmpty() ? tr("Not set") : parts.join(QStringLiteral(" · "));
}

// ---- pictures ----

QStringList SettingsController::imageFormats() const
{
  QStringList l;
  if(rxWidgetPtr)
    if(const QComboBox *c=rxWidgetPtr->findChild<QComboBox *>("defaultImageFormatComboBox"))
      for(int i=0;i<c->count();i++) l<<c->itemText(i);
  if(l.contains("jpg")) l.removeAll("jpeg");   // the same format under two names
  return l;
}

QString SettingsController::imageFormat() const {return ::defaultImageFormat;}

void SettingsController::setImageFormat(const QString &f)
{
  if(rxWidgetPtr)
    if(QComboBox *c=rxWidgetPtr->findChild<QComboBox *>("defaultImageFormatComboBox"))
      c->setCurrentText(f);   // the receive page saves it (slotGetParams)
  emit picturesChanged();
}

bool SettingsController::saveSent() const {return ::saveTXimages;}

void SettingsController::setSaveSent(bool on)
{
  ::saveTXimages=on;
  QSettings qSettings;
  qSettings.beginGroup("DIRECTORIES");
  qSettings.setValue("saveTXimages",on);
  qSettings.endGroup();
  emit picturesChanged();
}

QString SettingsController::picturesSummary() const
{
  return tr("Saved as %1").arg(::defaultImageFormat.toUpper());
}

// ---- position ----

bool SettingsController::gpsOn() const {return ::gpsEnabled;}

void SettingsController::setGpsOn(bool on)
{
  ::gpsEnabled=on;
  QSettings qSettings;
  qSettings.beginGroup("GPS");
  qSettings.setValue("enabled",on);
  qSettings.endGroup();
  if(txWidgetPtr) txWidgetPtr->gpsSettingsChanged();   // starts or stops the reader (the system asks for the permission)
  emit positionChanged();
}

bool SettingsController::placeLookup() const {return ::gpsPlaceLookup;}

void SettingsController::setPlaceLookup(bool on)
{
  ::gpsPlaceLookup=on;
  QSettings qSettings;
  qSettings.beginGroup("GPS");
  qSettings.setValue("placeLookup",on);
  qSettings.endGroup();
  emit positionChanged();
}

int SettingsController::coordFormat() const {return ::gpsCoordFormat==COORDDEGMIN ? 1 : 0;}

void SettingsController::setCoordFormat(int f)
{
  ::gpsCoordFormat=(f==1) ? COORDDEGMIN : COORDDECIMAL;
  QSettings qSettings;
  qSettings.beginGroup("GPS");
  qSettings.setValue("coordinateFormat",::gpsCoordFormat);
  qSettings.endGroup();
  if(txWidgetPtr) txWidgetPtr->positionFormatChanged();
  emit positionChanged();
}

QString SettingsController::positionSummary() const
{
  if(!::gpsEnabled) return tr("GPS off");
  return ::gpsPlaceLookup ? tr("GPS on · city and state looked up") : tr("GPS on · from the phone");
}

// ---- audio and radio ----

QString SettingsController::audioSummary() const {return AppModel::instance()->routeText();}

void SettingsController::openClassic()
{
  AppModel::instance()->openClassicSettings();
}
