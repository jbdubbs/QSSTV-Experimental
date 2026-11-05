#ifndef APPLOG_H
#define APPLOG_H

#include <QtGlobal>

/*!
  A line in the Android log (`adb logcat -s qsstv-tx qsstv-audio`), for what has to be known about a transmission on a phone
  while the program's own log (addToLog) is compiled out of release builds. Nothing elsewhere.
*/
#ifdef Q_OS_ANDROID
#include <android/log.h>
#define APPLOG(tag,...) __android_log_print(ANDROID_LOG_INFO,tag,__VA_ARGS__)
#else
#define APPLOG(tag,...) ((void)0)
#endif

#endif // APPLOG_H
