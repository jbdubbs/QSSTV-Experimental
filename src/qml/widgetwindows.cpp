#include "widgetwindows.h"
#include "appglobal.h"
#include "mainwindow.h"
#include "touchpage.h"
#include "uimode.h"

#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QEvent>
#include <QKeyEvent>
#include <QWindow>

// the Widgets window that is in front of the Qt Quick one, if any
static QWidget *frontWindow()
{
  QWidget *front=QApplication::activePopupWidget();
  if(!front) front=QApplication::activeModalWidget();
  if(!front)
    {
      QWidget *w=QApplication::activeWindow();
      if(w && w!=mainWindowPtr) front=w;
    }
  return front;
}

bool widgetWindows::back()
{
  QWidget *front=frontWindow();
  if(!front) return false;
  static QElapsedTimer last;   // some phones deliver a Back gesture twice in quick succession
  if(!last.isValid() || last.elapsed()>=250)
    {
      last.restart();
      backAware *levels=dynamic_cast<backAware *>(front);
      if(!levels || !levels->backPressed())
        {
          if(QDialog *d=qobject_cast<QDialog *>(front)) d->reject();   // reject(), not close(): a message box without a cancel button ignores close
          else front->close();
        }
    }
  return true;
}

bool widgetWindows::eventFilter(QObject *o,QEvent *e)
{
  if((e->type()==QEvent::KeyPress || e->type()==QEvent::KeyRelease) && static_cast<QKeyEvent *>(e)->key()==Qt::Key_Back)
    {
      // a Widgets window in front takes the Back key; the Qt Quick window gets it back untouched
      const bool press=e->type()==QEvent::KeyPress && !static_cast<QKeyEvent *>(e)->isAutoRepeat();
      if(press ? back() : frontWindow()!=nullptr)   // the release (and the repeats) of the key are taken too
        {
          e->accept();
          return true;
        }
    }
  if(e->type()==QEvent::Polish && o->isWidgetType()) prepareTouchWindow(static_cast<QWidget *>(o));
  if(e->type()==QEvent::Show && o->isWidgetType())
    {
      QWidget *w=static_cast<QWidget *>(o);
      if(w->isWindow() && w->property("touchMaximized").toBool() && w->windowHandle() && !w->property("safeAreaSet").toBool())
        {
          w->setProperty("safeAreaSet",true);
          w->setContentsMargins(w->windowHandle()->safeAreaMargins());
          connect(w->windowHandle(),&QWindow::safeAreaMarginsChanged,w,[w]{w->setContentsMargins(w->windowHandle()->safeAreaMargins());});
        }
    }
  return QObject::eventFilter(o,e);
}
