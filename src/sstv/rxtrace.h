#ifndef RXTRACE_H
#define RXTRACE_H

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <cstdio>

/**
 * Receiver diagnostic (issue #135): with QSSTV_RX_TRACE=<file> in the environment, the sync processor and the receiver
 * write one tab-separated line per event (UTC time, event, fields) to that file: pictures detected, display freezes,
 * resynchronisations, sync lost, pictures saved or discarded, and a minute-by-minute count of sync-like pulses. Without the
 * variable nothing is written and the cost is one test of a flag.
 */

inline FILE *rxTraceFile()
{
  static FILE *f=[]() -> FILE *
  {
    QByteArray path=qgetenv("QSSTV_RX_TRACE");
    return path.isEmpty() ? nullptr : fopen(path.constData(),"a");
  }();
  return f;
}

inline bool rxTraceOn()
{
  return rxTraceFile()!=nullptr;
}

inline void rxTrace(const char *event,const QString &fields)
{
  FILE *f=rxTraceFile();
  if(!f) return;
  fprintf(f,"%s\t%s\t%s\n",qPrintable(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd'T'HH:mm:ss.zzz")),event,qPrintable(fields));
  fflush(f);
}

#endif // RXTRACE_H
