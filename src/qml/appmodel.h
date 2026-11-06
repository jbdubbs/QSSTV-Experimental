#ifndef APPMODEL_H
#define APPMODEL_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/*!
  What the whole Qt Quick interface shares: which screen is showing, the analog / digital (SSTV / DRM) mode that Receive and Transmit
  have in common, the colour scheme and the text of the route chip. A singleton for QML ("AppModel").
*/
class AppModel : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int screen READ screen WRITE setScreen NOTIFY screenChanged)                 //!< 0 Receive, 1 Transmit, 2 Gallery
  Q_PROPERTY(bool digital READ digital WRITE setDigital NOTIFY digitalChanged)            //!< DRM instead of analog SSTV, for both screens
  Q_PROPERTY(int colorScheme READ colorScheme CONSTANT)                                   //!< 0 follow the system, 1 light, 2 dark
  Q_PROPERTY(QString routeText READ routeText NOTIFY routeChanged)                        //!< "Phone speaker", "USB: Digirig"
  Q_PROPERTY(qreal fontScale READ fontScale NOTIFY fontScaleChanged)                      //!< the system's text size setting, 1 = normal
  Q_PROPERTY(bool startListening READ startListening CONSTANT)                            //!< the receiver starts with the program

public:
  static AppModel *instance();
  static AppModel *create(QQmlEngine *,QJSEngine *);

  int screen() const {return m_screen;}
  void setScreen(int s);
  bool digital() const;
  void setDigital(bool on);
  int colorScheme() const {return m_colorScheme;}
  QString routeText() const;
  qreal fontScale() const {return m_fontScale;}
  bool startListening() const {return true;}

  //! the program's own analog / digital mode changed, whoever changed it
  void modeChanged() {emit digitalChanged();}

  //! Back was pressed on the first screen with nothing open: the app goes to the background and keeps receiving
  Q_INVOKABLE void moveToBackground();
  //! the window was closed (not Android): the program ends like the menu's Exit does
  Q_INVOKABLE void quit();

signals:
  void screenChanged();
  void digitalChanged();
  void routeChanged();
  void fontScaleChanged();

private:
  explicit AppModel(QObject *parent=nullptr);
  int m_screen=0;
  int m_colorScheme=0;
  qreal m_fontScale=1.0;
};

#endif // APPMODEL_H
