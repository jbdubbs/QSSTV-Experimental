#ifndef GUIDESERVER_H
#define GUIDESERVER_H

#include <QObject>
#include <QString>
#include <QUrl>

class QTcpServer;
class QTcpSocket;

/*!
  Tiny read-only static web server on 127.0.0.1 used to show the bundled User's Guide.
  Handing a file:// URL to xdg-open fails with sandboxed (Flatpak/Snap) browsers: the desktop portal
  exposes only that one file, so every link to a sibling page or image is a 404. Over http the browser
  needs no filesystem access at all. Only the guide directory is served, and only to the local machine.
*/
class guideServer : public QObject
{
  Q_OBJECT
public:
  static guideServer &instance();
  //! Start serving \a rootDir (if not already) and return the URL of its index.html; empty URL on failure.
  QUrl urlFor(const QString &rootDir);

private slots:
  void slotNewConnection();

private:
  explicit guideServer(QObject *parent=nullptr);
  void respond(QTcpSocket *sock);
  QTcpServer *server;
  QString root;
};

#endif // GUIDESERVER_H
