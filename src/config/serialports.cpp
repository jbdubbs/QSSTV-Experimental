#include "serialports.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QSerialPortInfo>

QList<serialPortChoice> serialPortList(const QString &byIdDir)
{
  QList<serialPortChoice> ports;
#ifdef Q_OS_LINUX
  const QFileInfoList links=QDir(byIdDir).entryInfoList(QDir::AllEntries|QDir::System|QDir::NoDotAndDotDot,QDir::Name);
  for(const QFileInfo &link:links) ports.append({link.absoluteFilePath(),link.symLinkTarget()});
#else
  Q_UNUSED(byIdDir);
#endif
  if(!ports.isEmpty()) return ports;
  const QList<QSerialPortInfo> found=QSerialPortInfo::availablePorts();
  for(const QSerialPortInfo &info:found)
    {
#ifdef Q_OS_WIN
      ports.append({info.portName(),info.description()});
#else
      ports.append({info.systemLocation(),info.description()});
#endif
    }
  return ports;
}

bool samePort(const QString &a,const QString &b)
{
  if(a.compare(b,Qt::CaseInsensitive)==0) return true;
  const QString ca=QFileInfo(a).canonicalFilePath();   // empty for a name that is not a path, such as COM1
  return !ca.isEmpty() && ca==QFileInfo(b).canonicalFilePath();
}

void fillPortComboBox(QComboBox *box,const QString &byIdDir)
{
  const QString current=box->currentText();
  box->clear();
  const QList<serialPortChoice> ports=serialPortList(byIdDir);
  for(const serialPortChoice &p:ports)
    {
      box->addItem(p.name);
      box->setItemData(box->count()-1,p.info,Qt::ToolTipRole);
    }
  // by-id names are long and differ at both ends: let the list be as wide as the longest of them, and elide in the middle
  // where the screen is not wide enough
  box->view()->setTextElideMode(Qt::ElideMiddle);
  box->view()->setMinimumWidth(box->view()->sizeHintForColumn(0)+box->view()->frameWidth()*2+8);
  box->setCurrentText(current);
}
