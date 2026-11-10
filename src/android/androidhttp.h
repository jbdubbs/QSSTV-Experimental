#ifndef ANDROIDHTTP_H
#define ANDROIDHTTP_H

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <functional>

// One HTTPS GET through the system's own HttpsURLConnection (packaging/android/src/.../HttpHelper.java): the system's certificates, and no OpenSSL
// in the package (Qt's Android build has no TLS library to load, so QNetworkAccessManager cannot make https requests there).

//! The answer: the HTTP status and the body, or status 0 and the reason (error "timeout" when there was no answer in time)
using androidHttpDone=std::function<void(int status,const QByteArray &body,const QString &error)>;

//! Starts the request. \a done is called once on the GUI thread, never before this returns. The token tells the request apart for androidHttpCancel().
qint64 androidHttpGet(const QUrl &url,const QByteArray &userAgent,int timeoutMs,androidHttpDone done);

//! The request is no longer wanted: its answer, if it still comes, is dropped
void androidHttpCancel(qint64 token);

#endif // ANDROIDHTTP_H
