#ifndef PLATFORMSERVICE_H
#define PLATFORMSERVICE_H

#include <QString>
#include <QtGlobal>

/*!
  The background service of the platform. On Android it is the foreground service that keeps the microphone, the CPU and
  the process alive while the program is not in front (packaging/android/src/.../RxService.java); everywhere else there is
  nothing to do.
*/
namespace platformService
{
  //! 0 nothing to receive or send, 1 listening, 2 sending a picture, 3 sending a tone or text (detail: what is sent, for the notification). Cheap to call often: only a change does anything.
  void setState(int state,const QString &detail=QString());
  //! true while there is something to receive or send: the same as setState(on ? 1 : 0)
  void setActive(bool on);
  //! Android: the Back key with nothing left to close sends the app to the background, as the home key does (the receiver keeps
  //! running in its service); finishing the activity would end it. No-op elsewhere.
  void moveToBackground();
}

#endif // PLATFORMSERVICE_H
