#include "settingscontroller.h"
#include "appglobal.h"
#include "appmodel.h"
#include "configdialog.h"
#include "configparams.h"
#include "directoriesconfig.h"
#include "gpsconfig.h"
#include "mainwindow.h"
#include "soundbase.h"
#include "soundconfig.h"
#include "soundqtmultimedia.h"
#include "guiconfig.h"
#include "nmeaparser.h"
#include "operatorconfig.h"
#include "rigcontrol.h"
#include "serialports.h"
#ifdef Q_OS_ANDROID
#include "androidusb.h"
#endif
#include "rxwidget.h"
#include "txcontroller.h"
#include "txwidget.h"
#include <QComboBox>
#include <QVariantMap>
#include <QJSEngine>
#include <QQmlEngine>
#include <QSettings>
#include <QTimer>

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

QStringList SettingsController::inputDevices() const
{
  QStringList in,out;
  getCardList(in,out);
  return in;
}

QStringList SettingsController::outputDevices() const
{
  QStringList in,out;
  getCardList(in,out);
  return out;
}

QString SettingsController::inputDevice() const {return ::inputAudioDevice;}
QString SettingsController::outputDevice() const {return ::outputAudioDevice;}
int SettingsController::micPreset() const {return ::micPreset;}
int SettingsController::rxGainDb() const {return int(::rxGainDb+0.5);}
QString SettingsController::routeDetail() const {return soundIOPtr ? soundIOPtr->routeSummary() : QString();}

// the same keys the sound page writes; a changed device or microphone setting reopens the sound device (as OK in the classic dialog does)
void SettingsController::saveSound(const char *key,const QVariant &value,bool restart)
{
  QSettings qSettings;
  qSettings.beginGroup("SOUND");
  qSettings.setValue(key,value);
  qSettings.endGroup();
  if(restart && mainWindowPtr)
    QTimer::singleShot(0,mainWindowPtr,[]{mainWindowPtr->soundSettingsChanged();   // outside the tap that asked for it
                                           emit SettingsController::instance()->audioChanged();});
  emit audioChanged();
}

void SettingsController::setInputDevice(const QString &d)
{
  if(d==::inputAudioDevice) return;
  ::inputAudioDevice=d;
  saveSound("inputAudioDevice",d,true);
}

void SettingsController::setOutputDevice(const QString &d)
{
  if(d==::outputAudioDevice) return;
  ::outputAudioDevice=d;
  saveSound("outputAudioDevice",d,true);
}

void SettingsController::setMicPreset(int p)
{
  if(p<micPresetAuto || p>micPresetStandard || p==::micPreset) return;
  ::micPreset=p;
  saveSound("micPreset",p,true);   // the preset is chosen when the stream is opened
}

void SettingsController::setRxGainDb(int db)
{
  ::rxGainDb=qBound(0,db,30);
  if(soundIOPtr) soundIOPtr->setRxGainDb(::rxGainDb);   // no reopening needed
  saveSound("rxGainDb",::rxGainDb,false);
}

void SettingsController::openClassic()
{
  AppModel::instance()->openClassicSettings();
}

// ---- radio control ----

QVariantList SettingsController::adapters() const
{
  QVariantList l;
#ifdef Q_OS_ANDROID
  for(const androidUsbPort &p:androidUsbPorts())
    {
      QVariantMap m;
      m["name"]=p.name;
      m["text"]=p.description.isEmpty() ? p.name : p.description;
      l.append(m);
    }
#else
  for(const serialPortChoice &p:serialPortList())
    {
      QVariantMap m;
      m["name"]=p.name;
      m["text"]=p.info.isEmpty() ? p.name : p.info;
      l.append(m);
    }
#endif
  return l;
}

QString SettingsController::adapter() const
{
  return rigControllerPtr ? rigControllerPtr->params()->serialPort : QString();
}

QString SettingsController::adapterText() const
{
  const QString a=adapter();
  for(const QVariant &v:adapters())
    if(v.toMap().value("name").toString()==a) return v.toMap().value("text").toString();
  return a.isEmpty() ? QString() : a;
}

void SettingsController::setAdapter(const QString &name)
{
  if(!rigControllerPtr) return;
  scatParams *cp=rigControllerPtr->params();
  cp->serialPort=name;
  cp->pttSerialPort=name;   // the same adapter carries the radio commands and the key line
  saveRadio();
}

QStringList SettingsController::radioModels() const
{
  static QStringList list;
  if(list.isEmpty() && rigControllerPtr)
    {
      QComboBox tmp;
      rigControllerPtr->getRadioList(&tmp);
      for(int i=0;i<tmp.count();i++) list<<tmp.itemText(i);
    }
  return list;
}

QString SettingsController::radioModel() const
{
  if(!rigControllerPtr) return QString();
  const scatParams *cp=rigControllerPtr->params();
  return cp->enableCAT ? cp->radioModel : QString();
}

QString SettingsController::radioModelText() const
{
  const QString m=radioModel().trimmed();
  return m;
}

void SettingsController::setRadioModel(const QString &model)
{
  if(!rigControllerPtr) return;
  scatParams *cp=rigControllerPtr->params();
  if(model.isEmpty())
    {
      cp->enableCAT=false;
    }
  else
    {
      cp->radioModel=model;
      cp->enableCAT=true;
      cp->enableSerialPTT=false;   // the key line of a CAT adapter is driven through the CAT port
      if(cp->pttType==RIG_PTT_NONE && cp->activeRTS) cp->pttType=RIG_PTT_SERIAL_RTS;
    }
  saveRadio();
}

int SettingsController::pttMode() const
{
  if(!rigControllerPtr) return 0;
  const scatParams *cp=rigControllerPtr->params();
  if(cp->enableSerialPTT) return cp->activeRTS ? 1 : cp->activeDTR ? 2 : 0;
  switch(cp->pttType)
    {
    case RIG_PTT_SERIAL_RTS: return 1;
    case RIG_PTT_SERIAL_DTR: return 2;
    case RIG_PTT_RIG:
    case RIG_PTT_RIG_MICDATA: return cp->enableCAT ? 3 : 0;   // a CAT command needs a radio to send it to
    default: return 0;
    }
}

void SettingsController::setPttMode(int mode)
{
  if(!rigControllerPtr) return;
  scatParams *cp=rigControllerPtr->params();
  cp->enableSerialPTT=false;
  cp->activeRTS=cp->activeDTR=cp->nactiveRTS=cp->nactiveDTR=false;
  cp->pttType=RIG_PTT_NONE;
  if(mode==1 || mode==2)
    {
      if(cp->enableCAT) cp->pttType=(mode==1) ? RIG_PTT_SERIAL_RTS : RIG_PTT_SERIAL_DTR;   // through the CAT adapter
      else
        {
          cp->enableSerialPTT=true;   // no radio control: the adapter only keys the transmitter
          cp->pttSerialPort=cp->serialPort;
          (mode==1 ? cp->activeRTS : cp->activeDTR)=true;
        }
    }
  else if(mode==3) cp->pttType=RIG_PTT_RIG;
  saveRadio();
}

int SettingsController::baudRate() const {return rigControllerPtr ? rigControllerPtr->params()->baudrate : 9600;}

void SettingsController::setBaudRate(int baud)
{
  if(!rigControllerPtr) return;
  rigControllerPtr->params()->baudrate=baud;
  saveRadio();
}

QString SettingsController::radioSummary() const
{
  if(!rigControllerPtr) return tr("Not available");
  const QString model=radioModel().trimmed();
  QStringList parts;
  parts<<(model.isEmpty() ? tr("No radio control") : model);
  switch(pttMode())
    {
    case 1: parts<<tr("RTS line keys the radio"); break;
    case 2: parts<<tr("DTR line keys the radio"); break;
    case 3: parts<<tr("CAT keys the radio"); break;
    default: parts<<tr("VOX or manual"); break;
    }
  return parts.join(QStringLiteral(" · "));
}

// the same keys the CAT page writes, then the rig controller starts again with them
void SettingsController::saveRadio()
{
  scatParams *cp=rigControllerPtr->params();
  QSettings qSettings;
  qSettings.beginGroup(cp->configLabel);
  qSettings.setValue("serialPort",cp->serialPort);
  qSettings.setValue("radioModel",cp->radioModel);
  qSettings.setValue("civAddress",cp->civAddress);
  qSettings.setValue("baudrate",cp->baudrate);
  qSettings.setValue("parity",cp->parity);
  qSettings.setValue("stopbits",cp->stopbits);
  qSettings.setValue("databits",cp->databits);
  qSettings.setValue("handshake",cp->handshake);
  qSettings.setValue("enableCAT",cp->enableCAT);
  qSettings.setValue("enableSerialPTT",cp->enableSerialPTT);
  qSettings.setValue("pttSerialPort",cp->pttSerialPort);
  qSettings.setValue("activeRTS",cp->activeRTS);
  qSettings.setValue("activeDTR",cp->activeDTR);
  qSettings.setValue("nactiveRTS",cp->nactiveRTS);
  qSettings.setValue("nactiveDTR",cp->nactiveDTR);
  qSettings.setValue("pttType",int(cp->pttType));
  qSettings.endGroup();
  emit radioChanged();
  QTimer::singleShot(0,this,[]   // outside the tap that asked for it: opening an adapter may ask the person first
    {
      if(!rigControllerPtr) return;
      rigControllerPtr->disable();
      if(rigControllerPtr->params()->enableCAT) rigControllerPtr->init();
    });
}

void SettingsController::rescanAdapters() {emit radioChanged();}

void SettingsController::testPtt()
{
  if(!rigControllerPtr || m_pttTesting) return;
  m_pttTesting=true;
  emit radioChanged();
  rigControllerPtr->activatePTT(true);
  QTimer::singleShot(1200,this,[this]
    {
      if(rigControllerPtr) rigControllerPtr->activatePTT(false);
      m_pttTesting=false;
      emit radioChanged();
    });
}
