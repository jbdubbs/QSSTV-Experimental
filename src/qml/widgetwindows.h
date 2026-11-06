#ifndef WIDGETWINDOWS_H
#define WIDGETWINDOWS_H

#include <QObject>

/*!
  The Widgets windows that still open over the Qt Quick interface (the program's own settings dialog until the new pages are there,
  the file pickers' message boxes, ...): what the Widgets touch shell did for them in mainWindow::eventFilter, which is not
  installed when the Qt Quick one runs. A window that nearly fills the screen is maximized and kept clear of the system bars
  (prepareTouchWindow), and the Back key closes the window in front, or takes a settings dialog one level up (backAware).
  The Back key of the Qt Quick window itself is not touched: it is handled in Main.qml.
*/
class widgetWindows : public QObject
{
  Q_OBJECT
public:
  explicit widgetWindows(QObject *parent=nullptr) : QObject(parent) {}
  bool eventFilter(QObject *o,QEvent *e) override;
};

#endif // WIDGETWINDOWS_H
