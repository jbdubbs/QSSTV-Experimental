#ifndef APPIMAGEINTEGRATION_H
#define APPIMAGEINTEGRATION_H

#include <QString>
#include <QUrl>

/*!
  Desktop integration for the AppImage build: adds/removes the application-menu entry, the icons and the AppStream
  metadata in the user's XDG data directory (an AppImage cannot do this by itself). Everything is a no-op unless
  running from an AppImage (APPIMAGE and APPDIR are set by its runtime).
*/
namespace appImageIntegration
{
  bool available();                 //!< running from an AppImage
  bool isInstalled();               //!< our menu entry exists in the user's applications directory
  bool isCurrent();                 //!< installed entry points at the AppImage that is running now
  QString installedTarget();        //!< AppImage path the installed entry launches (empty if none)
  bool installedByOtherTool();      //!< AppImageLauncher / appimaged already integrated this AppImage
  bool install(QString &message);   //!< also removes entries left by any other release
  // uninstall removes the entries, icons and metadata of any QSSTV-Experimental release, not just this one
  bool uninstall(QString &message);
  /*!
    Update check through appimageupdatetool (needs the embedded update information).
    Returns 0: up to date, 1: update available, -1: could not check (tool missing/failed).
  */
  int checkForUpdate(QString &message);
  bool openUrl(const QUrl &url);    //!< open in the default browser; from an AppImage with the bundled-library environment removed
  bool runUpdate();                 //!< start appimageupdatetool detached; it replaces the AppImage file
}

#endif
