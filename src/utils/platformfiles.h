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
    (the copy keeps the file's name and extension). Returns an empty string when it cannot be copied.
  */
  QString importPicked(const QString &picked);

  //! Android: make a saved picture show up in the gallery (Pictures/QSSTV-Experimental). Returns true when it was added.
  bool exportToGallery(const QString &localFile);

  //! Delete the copies importPicked() made in earlier runs
  void cleanImportCache();
}

#endif // PLATFORMFILES_H
