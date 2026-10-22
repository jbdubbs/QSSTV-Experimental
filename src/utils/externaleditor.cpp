#include "externaleditor.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace externalEditor
{

namespace
{
// formats an editor is expected to read and write unchanged; anything else (AVIF, HEIC, JPEG 2000 ...) is converted to PNG
bool plainFormat(const QString &suffix)
{
  static const QStringList formats = {"png", "jpg", "jpeg", "bmp", "gif", "tif", "tiff", "webp"};
  return formats.contains(suffix.toLower());
}
}  // namespace

QVector<Candidate> detect()
{
  QVector<Candidate> found;
  auto addProgram = [&found](const QString &label, const QString &program) {
    const QString path = QStandardPaths::findExecutable(program);
    if (!path.isEmpty())
      {
        QString quoted = path;
        if (quoted.contains(QLatin1Char(' '))) quoted = QLatin1Char('"') + quoted + QLatin1Char('"');
        found.append({label, quoted + QStringLiteral(" %f")});
      }
  };
#if defined(Q_OS_WIN)
  addProgram(QStringLiteral("GIMP"), QStringLiteral("gimp"));
  addProgram(QStringLiteral("Krita"), QStringLiteral("krita"));
  addProgram(QStringLiteral("Paint"), QStringLiteral("mspaint"));
#elif defined(Q_OS_MACOS)
  const QStringList apps = {"GIMP", "Krita", "Pixelmator Pro", "Pixelmator", "Affinity Photo 2", "Preview"};
  for (const QString &a : apps)
    if (QFileInfo::exists(QStringLiteral("/Applications/%1.app").arg(a)) || QFileInfo::exists(QStringLiteral("/System/Applications/%1.app").arg(a)))
      found.append({a, QStringLiteral("open -a \"%1\" %f").arg(a)});
#else
  addProgram(QStringLiteral("GIMP"), QStringLiteral("gimp"));
  addProgram(QStringLiteral("Krita"), QStringLiteral("krita"));
  addProgram(QStringLiteral("KolourPaint"), QStringLiteral("kolourpaint"));
  addProgram(QStringLiteral("Pinta"), QStringLiteral("pinta"));
  addProgram(QStringLiteral("Photoflare"), QStringLiteral("photoflare"));
  addProgram(QStringLiteral("MyPaint"), QStringLiteral("mypaint"));
  addProgram(QStringLiteral("Drawing"), QStringLiteral("drawing"));
#endif
  return found;
}

QString effectiveCommand(const QString &setting)
{
  if (!setting.trimmed().isEmpty()) return setting.trimmed();
  const QVector<Candidate> found = detect();
  return found.isEmpty() ? QString() : found.first().command;
}

QStringList commandArguments(const QString &command, const QString &file)
{
  QStringList args = QProcess::splitCommand(command.trimmed());
  if (args.isEmpty()) return args;
  bool substituted = false;
  for (QString &a : args)
    if (a.contains(QLatin1String("%f")))
      {
        a.replace(QLatin1String("%f"), file);
        substituted = true;
      }
  if (!substituted) args.append(file);
  return args;
}

bool launch(const QString &file, const QString &command, QString *error)
{
  const QString cmd = effectiveCommand(command);
  QString problem;
  if (!cmd.isEmpty())
    {
      QStringList args = commandArguments(cmd, file);
      if (!args.isEmpty())
        {
          const QString program = args.takeFirst();
          if (QProcess::startDetached(program, args)) return true;
          problem = QCoreApplication::translate("externalEditor", "Could not start %1").arg(program);
        }
    }
  // no program configured, or it would not start (a sandbox, a wrong path): the desktop's default program for the file
  if (QDesktopServices::openUrl(QUrl::fromLocalFile(file))) return true;
  if (error)
    *error = problem.isEmpty() ? QCoreApplication::translate("externalEditor", "No program found to open %1").arg(QDir::toNativeSeparators(file)) : problem;
  return false;
}

QString editCopyName(const QString &original, const QString &dir)
{
  const QFileInfo oi(original);
  const QString suffix = plainFormat(oi.suffix()) ? oi.suffix() : QStringLiteral("png");
  const QDir d(dir.isEmpty() ? oi.absolutePath() : dir);
  QString name = d.filePath(QStringLiteral("%1-edit.%2").arg(oi.completeBaseName(), suffix));
  for (int i = 2; QFile::exists(name); i++) name = d.filePath(QStringLiteral("%1-edit-%2.%3").arg(oi.completeBaseName()).arg(i).arg(suffix));
  return name;
}

}  // namespace externalEditor

// ---------------------------------------------------------------------------------------------------------------
ExternalEdit::ExternalEdit(const QString &original, const QString &dir, const QString &cmd, QObject *parent)
  : QObject(parent), originalFile(original), copyDir(dir), command(cmd)
{
  timer.setInterval(1000);
  connect(&timer, &QTimer::timeout, this, &ExternalEdit::poll);
}

ExternalEdit::~ExternalEdit()
{
  // nothing was saved: do not leave a duplicate of the picture behind
  if (!reported && !copyFile.isEmpty() && signature() == initial) QFile::remove(copyFile);
}

ExternalEdit::Signature ExternalEdit::signature() const
{
  Signature s;
  const QFileInfo fi(copyFile);
  if (fi.exists())
    {
      s.size = fi.size();
      s.modified = fi.lastModified();
    }
  return s;
}

bool ExternalEdit::start(QString *error)
{
  const QFileInfo oi(originalFile);
  if (!oi.isFile())
    {
      if (error) *error = tr("%1 does not exist").arg(QDir::toNativeSeparators(originalFile));
      return false;
    }
  if (!copyDir.isEmpty()) QDir().mkpath(copyDir);
  copyFile = externalEditor::editCopyName(originalFile, copyDir);
  bool made = false;
  if (QFileInfo(copyFile).suffix().compare(oi.suffix(), Qt::CaseInsensitive) == 0) made = QFile::copy(originalFile, copyFile);
  else
    {
      QImageReader reader(originalFile);
      reader.setAutoTransform(true);
      const QImage img = reader.read();
      made = !img.isNull() && img.save(copyFile, "PNG");
    }
  if (!made)
    {
      if (error) *error = tr("Cannot make a copy of %1 in %2").arg(oi.fileName(), QDir::toNativeSeparators(QFileInfo(copyFile).absolutePath()));
      QFile::remove(copyFile);
      copyFile.clear();
      return false;
    }
  // the copy of a read-only original is read-only too
  QFile::setPermissions(copyFile, QFile::permissions(copyFile) | QFileDevice::WriteOwner | QFileDevice::ReadOwner);
  initial = last = signature();
  if (!externalEditor::launch(copyFile, command, error))
    {
      QFile::remove(copyFile);
      copyFile.clear();
      return false;
    }
  age.start();
  timer.start();
  return true;
}

void ExternalEdit::poll()
{
  if (age.hasExpired(lifetimeMs))
    {
      deleteLater();                         // the destructor removes a copy nobody saved
      return;
    }
  const Signature now = signature();
  if (now.size < 0)
    {
      // mid-save (removed, about to be renamed into place) for a moment; gone for good after half a minute
      if (++missingPolls * timer.interval() > 30000) deleteLater();
      return;
    }
  missingPolls = 0;
  if (now != last)
    {
      // changed since the last look: wait until it stays put, the editor may still be writing
      last = now;
      pending = now != initial;
      return;
    }
  if (pending)
    {
      pending = false;
      reported = true;
      initial = now;                         // the next save is a new change
      emit saved(copyFile);
    }
}
