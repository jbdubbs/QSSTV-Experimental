#include "frequencycontroller.h"
#include "appglobal.h"
#include "appmodel.h"
#include "configdialog.h"
#include "frequencyselectwidget.h"
#include "hexconvertor.h"
#include "mainwindow.h"
#include "rigcontrol.h"

#include <QJSEngine>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>
#include <cmath>

namespace
{
const QStringList &modTable() {static const QStringList l={"LSB","USB","FM","AM","PKTLSB","PKTUSB","PKTFM"}; return l;}
const QStringList &passTable() {static const QStringList l={"Normal","Wide","Narrow"}; return l;}
}

FrequencyController *FrequencyController::instance()
{
  static FrequencyController *c=new FrequencyController;
  return c;
}

FrequencyController *FrequencyController::create(QQmlEngine *,QJSEngine *)
{
  FrequencyController *c=instance();
  QJSEngine::setObjectOwnership(c,QJSEngine::CppOwnership);
  return c;
}

FrequencyController::FrequencyController(QObject *parent) : QObject(parent)
{
  m_timer.setInterval(2000);
  connect(&m_timer,&QTimer::timeout,this,&FrequencyController::poll);
  m_timer.start();
  connect(AppModel::instance(),&AppModel::digitalChanged,this,&FrequencyController::poll);   // the chip names the mode of the list it is in
}

QStringList FrequencyController::modulations() const {return modTable();}
QStringList FrequencyController::passbands() const {return passTable();}

int FrequencyController::count() const {return int(::freqList.count());}

QString FrequencyController::describe(int i)
{
  const QString pass=::passBandList.value(i);
  return QObject::tr("%1, %2 passband").arg(::sbModeList.value(i),pass.toLower());
}

QVariantList FrequencyController::entries(bool drm) const
{
  QVariantList l;
  const QString mode=drm ? QStringLiteral("DRM") : QStringLiteral("SSTV");
  for(int i=0;i<::freqList.count();i++)
    {
      if(::modeList.value(i)!=mode) continue;
      QVariantMap m;
      m["index"]=i;
      m["freq"]=::freqList.at(i);
      m["mod"]=::sbModeList.value(i);
      m["pass"]=::passBandList.value(i);
      m["text"]=describe(i);
      l.append(m);
    }
  return l;
}

QString FrequencyController::summary() const
{
  if(::freqList.isEmpty()) return tr("None yet");
  QStringList f;
  for(int i=0;i<::freqList.count() && i<3;i++) f.append(::freqList.at(i));
  return tr("%n saved: %1","",int(::freqList.count())).arg(f.join(QStringLiteral(", "))+(::freqList.count()>3 ? QStringLiteral(" ...") : QString()));
}

QVariantMap FrequencyController::entry(int index) const
{
  QVariantMap m;
  if(index<0 || index>=::freqList.count()) return m;
  m["index"]=index;
  m["freq"]=::freqList.at(index);
  m["drm"]=::modeList.value(index)==QLatin1String("DRM");
  m["mod"]=::sbModeList.value(index);
  m["pass"]=::passBandList.value(index);
  return m;
}

QString FrequencyController::normalized(const QString &freq,bool *ok) const
{
  QString t=freq.trimmed();
  t.replace(QLatin1Char(','),QLatin1Char('.'));
  t.remove(QLatin1Char(' '));
  bool good=false;
  const double v=t.toDouble(&good);
  *ok=good && v>0.01 && v<100000.0;
  return t;
}

int FrequencyController::add(const QString &freq,bool drm,const QString &mod,const QString &pass)
{
  bool ok=false;
  const QString f=normalized(freq,&ok);
  if(!ok) return -1;
  ::freqList.append(f);
  ::modeList.append(drm ? QStringLiteral("DRM") : QStringLiteral("SSTV"));
  ::sbModeList.append(modTable().contains(mod) ? mod : QStringLiteral("USB"));
  ::passBandList.append(passTable().contains(pass) ? pass : QStringLiteral("Normal"));
  commit();
  return int(::freqList.count())-1;
}

bool FrequencyController::update(int index,const QString &freq,bool drm,const QString &mod,const QString &pass)
{
  if(index<0 || index>=::freqList.count()) return false;
  bool ok=false;
  const QString f=normalized(freq,&ok);
  if(!ok) return false;
  ::freqList[index]=f;
  ::modeList[index]=drm ? QStringLiteral("DRM") : QStringLiteral("SSTV");
  ::sbModeList[index]=modTable().contains(mod) ? mod : ::sbModeList.value(index);
  ::passBandList[index]=passTable().contains(pass) ? pass : ::passBandList.value(index);
  commit();
  return true;
}

void FrequencyController::remove(int index)
{
  if(index<0 || index>=::freqList.count()) return;
  m_rf=::freqList.takeAt(index);
  m_rm=::modeList.takeAt(index);
  m_rs=::sbModeList.takeAt(index);
  m_rp=::passBandList.takeAt(index);
  m_removedAt=index;
  m_removedValid=true;
  commit();
  emit removed(tr("%1 MHz removed").arg(m_rf));
}

void FrequencyController::undoRemove()
{
  if(!m_removedValid) return;
  const int at=qBound(0,m_removedAt,int(::freqList.count()));
  ::freqList.insert(at,m_rf);
  ::modeList.insert(at,m_rm);
  ::sbModeList.insert(at,m_rs);
  ::passBandList.insert(at,m_rp);
  m_removedValid=false;
  commit();
}

int FrequencyController::move(int index,int delta)
{
  if(index<0 || index>=::freqList.count() || delta==0) return index;
  // the neighbor is the next entry of the same mode in that direction: the order inside a group is what the picker shows
  const int step=delta>0 ? 1 : -1;
  int other=index+step;
  while(other>=0 && other<::freqList.count() && ::modeList.value(other)!=::modeList.value(index)) other+=step;
  if(other<0 || other>=::freqList.count()) return index;
  ::freqList.swapItemsAt(index,other);
  ::modeList.swapItemsAt(index,other);
  ::sbModeList.swapItemsAt(index,other);
  ::passBandList.swapItemsAt(index,other);
  commit();
  return other;
}

void FrequencyController::addUsual()
{
  struct usual {const char *f; bool drm; const char *mod; const char *pass;};
  static const usual u[]={{"14.230",false,"USB","Wide"},{"7.171",false,"LSB","Normal"},{"3.845",false,"LSB","Normal"},
                          {"21.340",false,"USB","Normal"},{"28.680",false,"USB","Normal"},{"14.233",true,"PKTUSB","Normal"}};
  for(const usual &e:u)
    {
      bool have=false;
      for(int i=0;i<::freqList.count();i++)
        if(::freqList.at(i)==QLatin1String(e.f) && (::modeList.at(i)==QLatin1String("DRM"))==e.drm) have=true;
      if(have) continue;
      ::freqList.append(QString::fromLatin1(e.f));
      ::modeList.append(e.drm ? QStringLiteral("DRM") : QStringLiteral("SSTV"));
      ::sbModeList.append(QString::fromLatin1(e.mod));
      ::passBandList.append(QString::fromLatin1(e.pass));
    }
  commit();
}

QString FrequencyController::command() const {return ::additionalCommand;}
bool FrequencyController::commandHex() const {return ::additionalCommandHex;}

void FrequencyController::setCommand(const QString &c)
{
  if(c==::additionalCommand) return;
  ::additionalCommand=c.trimmed();
  commit();
  emit commandChanged();
}

void FrequencyController::setCommandHex(bool on)
{
  if(on==::additionalCommandHex) return;
  ::additionalCommandHex=on;
  commit();
  emit commandChanged();
}

// the settings file as the classic page writes it, the classic page's table (it would write its own, older rows back when it is deleted), and
// the hidden frequency box of the main window
void FrequencyController::commit()
{
  QSettings s;
  s.beginGroup("FREQSELECT");
  s.setValue("frequencyList",::freqList);
  s.setValue("modeList",::modeList);
  s.setValue("sbModeList",::sbModeList);
  s.setValue("passBandList",::passBandList);
  s.setValue("additionalCommand",::additionalCommand);
  s.setValue("additionalCommandHex",::additionalCommandHex);
  s.endGroup();
  if(configDialogPtr)
    if(frequencySelectWidget *w=configDialogPtr->findChild<frequencySelectWidget *>()) w->setParams();
  if(mainWindowPtr) mainWindowPtr->setSSTVDRMPushButton(transmissionModeIndex==TRXDRM);
  emit listChanged();
  poll();   // the chip knows the entries (the mode of the frequency the radio is on)
}

// ---- the radio ----

bool FrequencyController::radioReady() const
{
  if(m_test) return m_testReady;
  return rigControllerPtr && rigControllerPtr->params()->enableCAT && !rigControllerPtr->params()->radioModel.trimmed().isEmpty();
}

static bool sameFrequency(const QString &list,double mhz) {return std::fabs(list.toDouble()-mhz)<0.0005;}

void FrequencyController::poll()
{
  if(m_test) return;
  QString chip;
  if(radioReady())
    {
      double fr=0;
      if(rigControllerPtr->getFrequency(fr) && fr>1)
        {
          const double mhz=fr/1000000.;
          chip=tr("%1 MHz").arg(QLocale(QLocale::English,QLocale::UnitedStates).toString(mhz,'f',3));
          const bool drm=AppModel::instance()->digital();
          for(int i=0;i<::freqList.count();i++)
            if(::modeList.value(i)==(drm ? QLatin1String("DRM") : QLatin1String("SSTV")) && sameFrequency(::freqList.at(i),mhz))
              {
                chip+=QLatin1Char(' ')+::sbModeList.value(i);
                break;
              }
        }
      else chip=tr("Radio: no answer");
    }
  if(chip!=m_chip)
    {
      m_chip=chip;
      emit radioChanged();
    }
}

QVariantList FrequencyController::pickerEntries() const
{
  QVariantList l;
  const bool drm=AppModel::instance()->digital();
  double mhz=0;
  if(m_test) mhz=14.230;
  else if(rigControllerPtr && radioReady()) {double fr=0; if(rigControllerPtr->getFrequency(fr)) mhz=fr/1000000.;}
  for(const QVariant &e:entries(drm))
    {
      QVariantMap m=e.toMap();
      m["current"]=mhz>0 && sameFrequency(m.value("freq").toString(),mhz);
      l.append(m);
    }
  return l;
}

void FrequencyController::setPickerOpen(bool on)
{
  if(on==m_pickerOpen) return;
  m_pickerOpen=on;
  emit pickerChanged();
}

void FrequencyController::tune(int index)
{
  if(index<0 || index>=::freqList.count()) return;
  setPickerOpen(false);
  if(m_test || !rigControllerPtr) return;
  const double fr=::freqList.at(index).toDouble()*1000000.;
  rigControllerPtr->setFrequency(fr);
  rigControllerPtr->setMode(::sbModeList.value(index),::passBandList.value(index));
  QByteArray ba;
  const QString s=::additionalCommand;
  if(!s.isEmpty() && !rigControllerPtr->params()->enableXMLRPC && hexFromString(s,ba,::additionalCommandHex))
    rigControllerPtr->rawCommand(ba);
  poll();
}

// ---- the test harness ----

void FrequencyController::setTestRadio(bool ready,const QString &chip)
{
  m_test=true;
  m_testReady=ready;
  m_chip=ready ? chip : QString();
  emit radioChanged();
}

void FrequencyController::setTestList(bool filled)
{
  ::freqList.clear();
  ::modeList.clear();
  ::sbModeList.clear();
  ::passBandList.clear();
  if(filled) addUsual();
  else commit();
  emit listChanged();
}
