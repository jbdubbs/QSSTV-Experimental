#include "drmprofilescontroller.h"
#include "appglobal.h"
#include "configdialog.h"
#include "drmprofileconfig.h"
#include "txcontroller.h"
#include "txwidget.h"

#include <QJSEngine>
#include <QQmlEngine>
#include <QSettings>

namespace
{
struct option {const char *key; const char *title; QStringList items; const char *hint;};

// the choices and the words of the User's Guide (Digital SSTV, Parameters)
const QList<option> &optionTable()
{
  static const QList<option> t={
    {"mode","Mode",{"A","B","E"},"Robustness. A is the fastest, E the most robust."},
    {"qam","QAM",{"4","16","64"},"Bits per carrier. 4 is the most robust; 64 is the fastest but needs a clean signal."},
    {"bandwidth","Bandwidth",{"2.3 kHz","2.8 kHz","4.8 kHz"},"Wider is faster. 4.8 kHz is for VHF and UHF FM."},
    {"interleave","Interleave",{"Short","Long"},"Long survives longer fades but adds delay."},
    {"protection","Protection",{"High","Low"},"Error correction. High is more robust but slower."},
    {"rs","Reed-Solomon",{"None","RS1","RS2","RS3","RS4"},"Extra repair data. More is slower and more robust. The picture is not shown while it arrives."}};
  return t;
}

int *field(drmTxParams &p,const QString &key)
{
  if(key=="mode") return &p.robMode;
  if(key=="qam") return &p.qam;
  if(key=="bandwidth") return &p.bandwith;
  if(key=="interleave") return &p.interleaver;
  if(key=="protection") return &p.protection;
  if(key=="rs") return &p.reedSolomon;
  return nullptr;
}

QString pick(const QStringList &l,int i) {return (i>=0 && i<l.size()) ? l.at(i) : QString();}
}

DrmProfilesController *DrmProfilesController::instance()
{
  static DrmProfilesController *c=new DrmProfilesController;
  return c;
}

DrmProfilesController *DrmProfilesController::create(QQmlEngine *,QJSEngine *)
{
  DrmProfilesController *c=instance();
  QJSEngine::setObjectOwnership(c,QJSEngine::CppOwnership);
  return c;
}

QVariantList DrmProfilesController::profiles() const
{
  QVariantList l;
  const QList<option> &o=optionTable();
  for(int i=0;i<NUMBEROFPROFILES;i++)
    {
      const drmTxParams &p=drmPFArray[i].params;
      QVariantMap m;
      m["index"]=i;
      m["name"]=drmPFArray[i].name;
      m["line1"]=tr("Mode %1, %2-QAM, %3").arg(pick(o.at(0).items,p.robMode),pick(o.at(1).items,p.qam),pick(o.at(2).items,p.bandwith));
      const QString rs=p.reedSolomon>0 ? tr("Reed-Solomon %1").arg(pick(o.at(5).items,p.reedSolomon)) : tr("no Reed-Solomon");
      m["line2"]=tr("%1 interleave, %2 protection, %3").arg(pick(o.at(3).items,p.interleaver),pick(o.at(4).items,p.protection).toLower(),rs);
      m["options"]=options(i);   // the pages read everything from this one list, so that a change reaches all of them
      l.append(m);
    }
  return l;
}

QString DrmProfilesController::summary() const
{
  QStringList n;
  for(int i=0;i<NUMBEROFPROFILES;i++) n.append(drmPFArray[i].name);
  return n.join(QStringLiteral(", "));
}

static bool isFactory(const sprofile &p,int i)
{
  const drmTxParams &d=p.params;
  return p.name==QStringLiteral("Profile %1").arg(i+1) && d.robMode==0 && d.qam==0 && d.bandwith==0 && d.interleaver==0 && d.protection==0 && d.reedSolomon==0;
}

bool DrmProfilesController::factory() const
{
  for(int i=0;i<NUMBEROFPROFILES;i++) if(!isFactory(drmPFArray[i],i)) return false;
  return true;
}

bool DrmProfilesController::showPartial() const {return ::drmShowPartialImage;}

void DrmProfilesController::setShowPartial(bool on)
{
  if(on==::drmShowPartialImage) return;
  ::drmShowPartialImage=on;
  commit(false);
}

QString DrmProfilesController::name(int index) const
{
  return (index>=0 && index<NUMBEROFPROFILES) ? drmPFArray[index].name : QString();
}

void DrmProfilesController::setName(int index,const QString &n)
{
  if(index<0 || index>=NUMBEROFPROFILES) return;
  const QString v=n.trimmed().isEmpty() ? QStringLiteral("Profile %1").arg(index+1) : n.trimmed();
  if(v==drmPFArray[index].name) return;
  drmPFArray[index].name=v;
  commit(true);
}

QVariantList DrmProfilesController::options(int index) const
{
  QVariantList l;
  if(index<0 || index>=NUMBEROFPROFILES) return l;
  drmTxParams p=drmPFArray[index].params;
  for(const option &o:optionTable())
    {
      QVariantMap m;
      m["key"]=QString::fromLatin1(o.key);
      m["title"]=tr(o.title);
      m["items"]=o.items;
      m["hint"]=tr(o.hint);
      const int *f=field(p,QString::fromLatin1(o.key));
      m["index"]=f ? *f : 0;
      l.append(m);
    }
  return l;
}

void DrmProfilesController::setOption(int index,const QString &key,int choice)
{
  if(index<0 || index>=NUMBEROFPROFILES) return;
  int *f=field(drmPFArray[index].params,key);
  if(!f || choice<0) return;
  for(const option &o:optionTable())
    if(key==QLatin1String(o.key) && choice>=o.items.size()) return;
  if(*f==choice) return;
  *f=choice;
  commit(true);
}

void DrmProfilesController::useTypical()
{
  if(!factory()) return;
  struct typical {const char *name; int mode,qam,bw,interleave,protection,rs;};
  // robustness mode B in all three; 4, 16 and 64-QAM; the fast one a little wider, with short interleaving and low protection
  static const typical t[NUMBEROFPROFILES]={{"Robust",1,0,0,1,0,0},{"Balanced",1,1,0,1,0,0},{"Fast",1,2,1,0,1,0}};
  for(int i=0;i<NUMBEROFPROFILES;i++)
    {
      drmPFArray[i].name=QString::fromLatin1(t[i].name);
      drmPFArray[i].params.robMode=t[i].mode;
      drmPFArray[i].params.qam=t[i].qam;
      drmPFArray[i].params.bandwith=t[i].bw;
      drmPFArray[i].params.interleaver=t[i].interleave;
      drmPFArray[i].params.protection=t[i].protection;
      drmPFArray[i].params.reedSolomon=t[i].rs;
    }
  commit(true);
}

void DrmProfilesController::setTestProfiles(bool typical)
{
  for(int i=0;i<NUMBEROFPROFILES;i++)
    {
      drmPFArray[i].name=QStringLiteral("Profile %1").arg(i+1);
      drmPFArray[i].params.robMode=drmPFArray[i].params.qam=drmPFArray[i].params.bandwith=0;
      drmPFArray[i].params.interleaver=drmPFArray[i].params.protection=drmPFArray[i].params.reedSolomon=0;
    }
  if(typical) useTypical();
  emit changed();
}

// the classic page's fields follow the values (it would write its own, older ones back when it is deleted), the settings file is written as the
// classic page does, and the Transmit screen takes up the new names and the profile it is on
void DrmProfilesController::commit(bool profilesChanged)
{
  // (drmProfileConfigPtr is never set: the classic page is found among the children of the configuration dialog)
  drmProfileConfig *page=configDialogPtr ? configDialogPtr->findChild<drmProfileConfig *>() : nullptr;
  if(page)
    {
      page->setParams();
      page->writeSettings();
    }
  else
    {
      QSettings s;
      s.beginGroup("DRMPROFILE");
      s.setValue("drmShowPartialImage",::drmShowPartialImage);
      s.endGroup();
    }
  if(profilesChanged && txWidgetPtr)
    {
      txWidgetPtr->reloadProfiles();
      emit TxController::instance()->drmChanged();
    }
  emit changed();
}
