#include "platformpermissions.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QPermissions>
#endif

namespace platformPermissions
{

#ifdef Q_OS_ANDROID

bool microphoneGranted()
{
  return qApp->checkPermission(QMicrophonePermission{})==Qt::PermissionStatus::Granted;
}

bool microphoneDenied()
{
  return qApp->checkPermission(QMicrophonePermission{})==Qt::PermissionStatus::Denied;
}

void requestMicrophone(QObject *context,const std::function<void(bool)> &done)
{
  qApp->requestPermission(QMicrophonePermission{},context,[done](const QPermission &p)
    {
      done(p.status()==Qt::PermissionStatus::Granted);
    });
}

#else

bool microphoneGranted() {return true;}
bool microphoneDenied() {return false;}
void requestMicrophone(QObject *,const std::function<void(bool)> &done) {done(true);}

#endif

}
