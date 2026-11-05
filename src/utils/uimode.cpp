#include "uimode.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>
#include <QtGlobal>

namespace uiMode
{

bool touch()
{
#ifdef Q_OS_ANDROID
  return true;
#else
  static const bool t=[]
    {
      const QByteArray v=qgetenv("QSSTV_UI");
      return v=="phone" || v=="tablet";
    }();
  return t;
#endif
}

bool narrow(int width)
{
  return width<narrowWidth;
}

bool narrow(const QWidget *w)
{
  return w && narrow(availableWidth(w));
}

int availableWidth(const QWidget *w)
{
  int width=w->width();
  if(const QScreen *s=w->screen()) width=qMin(width,s->availableGeometry().width());
  bool ok=false;
  const int imitated=qEnvironmentVariableIntValue("QSSTV_UI_WIDTH",&ok);
  if(ok && touch() && imitated>0) width=qMin(width,imitated);
  return width;
}

int shortSide(const QWidget *w)
{
  bool ok=false;
  const int imitated=qEnvironmentVariableIntValue("QSSTV_UI_SHORT",&ok);
  if(ok && touch() && imitated>0) return imitated;
  if(const QScreen *s=w ? w->screen() : QGuiApplication::primaryScreen()) return qMin(s->size().width(),s->size().height());
  return 360;
}

}
