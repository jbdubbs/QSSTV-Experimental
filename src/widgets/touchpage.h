#ifndef TOUCHPAGE_H
#define TOUCHPAGE_H

#include <QPalette>
#include <QProxyStyle>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QAbstractScrollArea>
#include <QScrollArea>
#include <QWidget>
#include <functional>

class QVBoxLayout;
class QTabWidget;
class QHBoxLayout;
class QPushButton;

/*!
  Building blocks of the touch shell (uimode.h): a page that scrolls with a finger, and the More page.
*/

/*!
  \a content in a scroll area that follows a finger (kinetic scrolling), vertically only: the content is made as wide as the
  area, so a page that is narrow enough never needs to scroll sideways.
*/
QScrollArea *makeTouchScrollPage(QWidget *content,QWidget *parent=nullptr);

/*!
  The last tab of the touch shell: what the menu bar holds on the desktop, as a list of large buttons.
*/
class morePage : public QScrollArea
{
  Q_OBJECT
public:
  explicit morePage(QWidget *parent=nullptr);
  //! a full width button; \a action runs when it is tapped
  void addButton(const QString &text,const std::function<void()> &action);
  //! a small heading above the buttons that follow
  void addHeading(const QString &text);
private:
  QVBoxLayout *box;
};

/*!
  The style of the touch shell: the program's own style (Fusion), with the controls made big enough for a finger (44 dp high,
  indicators of checkboxes and radio buttons 24 dp). A style sheet would do the same but restyles every widget, a proxy style
  only changes sizes.
*/
class touchStyle : public QProxyStyle
{
public:
  explicit touchStyle(const QString &baseStyle) : QProxyStyle(baseStyle) {}
  QSize sizeFromContents(ContentsType type,const QStyleOption *option,const QSize &size,const QWidget *widget) const override;
  int pixelMetric(PixelMetric metric,const QStyleOption *option=nullptr,const QWidget *widget=nullptr) const override;
};

/*!
  The palette of the touch shell, light or dark by the system's setting, complete (Qt on Android fills only a part of the roles
  for a dark system, the rest stays light: dark text on a dark box). Set again when the system changes between light and dark.
*/
QPalette touchPalette(bool dark);
void applyTouchPalette();

/*!
  What the touch shell's scroll areas show at the end of the content, as Android's own do (before Android 12): the content stops
  there and a soft glow in the highlight colour comes up along the edge and fades. It comes up as the finger keeps pulling on
  (pull()), or when a fling reaches the end (absorb(), found from the scroll bars: a fling that arrives with speed), and goes
  when the finger is lifted or turns back (letGo()). The content never moves past its end: nothing bounces back.
*/
class edgeGlow : public QWidget
{
  Q_OBJECT
public:
  enum Edge {Top,Bottom,Left,Right};
  explicit edgeGlow(QAbstractScrollArea *area);
  //! the finger moves on past \a edge by \a pixels more
  void pull(Edge edge,double pixels);
  //! the finger is lifted or has turned back: what is showing fades
  void letGo();
  //! a fling arrived at \a edge at \a pixelsPerSecond
  void absorb(Edge edge,double pixelsPerSecond);
  //! the finger was lifted with a flick: when it comes to rest at the end it was heading for, the glow shows it (\a velocity: m/s, QScroller's)
  void flung(const QPointF &velocity);
  //! 0 to 1: how much of the glow shows at the edge (tests/uishots)
  double strength(Edge edge) const {return level[edge];}
protected:
  void paintEvent(QPaintEvent *e) override;
  bool eventFilter(QObject *o,QEvent *e) override;
private:
  void tick();
  void scrolled(Qt::Orientation orientation,int value);
  void start();
  QAbstractScrollArea *area;
  double level[4]={0,0,0,0};
  double pulled[4]={0,0,0,0};
  bool holding[4]={false,false,false,false};
  QTimer timer;
  QElapsedTimer clock;
  qint64 lastTick=0;
  int lastValue[2]={0,0};
  qint64 lastTime[2]={0,0};
  double speed[2]={0,0};
  int flingEdge=-1;          // the end a flick is heading for, or -1
  double flingSpeed=0;
};

/*!
  A scroll area for a finger: scrolled by touchFlick (a press on any widget in it), no rubber band at the ends (the content stops
  and an edgeGlow shows it), a flick that slows down like Android's. \a sideways: also in the horizontal direction.
*/
void setupTouchScrolling(QAbstractScrollArea *area,bool sideways);

/*!
  Finger scrolling for the scroll areas made by makeTouchScrollPage(), over every widget in them: a press on a button, a box or
  a label is held back until it is clear whether it is a tap (then it is delivered, as a press and a release) or the start of a
  drag (then the page scrolls). Sliders, scroll bars, text areas and lists keep their own drags; a picture (property touchFlick
  = "pass") gets its press at once and loses the drag to the page when the finger moves; "skip" keeps all of it.
  Installed once on the application.
*/
class touchFlick : public QObject
{
  Q_OBJECT
public:
  explicit touchFlick(QObject *parent=nullptr) : QObject(parent) {}
  bool eventFilter(QObject *o,QEvent *e) override;
private:
  QAbstractScrollArea *areaOf(QWidget *w,bool &passive) const;
  QPointer<QWidget> target;
  QPointer<QAbstractScrollArea> area;
  QPointF lastGlobal;
  QPointF pressLocal;
  QPointF pressGlobal;
  bool passive=false;
  bool dragging=false;
  const QEvent *replayEvent=nullptr;   // the press or release being sent again
};

/*!
  Called for every window of its own that is about to be shown (an application wide event filter of the touch shell): a dialog
  that would fill most of the screen is maximized, and one that is maximized keeps clear of the system bars Android draws over it.
*/
void prepareTouchWindow(QWidget *w);

/*!
  \a content in a scroll area that follows a finger in both directions, for a page that is wider than the screen and has no
  arrangement for a narrow one yet (a page of Settings).
*/
QScrollArea *makeTouchScrollPage2D(QWidget *content,QWidget *parent=nullptr);

/*!
  The bottom navigation of the touch shell: one large button per page of \a tabs, sharing the width equally. The tab bar of the
  tab widget is hidden; the widget keeps the pages, and the buttons follow it when the page is changed by the program.
  (A style sheet on a QTabBar stops it from expanding over the width, so the buttons are the bar.)
*/
class bottomNav : public QWidget
{
  Q_OBJECT
public:
  explicit bottomNav(QTabWidget *tabs,QWidget *parent=nullptr);
  //! a column of buttons for the side of the window (a phone on its side has no height to spare) instead of a row
  void setRail(bool rail);
private:
  QHBoxLayout *row;
  QList<QPushButton *> buttons;
  static QString styleSheet(bool rail);
};

#endif // TOUCHPAGE_H
