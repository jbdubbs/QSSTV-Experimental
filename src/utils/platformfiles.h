#ifndef PLATFORMFILES_H
#define PLATFORMFILES_H

#include <QString>

/*!
  Files the way the platform wants them. On Android the file picker returns content:// URIs, which most of this program (paths,
  QFileInfo, QDir, OpenJPEG's fopen) cannot use, and a received picture saved in the app's private folder is invisible to the
  phone's gallery. Everywhere else these do nothing: a path stays a path.
*/
namespace platformFiles
{
  /*!
    A file the user picked. Returns a path the program can open: a content:// URI is copied into the app's cache first
    (the copy keeps the file's name and extension). A HEIC/HEIF picture is decoded by the phone and the PNG is returned,
    because the program has no HEVC decoder. Returns an empty string when it cannot be copied.
  */
  QString importPicked(const QString &picked);

  /*!
    The file to hand to the phone's gallery or to another app for a picture of ours. The phone knows PNG, JPEG, WebP, BMP, GIF and
    AVIF; a picture in any other format (JPEG 2000: what a DRM transmission delivers by default) is decoded and written as a PNG
    with the same name in dir, and that file is returned. A picture in a known format is returned as it is. Empty when it cannot be
    decoded or written. Not tied to Android, so the conversion can be tried anywhere.
  */
  QString pictureForPhone(const QString &file,const QString &dir);

  //! Android: make a saved picture show up in the gallery (Pictures/QSSTV-Experimental), as a PNG when the phone does not know its format. Returns true when it was added.
  bool exportToGallery(const QString &localFile);

  //! Android: offer a saved picture to other apps (the share sheet), as a PNG when the phone does not know its format. Returns false where there is no such thing.
  bool shareImage(const QString &localFile);

  //! Delete the copies importPicked() made in earlier runs
  void cleanImportCache();

  //! Delete the PNG copies pictureForPhone() made for the share sheet in earlier runs
  void cleanExportCache();
}

#endif // PLATFORMFILES_H
