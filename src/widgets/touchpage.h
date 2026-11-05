#ifndef TOUCHPAGE_H
#define TOUCHPAGE_H

#include <QProxyStyle>
#include <QList>
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
