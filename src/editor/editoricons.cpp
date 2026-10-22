#include "editoricons.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace editorIcons
{

namespace
{
QPixmap render(const QByteArray &svg, int px, const QColor &color)
{
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QByteArray data = svg;
  data.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
  QSvgRenderer r(data);
  if (!r.isValid()) return QPixmap();
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  r.render(&p, QRectF(0, 0, px, px));
  return pm;
}
}  // namespace

QIcon icon(const QString &name, const QPalette &pal)
{
  static QHash<QString, QByteArray> cache;
  if (!cache.contains(name))
    {
      QFile f(QStringLiteral(":/editor/icons/%1.svg").arg(name));
      cache.insert(name, f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray());
    }
  const QByteArray &svg = cache[name];
  if (svg.isEmpty()) return QIcon();
  QIcon ic;
  const QColor normal = pal.color(QPalette::Active, QPalette::ButtonText);
  const QColor disabled = pal.color(QPalette::Disabled, QPalette::ButtonText);
  const QColor selected = pal.color(QPalette::Active, QPalette::HighlightedText);
  for (int px : {16, 24, 32, 48})
    {
      ic.addPixmap(render(svg, px, normal), QIcon::Normal, QIcon::Off);
      ic.addPixmap(render(svg, px, disabled), QIcon::Disabled, QIcon::Off);
      ic.addPixmap(render(svg, px, selected), QIcon::Selected, QIcon::Off);
    }
  return ic;
}

QStringList names()
{
  QStringList l;
  const QStringList files = QDir(QStringLiteral(":/editor/icons")).entryList(QStringList() << QStringLiteral("*.svg"));
  for (const QString &f : files) l << f.left(f.size() - 4);
  return l;
}

}  // namespace editorIcons
