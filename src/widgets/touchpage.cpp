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
#include <QPainter>
#include <QRadialGradient>
#include <QScrollBar>
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
  setupTouchScrolling(area,false);   // a finger drags the page (touchFlick, for a press on any widget in it)
  return area;
}

void setupTouchScrolling(QAbstractScrollArea *area,bool sideways)
{
  area->setProperty("touchScroll",true);
  QScroller *scroller=QScroller::scroller(area->viewport());
  QScrollerProperties p=scroller->scrollerProperties();
  // no rubber band: the content stops at its end (the glow shows it); nothing swings past and back
  p.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,QScrollerProperties::OvershootAlwaysOff);
  p.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,QScrollerProperties::OvershootAlwaysOff);
  // a flick that slows down sooner and does not run away (Qt's own: 0.125 and 0.5 m/s)
  p.setScrollMetric(QScrollerProperties::DecelerationFactor,0.2);
  p.setScrollMetric(QScrollerProperties::MaximumVelocity,0.4);
  scroller->setScrollerProperties(p);
  new edgeGlow(area);
  Q_UNUSED(sideways);
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

QAbstractScrollArea *touchFlick::areaOf(QWidget *w,bool &pass) const
{
  pass=false;
  for(QWidget *x=w;x;x=x->parentWidget())
    {
      const flickAware *fa=dynamic_cast<const flickAware *>(x);
      const QString mode=fa ? fa->touchFlickMode() : x->property("touchFlick").toString();
      if(mode=="skip") return nullptr;
      if(mode=="pass") pass=true;
      if(qobject_cast<QAbstractSlider *>(x)) return nullptr;   // a slider or a scroll bar is dragged
      if(x->property("touchScroll").toBool()) return qobject_cast<QAbstractScrollArea *>(x);
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
      QAbstractScrollArea *a=areaOf(w,pass);
      if(!a) return false;
      target=w;
      area=a;
      passive=pass;
      dragging=false;
      pressLocal=me->position();
      pressGlobal=me->globalPosition();
      lastGlobal=pressGlobal;
      a->setProperty("fingerDown",true);   // a fling that reaches the end is not a drag that reaches it (edgeGlow)
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
      if(dragging)
        {
          QScroller::scroller(area->viewport())->handleInput(QScroller::InputMove,g,me->timestamp());
          // the finger goes on after the content has stopped at its end: the glow comes up (Android's onPull)
          const double dx=g.x()-lastGlobal.x(),dy=g.y()-lastGlobal.y();
          bool pulled=false;
          if(edgeGlow *glow=area->findChild<edgeGlow *>())
            {
              QScrollBar *vb=area->verticalScrollBar(),*hb=area->horizontalScrollBar();
              if(vb->maximum()>vb->minimum())
                {
                  if(dy<0 && vb->value()>=vb->maximum()) {glow->pull(edgeGlow::Bottom,-dy);pulled=true;}
                  else if(dy>0 && vb->value()<=vb->minimum()) {glow->pull(edgeGlow::Top,dy);pulled=true;}
                }
              if(!pulled && hb->maximum()>hb->minimum())
                {
                  if(dx<0 && hb->value()>=hb->maximum()) {glow->pull(edgeGlow::Right,-dx);pulled=true;}
                  else if(dx>0 && hb->value()<=hb->minimum()) {glow->pull(edgeGlow::Left,dx);pulled=true;}
                }
              if(!pulled) glow->letGo();
            }
        }
      lastGlobal=g;
      return dragging || !passive;
    }

  // release
  if(me->button()!=Qt::LeftButton) return false;
  QScroller::scroller(area->viewport())->handleInput(QScroller::InputRelease,me->globalPosition(),me->timestamp());
  area->setProperty("fingerDown",false);
  if(edgeGlow *glow=area->findChild<edgeGlow *>())
    {
      glow->letGo();
      if(dragging) glow->flung(QScroller::scroller(area->viewport())->velocity());
    }
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

// ---------------------------------------------------------------------------------------------------------------------

edgeGlow::edgeGlow(QAbstractScrollArea *a) : QWidget(a->viewport()), area(a)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setGeometry(a->viewport()->rect());
  a->viewport()->installEventFilter(this);
  raise();
  timer.setInterval(16);
  connect(&timer,&QTimer::timeout,this,&edgeGlow::tick);
  clock.start();
  connect(a->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int v){scrolled(Qt::Vertical,v);});
  connect(a->horizontalScrollBar(),&QScrollBar::valueChanged,this,[this](int v){scrolled(Qt::Horizontal,v);});
  // a flick that is heading for an end comes to rest there (Qt slows it down to the end, so the speed on arrival says nothing)
  connect(QScroller::scroller(a->viewport()),&QScroller::stateChanged,this,[this](QScroller::State state)
    {
      if(state==QScroller::Pressed || state==QScroller::Dragging) {flingEdge=-1;return;}
      if(state!=QScroller::Inactive || flingEdge<0) return;
      QScrollBar *bar=(flingEdge==Top || flingEdge==Bottom) ? area->verticalScrollBar() : area->horizontalScrollBar();
      const bool atEnd=(flingEdge==Bottom || flingEdge==Right) ? bar->value()>=bar->maximum() : bar->value()<=bar->minimum();
      if(atEnd) absorb(Edge(flingEdge),flingSpeed);
      flingEdge=-1;
    });
}

void edgeGlow::flung(const QPointF &velocity)
{
  const double perMeter=area->logicalDpiY()/0.0254;
  const double vx=velocity.x()*perMeter,vy=velocity.y()*perMeter;
  flingEdge=-1;
  if(qMax(qAbs(vx),qAbs(vy))<300) return;
  if(qAbs(vy)>=qAbs(vx)) {flingEdge=vy>0 ? Bottom : Top;flingSpeed=qAbs(vy);}
  else {flingEdge=vx>0 ? Right : Left;flingSpeed=qAbs(vx);}
}

bool edgeGlow::eventFilter(QObject *o,QEvent *e)
{
  if(o==area->viewport() && (e->type()==QEvent::Resize || e->type()==QEvent::ChildAdded || e->type()==QEvent::Show))
    {
      setGeometry(area->viewport()->rect());
      raise();   // above the content, which is a child of the same viewport
    }
  return QWidget::eventFilter(o,e);
}

void edgeGlow::start()
{
  lastTick=clock.elapsed();
  if(!timer.isActive()) timer.start();
}

void edgeGlow::pull(Edge e,double pixels)
{
  const double extent=(e==Top || e==Bottom) ? height() : width();
  for(int i=0;i<4;i++) if(i!=e) holding[i]=false;
  holding[e]=true;
  pulled[e]=qMin(pulled[e]+pixels,extent);
  level[e]=qMin(0.9,pulled[e]/(extent*0.35));
  start();
  update();
}

void edgeGlow::letGo()
{
  for(int i=0;i<4;i++)
    {
      holding[i]=false;
      pulled[i]=0;
    }
  start();
}

void edgeGlow::absorb(Edge e,double pixelsPerSecond)
{
  level[e]=qMax(level[e],qBound(0.25,pixelsPerSecond/4000.0,0.8));
  holding[e]=false;
  start();
  update();
}

void edgeGlow::tick()
{
  const qint64 now=clock.elapsed();
  const double dt=qBound<qint64>(1,now-lastTick,qint64(100))/1000.0;
  lastTick=now;
  bool any=false;
  for(int i=0;i<4;i++)
    {
      if(holding[i]) {any=true;continue;}
      if(level[i]>0)
        {
          level[i]=qMax(0.0,level[i]-dt*2.0);   // gone in about half a second
          any=any || level[i]>0;
        }
    }
  update();
  if(!any) timer.stop();
}

// a fling that arrives at the end: how fast the scroll bar was moving when it got there
void edgeGlow::scrolled(Qt::Orientation orientation,int value)
{
  const int axis=orientation==Qt::Vertical ? 0 : 1;
  QScrollBar *bar=orientation==Qt::Vertical ? area->verticalScrollBar() : area->horizontalScrollBar();
  const qint64 now=clock.elapsed();
  const qint64 dt=now-lastTime[axis];
  if(dt>0 && dt<150) speed[axis]=0.5*speed[axis]+0.5*(value-lastValue[axis])*1000.0/dt;
  else speed[axis]=0;
  const int before=lastValue[axis];
  lastValue[axis]=value;
  lastTime[axis]=now;
  if(value==before || area->property("fingerDown").toBool()) return;   // a finger on it: touchFlick pulls the glow itself
  const double v=speed[axis];
  if(value>=bar->maximum() && v>400) absorb(orientation==Qt::Vertical ? Bottom : Right,v);
  else if(value<=bar->minimum() && v<-400) absorb(orientation==Qt::Vertical ? Top : Left,-v);
}

void edgeGlow::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  QColor base=palette().color(QPalette::Highlight);
  for(int e=0;e<4;e++)
    {
      const double s=level[e];
      if(s<=0.002) continue;
      const bool vertical=(e==Top || e==Bottom);
      const double along=vertical ? width() : height();      // the length of the edge
      const double into=vertical ? height() : width();       // the way into the content
      const double rx=along*0.8;
      const double ry=(20+0.1*into)*s;
      p.save();
      // a frame with x along the edge and y into the content
      switch(e)
        {
        case Top:    p.translate(width()/2.0,0); break;
        case Bottom: p.translate(width()/2.0,height()); p.rotate(180); break;
        case Left:   p.translate(0,height()/2.0); p.rotate(-90); break;
        default:     p.translate(width(),height()/2.0); p.rotate(90); break;
        }
      p.scale(1.0,ry/rx);
      QRadialGradient g(0,0,rx);
      QColor c=base;
      c.setAlphaF(qMin(1.0,0.6*s));
      g.setColorAt(0,c);
      c.setAlphaF(0);
      g.setColorAt(1,c);
      p.setBrush(g);
      p.drawRect(QRectF(-rx,0,2*rx,rx));
      p.restore();
    }
}
