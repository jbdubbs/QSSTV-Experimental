#ifndef PLATFORMSERVICE_H
#define PLATFORMSERVICE_H

#include <QtGlobal>

/*!
  The background service of the platform. On Android it is the foreground service that keeps the microphone, the CPU and
  the process alive while the program is not in front (packaging/android/src/.../RxService.java); everywhere else there is
  nothing to do.
*/
namespace platformService
{
  //! true while there is something to receive or send. Cheap to call often: only a change does anything.
  void setActive(bool on);
  //! Android: the Back key with nothing left to close sends the app to the background, as the home key does (the receiver keeps
  //! running in its service); finishing the activity would end it. No-op elsewhere.
  void moveToBackground();
}

#endif // PLATFORMSERVICE_H
