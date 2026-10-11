#include "guideserver.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

guideServer::guideServer(QObject *parent) : QObject(parent), server(nullptr)
{
}

guideServer &guideServer::instance()
{
  static guideServer s;
  return s;
}

QUrl guideServer::urlFor(const QString &rootDir)
{
  const QString canon=QFileInfo(rootDir).canonicalFilePath();
  if(canon.isEmpty()) return QUrl();
  if(server && root!=canon)
    {
      delete server;
      server=nullptr;
    }
  root=canon;
  if(!server)
    {
      server=new QTcpServer(this);
      if(!server->listen(QHostAddress::LocalHost,0))
        {
          delete server;
          server=nullptr;
          return QUrl();
        }
      connect(server,&QTcpServer::newConnection,this,&guideServer::slotNewConnection);
    }
  return QUrl(QString("http://127.0.0.1:%1/index.html").arg(server->serverPort()));
}

void guideServer::slotNewConnection()
{
  while(server->hasPendingConnections())
    {
      QTcpSocket *sock=server->nextPendingConnection();
      connect(sock,&QTcpSocket::disconnected,sock,&QObject::deleteLater);
      connect(sock,&QTcpSocket::readyRead,this,[this,sock](){ respond(sock); });
    }
}

static QByteArray mimeFor(const QString &name)
{
  const QString ext=QFileInfo(name).suffix().toLower();
  if(ext=="html") return "text/html; charset=utf-8";
  if(ext=="css") return "text/css; charset=utf-8";
  if(ext=="png") return "image/png";
  if(ext=="jpg" || ext=="jpeg") return "image/jpeg";
  if(ext=="svg") return "image/svg+xml";
  if(ext=="ico") return "image/x-icon";
  return "application/octet-stream";
}

static void sendReply(QTcpSocket *sock,int code,const char *text,const QByteArray &type,const QByteArray &body,bool head)
{
  QByteArray r="HTTP/1.1 "+QByteArray::number(code)+" "+text+"\r\nContent-Type: "+type+
      "\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\nCache-Control: no-cache\r\n\r\n";
  if(!head) r+=body;
  sock->write(r);
  sock->disconnectFromHost();
}

void guideServer::respond(QTcpSocket *sock)
{
  if(!sock->property("done").isNull()) return;
  const QByteArray req=sock->peek(sock->bytesAvailable());
  if(!req.contains("\r\n\r\n") && req.size()<8192) return; // wait for the full header
  sock->setProperty("done",true);
  const QList<QByteArray> parts=req.left(req.indexOf("\r\n")).split(' ');
  const bool head=(parts.value(0)=="HEAD");
  if(parts.size()<2 || (parts[0]!="GET" && !head))
    {
      sendReply(sock,405,"Method Not Allowed","text/plain","Method not allowed",false);
      return;
    }
  QString path=QUrl::fromPercentEncoding(parts[1].split('?').first().split('#').first());
  if(path.endsWith('/')) path+="index.html";
  // resolve inside root only: canonical path must stay under the guide directory
  const QString file=QFileInfo(root+"/"+path).canonicalFilePath();
  if(file.isEmpty() || !file.startsWith(root+"/") || !QFileInfo(file).isFile())
    {
      sendReply(sock,404,"Not Found","text/plain","Not found",head);
      return;
    }
  QFile f(file);
  if(!f.open(QIODevice::ReadOnly))
    {
      sendReply(sock,404,"Not Found","text/plain","Not found",head);
      return;
    }
  sendReply(sock,200,"OK",mimeFor(file),f.readAll(),head);
}
