// The loopback web server that shows the User's Guide (src/utils/guideserver.cpp), serving from a Qt resource tree as it does on
// Android, where the guide is compiled into the app: pages and images are served, a missing file is a 404, and a path that tries
// to leave the guide folder gets nothing (outside.txt is in the resources too, one level up, to be found if it did).
#include "guideserver.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTcpSocket>
#include <QThread>
#include <cstdio>

static int fails=0;
#define CHECK(cond,what) do{ if(cond) printf("  ok   %s\n",what); else {printf("  FAIL %s\n",what);fails++;} }while(0)

// one request and the whole reply (the server closes the connection)
class client : public QThread
{
public:
  quint16 port=0;
  QByteArray target,method="GET",reply;
protected:
  void run() override
  {
    QTcpSocket s;
    s.connectToHost("127.0.0.1",port);
    if(!s.waitForConnected(3000)) {reply="no connection";return;}
    s.write(method+" "+target+" HTTP/1.1\r\nHost: localhost\r\n\r\n");
    while(s.state()!=QAbstractSocket::UnconnectedState)
      {
        if(!s.waitForReadyRead(3000)) break;
        reply+=s.readAll();
      }
    reply+=s.readAll();
  }
};

int main(int argc,char **argv)
{
  QCoreApplication app(argc,argv);
  const QUrl url=guideServer::instance().urlFor(":/guide");
  CHECK(url.isValid() && !url.isEmpty(),"the server starts on a resource folder");
  CHECK(url.path()=="/index.html","the guide's address is its index page");
  const quint16 port=quint16(url.port());
  // the server lives in this thread, the client blocks in its own
  auto ask=[&](const QByteArray &target,const QByteArray &method=QByteArray("GET"))
    {
      client c;
      c.port=port; c.target=target; c.method=method;
      QEventLoop loop;
      QObject::connect(&c,&QThread::finished,&loop,&QEventLoop::quit);
      c.start();
      loop.exec();
      return c.reply;
    };
  QByteArray r=ask("/index.html");
  CHECK(r.startsWith("HTTP/1.1 200") && r.contains("text/html") && r.contains("Guide index"),"the index page is served");
  r=ask("/");
  CHECK(r.startsWith("HTTP/1.1 200") && r.contains("Guide index"),"a folder address gives its index.html");
  r=ask("/sub/page.html");
  CHECK(r.startsWith("HTTP/1.1 200") && r.contains("A page in a folder"),"a page in a folder is served");
  r=ask("/pic.png");
  CHECK(r.startsWith("HTTP/1.1 200") && r.contains("image/png"),"an image is served with its type");
  r=ask("/missing.html");
  CHECK(r.startsWith("HTTP/1.1 404"),"a file that is not there is a 404");
  r=ask("/../outside.txt");
  CHECK(r.startsWith("HTTP/1.1 404") && !r.contains("secret"),"a path that climbs out of the folder is a 404");
  r=ask("/%2e%2e/outside.txt");
  CHECK(r.startsWith("HTTP/1.1 404") && !r.contains("secret"),"the same with the dots percent-encoded");
  r=ask("/sub/../../outside.txt");
  CHECK(r.startsWith("HTTP/1.1 404") && !r.contains("secret"),"and when the climb starts in a subfolder");
  r=ask("/index.html","HEAD");
  CHECK(r.startsWith("HTTP/1.1 200") && !r.contains("Guide index"),"HEAD answers without the body");
  r=ask("/index.html","POST");
  CHECK(r.startsWith("HTTP/1.1 405"),"anything but GET and HEAD is refused");
  printf("\n%s\n",fails ? "FAILED" : "all checks passed");
  return fails ? 1 : 0;
}
