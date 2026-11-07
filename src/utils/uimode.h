#ifndef UIMODE_H
#define UIMODE_H

#include <QString>
#include <QtGlobal>

class QWidget;

/*!
  Which user interface the program shows. The desktop has the Widgets one (menu bar, tabs at the top, the status bar with the operating
  buttons). Android has the Qt Quick one (src/qml), which is also what QSSTV_UI=qml shows on a desktop built with QSSTV_QML_UI=ON; the
  Widgets pages are then still built, never shown, because they hold the receiver and transmitter state the Qt Quick screens read.

  touch() is also true on Android for the one Widgets window the touch interface still opens: the classic Configuration dialog (the
  entries of Settings > Advanced that have no page of their own yet), which lists its sections, reflows them for a narrow screen and scrolls
  with a finger. On the desktop QSSTV_UI=phone or tablet shows the dialog that way (tests/uishots).
*/
namespace uiMode
{
  //! below this many device independent pixels (Qt's logical pixels on Android) a page uses one column
  constexpr int narrowWidth=600;

  //! the touch shell is in use
  bool touch();

  /*!
    The touch shell is the Qt Quick one (src/qml) instead of the Widgets one: Android builds made with QSSTV_QML_UI, and on the
    desktop QSSTV_UI=qml (a development build with -DQSSTV_QML_UI=ON). The Widgets pages are then still built, but never shown: they
    hold the receiver and transmitter state the Qt Quick screens read.
  */
  bool qml();

  //! the width a page has, or is about to be given, is a narrow one
  bool narrow(int width);
  bool narrow(const QWidget *w);

  /*!
    The width to arrange a window for: the smaller of its own width and its screen's. A window that opens wider than the screen
    (its pages cannot get narrower, until they are arranged for a narrow screen) must not be taken for a wide one. With
    QSSTV_UI set, the environment variable QSSTV_UI_WIDTH stands in for the screen, which is how a phone is imitated on the desktop.
  */
  int availableWidth(const QWidget *w);

  /*!
    The shorter side of the screen: what a window is as wide as when the phone is held upright, whichever way it is held now.
    What is arranged once for a narrow screen (the pages of Settings) uses this, so turning the phone does not change it.
    With QSSTV_UI set, QSSTV_UI_SHORT stands in for it.
  */
  int shortSide(const QWidget *w);

  //! the smallest comfortable size of a thing to tap, in logical pixels (48 dp)
  constexpr int tapSize=48;
}

/*!
  A window with levels of its own (Settings: the list of sections, and a section) that takes the Back key before it is closed:
  backPressed() returns true when it went up one level instead.
*/
class backAware
{
public:
  virtual ~backAware() = default;
  virtual bool backPressed() = 0;
};

/*!
  A widget that tells the page scrolling of the touch shell (touchFlick, touchpage.h) what a finger on it should do: "pass" (the
  widget has the press, the page takes the drag), "skip" (the widget keeps the drag) or "" (the usual: the press waits for the
  finger to be lifted or to move).
*/
class flickAware
{
public:
  virtual ~flickAware() = default;
  virtual QString touchFlickMode() const = 0;
};

#endif // UIMODE_H
