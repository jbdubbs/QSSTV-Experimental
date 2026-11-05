#include "soundbackend.h"

#include <QtGlobal>   // Q_OS_ANDROID

#ifdef Q_OS_ANDROID
#include "soundandroid.h"
#else
#include "soundqtmultimedia.h"
#endif

soundBase *newSoundBackend()
{
#ifdef Q_OS_ANDROID
  return new soundAndroid;
#else
  return new soundQtMultimedia;
#endif
}
