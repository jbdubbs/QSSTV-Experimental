#include "touchpage.h"
#include "uimode.h"

#include <QButtonGroup>
#include <QDialog>
#include <QScreen>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScroller>
#include <QScrollBar>
#include <QScrollerProperties>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

//! a finger scrolls, so the scroll bar is only a thin marker of where the page is
void slimScrollBar(QScrollArea *area)
{
  area->verticalScrollBar()->setStyleSheet(
    "QScrollBar:vertical{width:6px;margin:0;background:transparent;}"
    "QScrollBar::handle:vertical{background:rgba(0,0,0,110);border-radius:3px;min-height:40px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
    "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}");
}

QScrollArea *makeTouchScrollPage(QWidget *content,QWidget *parent)
{
  QScrollArea *area=new QScrollArea(parent);
  area->setFrameShape(QFrame::NoFrame);
  area->setWidgetResizable(true);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // A form's own minimumSize (rxwidget.ui has 0x20) replaces what its layout needs in the scroll area's reckoning, and the page
  // is then squeezed into the viewport instead of scrolling: take it off, the layout knows the size better.
  content->setMinimumSize(0,0);
  area->setWidget(content);
  slimScrollBar(area);
  // a finger drags the page; a press on a button or a box inside it still reaches that control until the finger moves
  QScroller::grabGesture(area->viewport(),QScroller::LeftMouseButtonGesture);
  QScrollerProperties p=QScroller::scroller(area->viewport())->scrollerProperties();
  p.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,QScrollerProperties::OvershootAlwaysOff);
  p.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,QScrollerProperties::OvershootWhenScrollable);
  QScroller::scroller(area->viewport())->setScrollerProperties(p);
  return area;
}

QSize touchStyle::sizeFromContents(ContentsType type,const QStyleOption *option,const QSize &size,const QWidget *widget) const
{
  QSize s=QProxyStyle::sizeFromContents(type,option,size,widget);
  switch(type)
    {
    case CT_PushButton:
    case CT_ComboBox:
    case CT_SpinBox:
    case CT_LineEdit:
      s.setHeight(qMax(s.height(),uiMode::tapSize-8));
      break;
    case CT_CheckBox:
    case CT_RadioButton:
      s.setHeight(qMax(s.height(),uiMode::tapSize-12));
      break;
    default:
      break;
    }
  return s;
}

int touchStyle::pixelMetric(PixelMetric metric,const QStyleOption *option,const QWidget *widget) const
{
  const int m=QProxyStyle::pixelMetric(metric,option,widget);
  switch(metric)
    {
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
      return qMax(m,22);
    default:
      return m;
    }
}

QScrollArea *makeTouchScrollPage2D(QWidget *content,QWidget *parent)
{
  QScrollArea *area=makeTouchScrollPage(content,parent);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  area->horizontalScrollBar()->setStyleSheet(
    "QScrollBar:horizontal{height:6px;margin:0;background:transparent;}"
    "QScrollBar::handle:horizontal{background:rgba(0,0,0,110);border-radius:3px;min-width:40px;}"
    "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
    "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{background:transparent;}");
  QScrollerProperties p=QScroller::scroller(area->viewport())->scrollerProperties();
  p.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,QScrollerProperties::OvershootWhenScrollable);
  QScroller::scroller(area->viewport())->setScrollerProperties(p);
  return area;
}

void prepareTouchWindow(QWidget *w)
{
  if(!w->isWindow() || !qobject_cast<QDialog *>(w) || w->property("touchPrepared").toBool()) return;
  w->setProperty("touchPrepared",true);
  const QRect avail=w->screen()->availableGeometry();
  const QSize hint=w->sizeHint().expandedTo(w->minimumSizeHint());
  if(hint.width()>avail.width()*85/100 || hint.height()>avail.height()*85/100)
    {
      w->setWindowState(w->windowState()|Qt::WindowMaximized);
      w->setProperty("touchMaximized",true);
    }
}

morePage::morePage(QWidget *parent) : QScrollArea(parent)
{
  QWidget *content=new QWidget;
  box=new QVBoxLayout(content);
  box->setContentsMargins(12,12,12,12);
  box->setSpacing(8);
  box->addStretch(1);
  setFrameShape(QFrame::NoFrame);
  setWidgetResizable(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWidget(content);
  slimScrollBar(this);
  QScroller::grabGesture(viewport(),QScroller::LeftMouseButtonGesture);
}

void morePage::addButton(const QString &text,const std::function<void()> &action)
{
  QPushButton *b=new QPushButton(text);
  b->setMinimumHeight(uiMode::tapSize);
  QFont f=b->font();
  f.setPixelSize(16);
  b->setFont(f);
  connect(b,&QPushButton::clicked,this,[action]{action();});
  box->insertWidget(box->count()-1,b);   // in front of the closing stretch
}

void morePage::addHeading(const QString &text)
{
  QLabel *l=new QLabel(text);
  QFont f=l->font();
  f.setBold(true);
  l->setFont(f);
  l->setContentsMargins(2,10,0,0);
  box->insertWidget(box->count()-1,l);
}

QString bottomNav::styleSheet(bool rail)
{
  // on the buttons only, so nothing else in the program is restyled
  const QString edge=rail ? "border-left" : "border-top";
  return QString("QPushButton{font-size:15px;border:0;%1:1px solid #b0b0b8;background:#e8e8ee;border-radius:0;}"
                 "QPushButton:checked{font-weight:bold;background:#ffffff;%1:3px solid #2f6fd0;}"
                 "QPushButton:pressed{background:#d4d4dc;}").arg(edge);
}

bottomNav::bottomNav(QTabWidget *tabs,QWidget *parent) : QWidget(parent)
{
  row=new QHBoxLayout(this);
  row->setContentsMargins(0,0,0,0);
  row->setSpacing(0);
  QButtonGroup *group=new QButtonGroup(this);
  group->setExclusive(true);
  for(int i=0;i<tabs->count();i++)
    {
      QPushButton *b=new QPushButton(tabs->tabText(i),this);
      b->setCheckable(true);
      b->setFocusPolicy(Qt::NoFocus);
      b->setMinimumHeight(uiMode::tapSize+4);
      b->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
      b->setStyleSheet(styleSheet(false));
      group->addButton(b,i);
      row->addWidget(b,1);
      buttons<<b;
    }
  tabs->tabBar()->hide();
  tabs->setDocumentMode(true);   // no frame round the pages
  if(QAbstractButton *b=group->button(tabs->currentIndex())) b->setChecked(true);
  connect(group,&QButtonGroup::idClicked,tabs,&QTabWidget::setCurrentIndex);
  connect(tabs,&QTabWidget::currentChanged,this,[group](int i)
    {
      if(QAbstractButton *b=group->button(i)) b->setChecked(true);
    });
}

void bottomNav::setRail(bool rail)
{
  row->setDirection(rail ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
  if(rail)
    {
      setFixedWidth(96);
    }
  else
    {
      setMinimumWidth(0);
      setMaximumWidth(QWIDGETSIZE_MAX);
    }
  for(QPushButton *b:buttons)
    {
      b->setSizePolicy(QSizePolicy::Expanding,rail ? QSizePolicy::Expanding : QSizePolicy::Fixed);
      b->setStyleSheet(styleSheet(rail));
    }
}
