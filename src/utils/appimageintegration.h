#ifndef APPIMAGEINTEGRATION_H
#define APPIMAGEINTEGRATION_H

#include <QString>

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
  bool installedByOtherTool();      //!< AppImageLauncher / appimaged already integrated this AppImage
  bool install(QString &message);
  bool uninstall(QString &message);
  /*!
    Update check through appimageupdatetool (needs the embedded update information).
    Returns 0: up to date, 1: update available, -1: could not check (tool missing/failed).
  */
  int checkForUpdate(QString &message);
  bool runUpdate();                 //!< start appimageupdatetool detached; it replaces the AppImage file
}

#endif
