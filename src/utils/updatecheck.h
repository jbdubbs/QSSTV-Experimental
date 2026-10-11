#ifndef UPDATECHECK_H
#define UPDATECHECK_H

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

/*!
  Update check against the GitHub releases of QSSTV-Experimental (used where the AppImage updater is not
  available, i.e. Windows). It only reports; the user downloads the new build from the release page.
  Pre-releases count, because every published version so far is a 10.0-PreN.
*/
class updateChecker : public QObject
{
  Q_OBJECT
public:
  struct Result
  {
    bool ok=false;          //!< the check itself worked
    bool newer=false;       //!< a release newer than the running version exists
    QString latestTag;
    QUrl pageUrl;           //!< release page
    QString error;
  };
  explicit updateChecker(QObject *parent=nullptr);
  void start();
  /*!
    Compare version strings such as "10.0-Pre8" or "v10.0" (a release outranks any Pre of the same number).
    Returns <0, 0 or >0.
  */
  static int compareVersions(const QString &a,const QString &b);
  static QString currentVersion();
  static QUrl releasesPage();

signals:
  void finished(const updateChecker::Result &result);

private slots:
  void slotReply(QNetworkReply *reply);

private:
  QNetworkAccessManager *nam;
};

#endif
