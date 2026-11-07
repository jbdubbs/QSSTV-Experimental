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
  Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY themeChanged)                    //!< 0 follow the phone, 1 light, 2 dark, 3 dim, 4 night (red on black)
  Q_PROPERTY(bool tintPictures READ tintPictures WRITE setTintPictures NOTIFY tintPicturesChanged)   //!< in Night the pictures are red too
  Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged)           //!< the phone's "Remove animations" is on: nothing slides or fades
  Q_PROPERTY(QString routeText READ routeText NOTIFY routeChanged)                        //!< "Phone speaker", "USB: Digirig"
  Q_PROPERTY(qreal fontScale READ fontScale NOTIFY fontScaleChanged)                      //!< the system's text size setting, 1 = normal
  Q_PROPERTY(int mediaVolume READ mediaVolume WRITE setMediaVolume NOTIFY mediaVolumeChanged)   //!< the phone's media volume, 0 to 100: how loud a picture is sent
  Q_PROPERTY(bool mediaVolumeAvailable READ mediaVolumeAvailable CONSTANT)                    //!< there is a media volume to set (Android)
  Q_PROPERTY(QString versionText READ versionText CONSTANT)                                   //!< "10.1.1"
  Q_PROPERTY(bool startListening READ startListening CONSTANT)                            //!< the receiver starts with the program
  Q_PROPERTY(bool firstRun READ firstRun NOTIFY firstRunChanged)                       //!< the first start: the short setup shows
  Q_PROPERTY(bool micDenied READ micDenied NOTIFY micDeniedChanged)                     //!< the microphone permission was refused: the receiver cannot listen
  Q_PROPERTY(bool settingsOpen READ settingsOpen WRITE setSettingsOpen NOTIFY settingsOpenChanged)   //!< the settings pages are showing

public:
  static AppModel *instance();
  static AppModel *create(QQmlEngine *,QJSEngine *);

  int screen() const {return m_screen;}
  void setScreen(int s);
  bool digital() const;
  void setDigital(bool on);
  int theme() const {return m_theme;}
  void setTheme(int t);
  bool tintPictures() const {return m_tintPictures;}
  void setTintPictures(bool on);
  bool reducedMotion() const {return m_reducedMotion;}
  void setReducedMotion(bool on);   //!< for the tests
  QString routeText() const;
  qreal fontScale() const {return m_fontScale;}
  int mediaVolume() const;
  void setMediaVolume(int percent);
  bool mediaVolumeAvailable() const;
  bool startListening() const {return true;}
  QString versionText() const;
  bool firstRun() const {return m_firstRun;}
  bool micDenied() const {return m_micDenied;}
  void setMicDenied(bool d);
  void setFirstRun(bool on);   //!< for the tests
  bool settingsOpen() const {return m_settingsOpen;}
  void setSettingsOpen(bool on);

  //! the program's own analog / digital mode changed, whoever changed it
  void modeChanged() {emit digitalChanged();}

  //! Back was pressed on the first screen with nothing open: the app goes to the background and keeps receiving
  //! look at the media volume again (the volume keys change it behind our back); a Timer on the screen that shows it calls this
  Q_INVOKABLE void refreshVolume();
  //! a short vibration (0 a tick, 1 a confirmation, 2 a refusal), where the phone has one
  Q_INVOKABLE void haptic(int kind);
  //! the icons of the system bars: dark ones where the screen under them is light
  Q_INVOKABLE void systemBarIcons(bool dark);
  //! the screen stays on (true) while a picture is sent
  Q_INVOKABLE void keepScreenOn(bool on);
  //! the system's settings of this app (permissions): where a refused permission is allowed again
  Q_INVOKABLE void openAppSettings();
  //! the licenses of the libraries the program uses (THIRD_PARTY_LICENSES.md, compiled in)
  Q_INVOKABLE QString licensesText() const;
  //! the User's Guide, in the browser
  Q_INVOKABLE void openHelp();
  //! the setup is over (or skipped): it does not come again
  Q_INVOKABLE void finishFirstRun();
  //! the system asks for the microphone (once): the setup does it when it comes to the sound
  Q_INVOKABLE void askMicrophone();
  //! the settings pages
  Q_INVOKABLE void openSettings();
  //! the classic Configuration dialog (Widgets), for what the new pages do not have yet
  Q_INVOKABLE void openClassicSettings();
  Q_INVOKABLE void moveToBackground();
  //! the window was closed (not Android): the program ends like the menu's Exit does
  Q_INVOKABLE void quit();

signals:
  void screenChanged();
  void digitalChanged();
  void routeChanged();
  void fontScaleChanged();
  void themeChanged();
  void tintPicturesChanged();
  void reducedMotionChanged();
  void mediaVolumeChanged();
  void settingsOpenChanged();
  void micDeniedChanged();
  void firstRunChanged();
  void backInvoked();                                              //!< the system's Back (Android 13 and later): one step back, or to the home screen
  void screenRequested(int screen);                                //!< a launcher shortcut: 0 Receive, 1 Transmit, 2 Gallery
  void noticeShown(const QString &text);                           //!< a short message for the snackbar
  void problemShown(const QString &title,const QString &text);   //!< a dialog with this text

private:
  explicit AppModel(QObject *parent=nullptr);
  void checkShortcut();
  static bool systemAnimationsRemoved();
  int m_screen=0;
  int m_theme=0;
  bool m_tintPictures=false;
  bool m_reducedMotion=false;
  qreal m_fontScale=1.0;
  int m_volume=70;
  bool m_settingsOpen=false;
  bool m_micDenied=false;
  bool m_firstRun=false;
};

#endif // APPMODEL_H
