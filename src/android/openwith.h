#ifndef OPENWITH_H
#define OPENWITH_H

#include <QObject>
#include <QStringList>
#include <QUrl>

/*!
  "Open with QSSTV-Experimental" for an audio recording on Android (AndroidManifest.xml has the VIEW intent filter). Qt forwards
  the intent's URL to QDesktopServices::openUrl(), so this registers as the handler of the file and content schemes. A recording
  is decoded like one chosen under Decode from file; any other file URL is handed on to the system as before. A URL that arrives
  while the program is still starting (a cold start from the intent) waits until windowIsUp().
*/
class openWith : public QObject
{
  Q_OBJECT
public:
  explicit openWith(QObject *parent=nullptr);
  ~openWith();
  void windowIsUp();
public slots:
  void open(const QUrl &url);
private:
  void decode(const QString &what);
  QStringList pending;
  bool ready;
};

#endif // OPENWITH_H
