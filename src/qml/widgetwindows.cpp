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

bool widgetWindows::eventFilter(QObject *o,QEvent *e)
{
  if((e->type()==QEvent::KeyPress || e->type()==QEvent::KeyRelease) && static_cast<QKeyEvent *>(e)->key()==Qt::Key_Back)
    {
      // a Widgets window in front takes the Back key; the Qt Quick window gets it back untouched
      QWidget *front=QApplication::activePopupWidget();
      if(!front) front=QApplication::activeModalWidget();
      if(!front)
        {
          QWidget *w=QApplication::activeWindow();
          if(w && w!=mainWindowPtr) front=w;
        }
      if(front)
        {
          if(e->type()==QEvent::KeyPress && !static_cast<QKeyEvent *>(e)->isAutoRepeat())
            {
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
            }
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
