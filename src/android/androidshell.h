#ifndef ANDROIDSHELL_H
#define ANDROIDSHELL_H

#include <QString>

/*!
  What the program has to do with Android's shell (packaging/android/src/.../MainActivity.java and RxService.java): the Back gesture, the launcher
  shortcuts and the Stop button of the notification. Android builds only.
*/

//! the callbacks the Java side calls are registered (once; cheap to call again)
void androidShellInit();
//! the screen a launcher shortcut asked for ("receive", "transmit", "gallery"), once; empty when there is none
QString androidTakeRequestedScreen();
//! the service and its notification: 0 none, 1 listening, 2 sending a picture, 3 sending a tone or text (with a short line about it)
void androidSetServiceState(int state,const QString &detail);

#endif // ANDROIDSHELL_H
