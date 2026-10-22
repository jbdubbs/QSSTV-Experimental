/***************************************************************************
 *   Hand-off of a picture to an external program: the picture is copied    *
 *   (name-edit.ext), the copy is opened in the user's picture editor, and  *
 *   QSSTV is told when the editor saves it. The original is never touched. *
 *                                                                          *
 *   Depends on QtCore/QtGui only, so it can be tested on its own           *
 *   (tests/editor/externaleditor-test.cpp).                                *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef EXTERNALEDITOR_H
#define EXTERNALEDITOR_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

namespace externalEditor
{

struct Candidate
{
  QString label;      //!< "GIMP"
  QString command;    //!< what goes into the setting: "/usr/bin/gimp %f"
};

/*! the picture editors found on this computer, best first */
QVector<Candidate> detect();

/*! The command line to run: `setting` when it is not empty, else the first detected editor, else empty (the system's
    default program for the file is used then). `%f` stands for the file. */
QString effectiveCommand(const QString &setting);

/*! Splits a command line into program and arguments (quotes respected), with %f replaced by `file`, or `file` appended
    when the command has no %f. Empty list for an empty command. */
QStringList commandArguments(const QString &command, const QString &file);

/*! Opens `file` in the program of `command` (see effectiveCommand), or in the desktop's default program for it when no
    program is configured or it cannot be started. Returns false with *error set when nothing could be started. */
bool launch(const QString &file, const QString &command, QString *error);

/*! The path of the copy to edit: <base>-edit.<ext> in `dir` (<base>-edit-2.<ext>... when taken). */
QString editCopyName(const QString &original, const QString &dir);

}  // namespace externalEditor

/*! One hand-off. Makes the copy, starts the editor and polls the copy; saved() is emitted each time the editor
    has written a new version. An unchanged copy is removed again when the session ends. Sessions are owned by
    the application, so they outlive the widget they were started from. */
class ExternalEdit : public QObject
{
  Q_OBJECT
public:
  /*! `copyDir`: where the copy goes. Pass an empty string to put it next to the original. */
  ExternalEdit(const QString &original, const QString &copyDir, const QString &command, QObject *parent = nullptr);
  ~ExternalEdit() override;
  /*! creates the copy and starts the editor; false with *error set if that failed (nothing is left behind) */
  bool start(QString *error);
  QString original() const { return originalFile; }
  QString copy() const { return copyFile; }
  /*! the copy was changed at least once (saved() was emitted) */
  bool changed() const { return reported; }
  /*! how often the copy is looked at (default 1000 ms); a save is reported on the second look that finds it unchanged */
  void setPollInterval(int ms) { timer.setInterval(ms); }

signals:
  void saved(const QString &copyFile);

private slots:
  void poll();

private:
  struct Signature
  {
    qint64 size = -1;
    QDateTime modified;
    bool operator==(const Signature &o) const { return size == o.size && modified == o.modified; }
    bool operator!=(const Signature &o) const { return !(*this == o); }
  };
  Signature signature() const;
  QString originalFile, copyDir, command, copyFile;
  Signature initial, last;
  bool reported = false;      //!< saved() was emitted at least once
  bool pending = false;       //!< a change was seen, waiting for the file to stop changing
  QTimer timer;
};

#endif  // EXTERNALEDITOR_H
