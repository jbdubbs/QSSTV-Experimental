#include "txcontroller.h"
#include "appglobal.h"
#include "appmodel.h"
#include "imageviewer.h"
#include "operatorconfig.h"
#include "sstvparam.h"
#include "txfunctions.h"
#include "txwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QJSEngine>
#include <QLineEdit>
#include <QQmlEngine>

TxController *TxController::instance()
{
  static TxController *c=new TxController;
  return c;
}

TxController *TxController::create(QQmlEngine *,QJSEngine *)
{
  TxController *c=instance();
  QJSEngine::setObjectOwnership(c,QJSEngine::CppOwnership);
  return c;
}

TxController::TxController(QObject *parent) : QObject(parent)
{
  m_timer.setInterval(200);
  connect(&m_timer,&QTimer::timeout,this,&TxController::poll);
  m_timer.start();
  connectToPage();
}

void TxController::connectToPage()
{
  if(m_connected || !txWidgetPtr) return;
  m_connected=true;
  connect(txWidgetPtr,&txWidget::progressChanged,this,&TxController::progressTick);
  connect(txWidgetPtr,&txWidget::txTimeChanged,this,[this]{refreshPicture(); emit timeChanged();});
  connect(txWidgetPtr->getImageViewerPtr(),&imageViewer::imageChanged,this,&TxController::refreshPicture);
  refreshPicture();
}

void TxController::refreshPicture()
{
  if(!txWidgetPtr) return;
  imageViewer *v=txWidgetPtr->getImageViewerPtr();
  const QImage *im=v->hasValidImage() ? v->getDisplayedImage() : nullptr;
  m_picture=(im && !im->isNull()) ? *im : QImage();
  ++m_pictureRevision;
  emit pictureChanged();
}

int TxController::mode() const {return int(sstvModeIndexTx);}

void TxController::setMode(int m)
{
  if(m<0 || m>=NUMSSTVMODES || !txWidgetPtr) return;
  // the page's own combo box: it does what a choice there does (the grid control, the template, the saving)
  if(QComboBox *box=txWidgetPtr->findChild<QComboBox *>(QStringLiteral("sstvModeComboBox"))) box->setCurrentIndex(m);
  m_cachedMode=int(sstvModeIndexTx);
  emit modeChanged();
  emit timeChanged();
}

QString TxController::modeName() const
{
  return SSTVTable[int(sstvModeIndexTx)].name;
}

QString TxController::pictureSize() const
{
  const sSSTVParam &p=SSTVTable[int(sstvModeIndexTx)];
  return QString("%1 x %2").arg(p.numberOfPixels).arg(p.numberOfDisplayLines);
}

QVariantList TxController::favorites() const
{
  // Scottie 1, Martin 1, PD120 and Robot 36 (the user's choice at the design review); the other modes are under "All modes"
  QVariantList l;
  for(esstvMode m:{S1,M1,PD120,R36})
    {
      QVariantMap e;
      e["mode"]=int(m);
      e["name"]=SSTVTable[int(m)].name;
      l.append(e);
    }
  return l;
}

static QString timeString(int seconds)
{
  if(seconds<=0) return QString();
  const int minutes=seconds/60;
  seconds-=minutes*60;
  if(minutes==0) return QObject::tr("%1 s").arg(seconds);
  return QObject::tr("%1 min %2 s").arg(minutes).arg(seconds);
}

QString TxController::timeText() const
{
  return txWidgetPtr ? timeString(txWidgetPtr->txSeconds()) : QString();
}

QVariantList TxController::allModes() const
{
  QVariantList l;
  for(int i=0;i<NUMSSTVMODES;i++)
    {
      const sSSTVParam &p=SSTVTable[i];
      if(p.name.isEmpty()) continue;
      const int seconds=int(p.imageTime+0.5);
      QVariantMap e;
      e["mode"]=i;
      e["name"]=p.name;
      e["size"]=QString("%1 x %2").arg(p.numberOfPixels).arg(p.numberOfDisplayLines);
      e["time"]=timeString(seconds);
      e["seconds"]=seconds;
      e["group"]=seconds<=75 ? tr("Quick") : seconds<=150 ? tr("Standard") : tr("Long");
      l.append(e);
    }
  // quick ones first, each group by length
  std::stable_sort(l.begin(),l.end(),[](const QVariant &a,const QVariant &b)
    {
      return a.toMap().value("seconds").toInt()<b.toMap().value("seconds").toInt();
    });
  return l;
}

QString TxController::remainingText() const
{
  const int total=txWidgetPtr ? txWidgetPtr->txSeconds() : 0;
  const int left=qMax(0,int(total*(1.0-m_progress)+0.5));
  return QString("%1:%2").arg(left/60).arg(left%60,2,10,QLatin1Char('0'));
}

bool TxController::useTemplateOn() const {return ::useTemplate;}

void TxController::setUseTemplate(bool on)
{
  if(!txWidgetPtr) return;
  if(QCheckBox *box=txWidgetPtr->findChild<QCheckBox *>(QStringLiteral("templateCheckBox"))) box->setChecked(on);
  emit detailsChanged();
}

static QLineEdit *lineEdit(const char *name)
{
  return txWidgetPtr ? txWidgetPtr->findChild<QLineEdit *>(QString::fromLatin1(name)) : nullptr;
}

static void setLine(const char *name,const QString &s)
{
  if(QLineEdit *e=lineEdit(name))
    {
      e->setText(s);
      emit e->editingFinished();   // the page takes the text over when editing finishes
    }
}

QString TxController::toCall() const {QLineEdit *e=lineEdit("toCallLineEdit"); return e ? e->text() : QString();}
void TxController::setToCall(const QString &s) {setLine("toCallLineEdit",s); emit detailsChanged();}
QString TxController::toOperator() const {QLineEdit *e=lineEdit("operatorLineEdit"); return e ? e->text() : QString();}
void TxController::setToOperator(const QString &s) {setLine("operatorLineEdit",s); emit detailsChanged();}
QString TxController::report() const {QLineEdit *e=lineEdit("rsvLineEdit"); return e ? e->text() : QString();}
void TxController::setReport(const QString &s) {setLine("rsvLineEdit",s); emit detailsChanged();}

QString TxController::detailsSummary() const
{
  QStringList parts;
  if(!myCallsign.isEmpty()) parts<<myCallsign;
  if(!toCall().isEmpty()) parts<<tr("To %1").arg(toCall());
  if(!report().isEmpty()) parts<<report();
  return parts.join(QStringLiteral(" · "));
}

void TxController::pickPhoto() {pickFile();}

void TxController::pickFile()
{
  if(txWidgetPtr) QMetaObject::invokeMethod(txWidgetPtr,"slotFileOpen",Qt::QueuedConnection);
}

void TxController::camera()
{
  if(txWidgetPtr) QMetaObject::invokeMethod(txWidgetPtr,"slotSnapshot",Qt::QueuedConnection);
}

void TxController::showPicture(const QString &file)
{
  if(!txWidgetPtr) return;
  txWidgetPtr->setImage(file);
  AppModel::instance()->setScreen(1);
}

void TxController::send()
{
  if(txWidgetPtr && hasPicture()) QMetaObject::invokeMethod(txWidgetPtr,"slotStart",Qt::QueuedConnection);
}

void TxController::stop()
{
  if(txWidgetPtr) QMetaObject::invokeMethod(txWidgetPtr,"slotStop",Qt::QueuedConnection);
}

void TxController::progressTick(int percent)
{
  if(m_test) return;
  m_progress=qBound(0.0,percent/100.0,1.0);
  emit progressChanged();
}

void TxController::setTestSending(qreal progress)
{
  m_test=progress>=0;
  m_sending=m_test;
  m_progress=m_test ? progress : 0;
  emit sendingChanged();
  emit progressChanged();
}

void TxController::poll()
{
  connectToPage();
  if(!txWidgetPtr) return;
  if(m_cachedMode!=int(sstvModeIndexTx))
    {
      m_cachedMode=int(sstvModeIndexTx);
      emit modeChanged();
    }
  if(m_test) return;
  const bool sending=txWidgetPtr->functionsPtr() && txWidgetPtr->functionsPtr()->txBusy();
  if(sending!=m_sending)
    {
      m_sending=sending;
      if(!sending) {m_progress=0; emit progressChanged();}
      emit sendingChanged();
    }
}
