#ifndef PLATFORMPERMISSIONS_H
#define PLATFORMPERMISSIONS_H

#include <functional>
#include <QObject>

/*!
  Runtime permissions. Only Android makes the user grant the microphone (and, later, the camera and the location) while
  the program runs; on every other platform each of these is granted without asking (macOS keeps asking by itself the
  first time the sound card is opened, through Info.plist).
*/
namespace platformPermissions
{
  bool microphoneGranted();   //!< true when sound can be captured (always, except on Android before the user said yes)
  bool microphoneDenied();    //!< Android: the user said no; the system will not ask again, only the app settings can undo it
  /*!
    Ask for the microphone and call done(granted) in the GUI thread, with context as the receiver. Calls done(true) at once
    when nothing needs to be asked. The system's own dialog is shown on Android.
  */
  void requestMicrophone(QObject *context,const std::function<void(bool)> &done);
}

#endif // PLATFORMPERMISSIONS_H
