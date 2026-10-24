// A web server for the tests of the place lookup: it answers every request with what the test says and keeps the request it saw,
// so the lookup can be driven without the internet (the lookup is pointed at url() with placeLookup::setServiceUrl()).
#ifndef FAKESERVICE_H
#define FAKESERVICE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <memory>

// a web server that answers every request with what the test says; it keeps the request it saw
struct fakeService
{
  QTcpServer server;
  QObject owner;
  int status=200;
  QByteArray body;
  bool silent=false;        //!< accept the request and never answer
  int delayMs=0;            //!< answer this long after the request
  int hits=0;
  QByteArray lastRequest;

  fakeService()
  {
    server.listen(QHostAddress::LocalHost,0);
    QObject::connect(&server,&QTcpServer::newConnection,&owner,[this] {
      while(QTcpSocket *s=server.nextPendingConnection())
        {
          auto head=std::make_shared<QByteArray>();
          QObject::connect(s,&QTcpSocket::readyRead,s,[this,s,head] {
            head->append(s->readAll());
            if(!head->contains("\r\n\r\n")) return;
            hits++;
            lastRequest=*head;
            head->clear();
            if(silent) return;
            const QByteArray reply="HTTP/1.1 "+QByteArray::number(status)+" Test\r\nContent-Type: application/json\r\nContent-Length: "+
              QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body;
            QTimer::singleShot(delayMs,s,[s,reply] {
              s->write(reply);
              s->disconnectFromHost();
            });
          });
          QObject::connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
        }
    });
  }
  QUrl url() const {return QUrl(QString("http://127.0.0.1:%1/reverse").arg(server.serverPort()));}
};

static const char *seattle="{\"address\":{\"city\":\"Seattle\",\"state\":\"Washington\",\"ISO3166-2-lvl4\":\"US-WA\",\"country_code\":\"us\"}}";
static const char *missoula="{\"address\":{\"city\":\"Missoula\",\"state\":\"Montana\",\"ISO3166-2-lvl4\":\"US-MT\",\"country_code\":\"us\"}}";

#endif // FAKESERVICE_H
