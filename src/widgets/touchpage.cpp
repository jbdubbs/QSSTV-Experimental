#include "touchpage.h"
#include "uimode.h"

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QButtonGroup>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyleHints>
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
    "QScrollBar::handle:vertical{background:palette(mid);border-radius:3px;min-height:40px;}"
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
  // a finger drags the page (touchFlick, for a press on any widget in it)
  area->setProperty("touchScroll",true);
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
    "QScrollBar::handle:horizontal{background:palette(mid);border-radius:3px;min-width:40px;}"
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
  QSize hint=w->sizeHint().expandedTo(w->minimumSizeHint());
  if(w->testAttribute(Qt::WA_Resized)) hint=hint.expandedTo(w->size());   // a size the program asked for (resize(900,600))
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
  setProperty("touchScroll",true);
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
  // on the buttons only, so nothing else in the program is restyled; the colours are the palette's, light or dark
  const QString edge=rail ? "border-left" : "border-top";
  return QString("QPushButton{font-size:15px;border:0;%1:1px solid palette(mid);background:palette(button);color:palette(button-text);border-radius:0;}"
                 "QPushButton:checked{font-weight:bold;background:palette(base);color:palette(text);%1:3px solid palette(highlight);}"
                 "QPushButton:pressed{background:palette(midlight);}").arg(edge);
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

QPalette touchPalette(bool dark)
{
  QPalette p;
  auto set=[&p](QPalette::ColorRole r,const char *normal,const char *disabled=nullptr)
  {
    p.setColor(QPalette::Active,r,QColor(normal));
    p.setColor(QPalette::Inactive,r,QColor(normal));
    p.setColor(QPalette::Disabled,r,QColor(disabled ? disabled : normal));
  };
  if(dark)
    {
      set(QPalette::Window,"#1f2024");
      set(QPalette::WindowText,"#e4e4e8","#7a7c84");
      set(QPalette::Base,"#17181b");
      set(QPalette::AlternateBase,"#232428");
      set(QPalette::Text,"#e4e4e8","#7a7c84");
      set(QPalette::Button,"#2e3036");
      set(QPalette::ButtonText,"#e4e4e8","#7a7c84");
      set(QPalette::BrightText,"#ffffff");
      set(QPalette::Light,"#4a4c55");
      set(QPalette::Midlight,"#3b3d45");
      set(QPalette::Mid,"#6a6c75");
      set(QPalette::Dark,"#141518");
      set(QPalette::Shadow,"#000000");
      set(QPalette::Highlight,"#3d7be0","#3b3d45");
      set(QPalette::HighlightedText,"#ffffff","#9a9ca4");
      set(QPalette::Link,"#7aa7ff");
      set(QPalette::LinkVisited,"#b79bff");
      set(QPalette::ToolTipBase,"#3b3d45");
      set(QPalette::ToolTipText,"#e4e4e8");
      set(QPalette::PlaceholderText,"#8c8e96");
    }
  else
    {
      set(QPalette::Window,"#efeff4");
      set(QPalette::WindowText,"#1b1b1f","#9a9aa2");
      set(QPalette::Base,"#ffffff");
      set(QPalette::AlternateBase,"#f4f4f8");
      set(QPalette::Text,"#1b1b1f","#9a9aa2");
      set(QPalette::Button,"#e3e3ea");
      set(QPalette::ButtonText,"#1b1b1f","#9a9aa2");
      set(QPalette::BrightText,"#ffffff");
      set(QPalette::Light,"#ffffff");
      set(QPalette::Midlight,"#d4d4dc");
      set(QPalette::Mid,"#a9a9b4");
      set(QPalette::Dark,"#8e8e9a");
      set(QPalette::Shadow,"#50505a");
      set(QPalette::Highlight,"#2f6fd0","#c8c8d0");
      set(QPalette::HighlightedText,"#ffffff","#ffffff");
      set(QPalette::Link,"#0b57d0");
      set(QPalette::LinkVisited,"#6a3fb5");
      set(QPalette::ToolTipBase,"#fff7d6");
      set(QPalette::ToolTipText,"#1b1b1f");
      set(QPalette::PlaceholderText,"#77777f");
    }
  return p;
}

void applyTouchPalette()
{
  static bool connected=false;
  QGuiApplication::setPalette(touchPalette(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark));
  if(!connected)
    {
      connected=true;
      QObject::connect(QGuiApplication::styleHints(),&QStyleHints::colorSchemeChanged,qApp,[]{applyTouchPalette();});
    }
}

// ---------------------------------------------------------------------------------------------------------------------

QScrollArea *touchFlick::areaOf(QWidget *w,bool &pass) const
{
  pass=false;
  for(QWidget *x=w;x;x=x->parentWidget())
    {
      const flickAware *fa=dynamic_cast<const flickAware *>(x);
      const QString mode=fa ? fa->touchFlickMode() : x->property("touchFlick").toString();
      if(mode=="skip") return nullptr;
      if(mode=="pass") pass=true;
      if(qobject_cast<QAbstractSlider *>(x)) return nullptr;   // a slider or a scroll bar is dragged
      if(x->property("touchScroll").toBool()) return qobject_cast<QScrollArea *>(x);
      // a text area, a list or a table inside the page scrolls itself (its viewport is x; the page's own viewport is not meant)
      if(QAbstractScrollArea *other=qobject_cast<QAbstractScrollArea *>(x->parentWidget()))
        if(other->viewport()==x && !other->property("touchScroll").toBool()) return nullptr;
    }
  return nullptr;
}

bool touchFlick::eventFilter(QObject *o,QEvent *e)
{
  const QEvent::Type t=e->type();
  if(e==replayEvent || !o->isWidgetType()) return false;   // our own replay: not again
  if(t!=QEvent::MouseButtonPress && t!=QEvent::MouseButtonDblClick && t!=QEvent::MouseMove && t!=QEvent::MouseButtonRelease) return false;
  QMouseEvent *me=static_cast<QMouseEvent *>(e);
  QWidget *w=static_cast<QWidget *>(o);

  if(t==QEvent::MouseButtonPress || t==QEvent::MouseButtonDblClick)
    {
      if(me->button()!=Qt::LeftButton) return false;
      target=nullptr;   // a press while one was thought to be down: its release was lost
      area=nullptr;
      dragging=false;
      bool pass=false;
      QScrollArea *a=areaOf(w,pass);
      if(!a) return false;
      target=w;
      area=a;
      passive=pass;
      dragging=false;
      pressLocal=me->position();
      pressGlobal=me->globalPosition();
      QScroller::scroller(a->viewport())->handleInput(QScroller::InputPress,pressGlobal,me->timestamp());
      return !passive;   // a picture has its press at once, anything else when the finger is up
    }

  if(!target || !area) return false;
  if(t==QEvent::MouseMove)
    {
      if(!(me->buttons() & Qt::LeftButton)) return false;
      const QPointF g=me->globalPosition();
      if(!dragging && (g-pressGlobal).manhattanLength()>qMax(QGuiApplication::styleHints()->startDragDistance(),12))
        {
          dragging=true;
          if(passive)
            {
              // the picture took the press: let it end that (its long press, its drag) before the page takes over
              QMouseEvent up(QEvent::MouseButtonRelease,target->mapFromGlobal(g.toPoint()),g,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
              replayEvent=&up;
              QCoreApplication::sendEvent(target,&up);
              replayEvent=nullptr;
            }
        }
      if(dragging) QScroller::scroller(area->viewport())->handleInput(QScroller::InputMove,g,me->timestamp());
      return dragging || !passive;
    }

  // release
  if(me->button()!=Qt::LeftButton) return false;
  QScroller::scroller(area->viewport())->handleInput(QScroller::InputRelease,me->globalPosition(),me->timestamp());
  QPointer<QWidget> tgt=target;
  const bool wasDragging=dragging;
  const bool wasPassive=passive;
  target=nullptr;
  area=nullptr;
  dragging=false;
  if(wasDragging) return true;
  if(wasPassive) return false;
  // a tap on a control: the press and the release it was waiting for
  if(tgt)
    {
      // (a release can open a dialog with an event loop of its own: only these two events are let through, not everything meanwhile)
      QMouseEvent down(QEvent::MouseButtonPress,pressLocal,pressGlobal,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
      replayEvent=&down;
      QCoreApplication::sendEvent(tgt,&down);
      replayEvent=nullptr;
      if(tgt)
        {
          QMouseEvent up(QEvent::MouseButtonRelease,pressLocal,pressGlobal,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
          replayEvent=&up;
          QCoreApplication::sendEvent(tgt,&up);
          replayEvent=nullptr;
        }
    }
  return true;
}
