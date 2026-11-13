#include "appimageupdater.h"
#include "appimageintegration.h"
#include <QRegularExpression>

appImageUpdater::appImageUpdater(QObject *parent) : QObject(parent)
{
  process.setProcessChannelMode(QProcess::MergedChannels);   // the progress is on stdout, the result on stderr
  connect(&process,&QProcess::readyRead,this,&appImageUpdater::readOutput);
  connect(&process,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,&appImageUpdater::processFinished);
  connect(&process,&QProcess::errorOccurred,this,&appImageUpdater::processError);
}

appImageUpdater::~appImageUpdater()
{
  if(running())
    {
      disconnect(&process,nullptr,this,nullptr);
      process.kill();
      process.waitForFinished(2000);
    }
}

bool appImageUpdater::start(const QString &tool,const QString &appImagePath)
{
  appImage=appImagePath;
  output.clear();
  pending.clear();
  cancelled=false;
  done=false;
  QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
  env.insert("APPIMAGE_EXTRACT_AND_RUN","1");   // the tool is an AppImage too; do not require FUSE
  process.setProcessEnvironment(env);
  process.start(tool,{"-r",appImagePath});
  return process.waitForStarted(10000);
}

void appImageUpdater::cancel()
{
  if(!running()) return;
  cancelled=true;
  process.kill();
}

bool appImageUpdater::parseProgress(const QString &line,double &percent,double &doneMiB,double &totalMiB)
{
  static const QRegularExpression re(R"((\d+(?:\.\d+)?)% done \((\d+(?:\.\d+)?) of (\d+(?:\.\d+)?) MiB\))");
  const QRegularExpressionMatch m=re.match(line);
  if(!m.hasMatch()) return false;
  percent=m.captured(1).toDouble();
  doneMiB=m.captured(2).toDouble();
  totalMiB=m.captured(3).toDouble();
  return true;
}

QString appImageUpdater::parseNewFile(const QString &text)
{
  static const QRegularExpression re(R"(New file created:\s*(.+?)\s*$)",QRegularExpression::MultilineOption);
  const QRegularExpressionMatch m=re.match(text);
  return m.hasMatch() ? m.captured(1) : QString();
}

void appImageUpdater::readOutput()
{
  pending+=process.readAll();
  // the tool redraws its progress line with a carriage return; the escape sequence "ESC[2K" clears the line
  int start=0;
  for(int i=0;i<pending.size();i++)
    {
      if(pending.at(i)!='\r' && pending.at(i)!='\n') continue;
      const QString line=QString::fromUtf8(pending.mid(start,i-start)).remove(QRegularExpression("\x1b\\[[0-9;]*[A-Za-z]")).trimmed();
      start=i+1;
      if(line.isEmpty()) continue;
      output+=line+'\n';
      double p,d,t;
      if(parseProgress(line,p,d,t)) emit progress(p,d,t);
    }
  pending.remove(0,start);
}

void appImageUpdater::emitFinished(bool ok,const QString &newPath,const QString &error)
{
  if(done) return;
  done=true;
  emit finished(ok,newPath,error);
}

void appImageUpdater::processFinished(int exitCode,QProcess::ExitStatus status)
{
  readOutput();
  if(!pending.isEmpty()) {pending+='\n'; readOutput();}
  if(cancelled)
    {
      emitFinished(false,QString(),QString());   // no error: the user asked for it
      return;
    }
  if(status==QProcess::NormalExit && exitCode==0)
    {
      QString path=parseNewFile(output);
      if(path.isEmpty()) path=appImage;   // an update in place names no new file
      emitFinished(true,path,QString());
      return;
    }
  // the last line that is not progress says what went wrong
  QString why;
  for(const QString &l:output.split('\n',Qt::SkipEmptyParts))
    {
      double p,d,t;
      if(!parseProgress(l,p,d,t) && !l.startsWith("Validation warning")) why=l;
    }
  emitFinished(false,QString(),why.isEmpty() ? tr("appimageupdatetool failed (exit code %1).").arg(exitCode) : why);
}

void appImageUpdater::processError(QProcess::ProcessError e)
{
  if(e==QProcess::FailedToStart) emitFinished(false,QString(),tr("appimageupdatetool could not be started."));
}
