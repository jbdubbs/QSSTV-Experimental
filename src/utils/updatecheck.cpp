#include "updatecheck.h"
#include "appglobal.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QRegularExpression>
#include <climits>

static const char *apiUrl="https://api.github.com/repos/jbdubbs/QSSTV-Experimental/releases?per_page=10";

updateChecker::updateChecker(QObject *parent):QObject(parent)
{
  nam=new QNetworkAccessManager(this);
  connect(nam,&QNetworkAccessManager::finished,this,&updateChecker::slotReply);
}

QString updateChecker::currentVersion()
{
  return MAJORVERSION+MINORVERSION;
}

QUrl updateChecker::releasesPage()
{
  return QUrl("https://github.com/jbdubbs/QSSTV-Experimental/releases");
}

void updateChecker::start()
{
  QNetworkRequest req{QUrl(apiUrl)};
  req.setRawHeader("User-Agent","QSSTV-Experimental");
  req.setRawHeader("Accept","application/vnd.github+json");
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setTransferTimeout(15000);
  nam->get(req);
}

int updateChecker::compareVersions(const QString &a,const QString &b)
{
  // "<major>.<minor>[-Pre<n>]", optional leading v; a missing -Pre ranks above any -PreN
  auto parse=[](const QString &s,int &maj,int &min,int &pre)
  {
    static const QRegularExpression re("^v?(\\d+)(?:\\.(\\d+))?(-?[Pp]re(\\d*))?");
    QRegularExpressionMatch m=re.match(s.trimmed());
    maj=m.captured(1).toInt();
    min=m.captured(2).toInt();
    pre=m.captured(3).isEmpty()?INT_MAX:m.captured(4).toInt();
  };
  int am,an,ap,bm,bn,bp;
  parse(a,am,an,ap);
  parse(b,bm,bn,bp);
  if(am!=bm) return am<bm?-1:1;
  if(an!=bn) return an<bn?-1:1;
  if(ap!=bp) return ap<bp?-1:1;
  return 0;
}

void updateChecker::slotReply(QNetworkReply *reply)
{
  Result r;
  reply->deleteLater();
  if(reply->error()!=QNetworkReply::NoError)
    {
      r.error=reply->errorString();
      emit finished(r);
      return;
    }
  QJsonParseError pe;
  QJsonDocument doc=QJsonDocument::fromJson(reply->readAll(),&pe);
  if(!doc.isArray())
    {
      r.error=tr("Unexpected reply from GitHub");
      emit finished(r);
      return;
    }
  QDateTime bestTime;
  QJsonObject best;
  const QJsonArray arr=doc.array();
  for(const QJsonValue &v:arr)
    {
      QJsonObject o=v.toObject();
      if(o.value("draft").toBool()) continue;
      QDateTime t=QDateTime::fromString(o.value("published_at").toString(),Qt::ISODate);
      if(best.isEmpty() || t>bestTime) { best=o; bestTime=t; }
    }
  if(best.isEmpty())
    {
      r.error=tr("No releases found");
      emit finished(r);
      return;
    }
  r.ok=true;
  r.latestTag=best.value("tag_name").toString();
  r.pageUrl=QUrl(best.value("html_url").toString());
  if(!r.pageUrl.isValid() || r.pageUrl.isEmpty()) r.pageUrl=releasesPage();
  r.newer=compareVersions(r.latestTag,currentVersion())>0;
  emit finished(r);
}
