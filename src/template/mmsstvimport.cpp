#include "mmsstvimport.h"
#include "mmsstvtemplate.h"
#include "templaterender.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QTemporaryDir>
#include <cmath>

namespace Mmsstv
{
tpl::LoadResult convertTemplate(const Document &source)
{
  tpl::LoadResult r;
  r.warnings = source.warnings;
  if (source.items.size() > tpl::limits::maxItems)
    {
      r.error = QStringLiteral("Too many objects for a native template (limit %1).").arg(tpl::limits::maxItems);
      return r;
    }
  r.doc.canvas = QRectF(QPointF(), source.size);
  r.doc.layout = tpl::TemplateDocument::Stretch;
  if (source.colorKey
      && (!source.size.isValid() || source.size.width() > 4096 || source.size.height() > 4096
          || !QRect(QPoint(), source.size).contains(source.transparencyPoint)))
    {
      r.error = QStringLiteral("Invalid MMSSTV canvas or transparency sample point.");
      return r;
    }
  const auto families = QFontDatabase::families();
  const Qt::PenStyle styles[] = {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine, Qt::DashDotDotLine, Qt::NoPen, Qt::SolidLine};
  for (const Item &item : source.items)
    {
      if (item.text.size() > tpl::limits::maxTextLength || item.font.family().size() > tpl::limits::maxStringLength
          || item.rect.width() > tpl::limits::maxCoord || item.rect.height() > tpl::limits::maxCoord)
        {
          r.error = QStringLiteral("An object exceeds the native text, font or geometry limits.");
          return r;
        }
      tpl::Element e;
      switch (item.kind)
        {
        case Item::Line:
          e.type = tpl::ItemType::Line;
          break;
        case Item::Rectangle:
          e.type = tpl::ItemType::Rect;
          break;
        case Item::Ellipse:
          e.type = tpl::ItemType::Ellipse;
          break;
        case Item::Text:
          e.type = tpl::ItemType::Text;
          break;
        case Item::Image:
          e.type = tpl::ItemType::Image;
          break;
        case Item::Replay:
          e.type = tpl::ItemType::Replay;
          break;
        }
      e.id = QStringLiteral("mmsstv-%1").arg(r.doc.items.size() + 1);
      e.pos = item.rect.topLeft();
      e.rect = QRectF(QPointF(), item.rect.size());
      e.fill = item.fill;
      if (item.kind == Item::Text && !item.image.isNull()) e.fill = Qt::black;
      e.outline.color = item.color;
      e.outline.width = item.penWidth;
      e.outline.style = styles[qBound(0, item.penStyle, 6)];
      e.text = item.text;
      e.font = tpl::FontSpec::fromFont(item.font);
      e.image = item.image.isNull() ? QImage() : item.image.convertToFormat(QImage::Format_ARGB32);
      if (item.kind == Item::Text)
        {
          // The native renderer uses 96 dpi regardless of the desktop's logical DPI.
          const double pixels = item.font.pixelSize() > 0 ? item.font.pixelSize() : item.font.pointSizeF() * 96.0 / 72.0;
          e.font.pointSize = pixels * 72.0 / 96.0;
          QFont pixelFont = item.font;
          pixelFont.setPixelSize(qMax(1, qRound(pixels)));
          e.pos.ry() += QFontMetricsF(pixelFont).ascent();
          e.rotation = -item.rotation;
          if (!families.contains(item.font.family(), Qt::CaseInsensitive))
            {
              const QString warning = QStringLiteral("Font '%1' is unavailable; Qt will substitute a font.").arg(item.font.family());
              if (!r.warnings.contains(warning)) r.warnings.append(warning);
            }
        }
      else if (item.kind == Item::Line)
        {
          const double length = item.line.length();
          e.rect = QRectF(0, 0, length, 2);
          e.pos = item.line.pointAt(0.5) - QPointF(length / 2, 1);
          e.rotation = -item.line.angle();
        }
      if (item.gradientDirection >= 0 && item.gradientColors.size() == 4)
        {
          e.gradient.kind = tpl::Gradient::Linear;
          e.gradient.angle = item.gradientDirection;
          for (int i = 0; i < 4; ++i) e.gradient.stops.append({i / 3.0, item.gradientColors[i]});
        }
      r.doc.items.append(e);
    }
  if (source.colorKey)
    {
      // Sample only during conversion. Native rendering always paints directly onto transparency.
      QImage composite(source.size, QImage::Format_ARGB32);
      composite.fill(source.background);
      tpl::RenderOptions options;
      options.expandMacros = false;
      {
        QPainter painter(&composite);
        painter.setRenderHint(QPainter::Antialiasing);
        tpl::paintDocument(painter, r.doc, r.doc.canvas, options);
      }
      const QColor key = composite.pixelColor(source.transparencyPoint);
      for (int i = 0; i < r.doc.items.size(); ++i)
        {
          auto &element = r.doc.items[i];
          bool keyed = false;
          const auto clearKey = [&](QColor &color)
            {
              if (color.alpha() && color.rgb() == key.rgb())
                {
                  color.setAlpha(0);
                  keyed = true;
                }
            };
          clearKey(element.fill);
          clearKey(element.outline.color);
          for (auto &stop : element.gradient.stops) clearKey(stop.color);
          if (!keyed) continue;
          const QRectF bounds = tpl::sceneOutline(element, options).boundingRect();
          for (int lower = 0; lower < i; ++lower)
            if (bounds.intersects(tpl::sceneOutline(r.doc.items[lower], options).boundingRect()))
              {
                const QString warning = QStringLiteral("A key-color object overlaps a lower object. Transparent colors reveal lower objects; format "
                                                       "version 1 cannot punch through them.");
                if (!r.warnings.contains(warning)) r.warnings.append(warning);
                break;
              }
        }
    }
  tpl::sanitize(r.doc);
  r.ok = true;
  return r;
}

tpl::LoadResult loadTemplate(const QString &fileName)
{
  Document source;
  QString error;
  if (!readTemplate(fileName, &source, &error))
    {
      tpl::LoadResult r;
      r.error = error;
      return r;
    }
  return convertTemplate(source);
}

QVector<ImportResult> importTemplates(const QStringList &sources, const QString &directory)
{
  QVector<ImportResult> results;
  const QDir destination(directory);
  for (const QString &source : sources)
    {
      ImportResult result;
      result.source = source;
      if (!QFileInfo(directory).isDir()) result.error = QStringLiteral("Destination directory does not exist.");
      else
        {
          const auto loaded = loadTemplate(source);
          result.warnings = loaded.warnings;
          if (!loaded.ok) result.error = loaded.error;
          else
            {
              QTemporaryDir staging(destination.absoluteFilePath(QStringLiteral(".qsstv-import-XXXXXX")));
              const QString staged = staging.filePath(QStringLiteral("template.qtempl"));
              if (!staging.isValid()) result.error = QStringLiteral("Cannot create a temporary directory in the destination.");
              else if (tpl::saveFile(staged, loaded.doc, &result.error))
                {
                  QString base = QFileInfo(source).completeBaseName();
                  if (base.isEmpty() || base == "." || base == "..") base = QStringLiteral("template");
                  for (int suffix = 1; suffix <= 100000; ++suffix)
                    {
                      const QString name = base + (suffix == 1 ? QString() : QStringLiteral("_%1").arg(suffix)) + ".qtempl";
                      const QString target = destination.absoluteFilePath(name);
                      const QString legacy = destination.absoluteFilePath(QFileInfo(name).completeBaseName() + ".templ");
                      if (QFileInfo::exists(target) || QFileInfo(target).isSymLink() || QFileInfo::exists(legacy) || QFileInfo(legacy).isSymLink())
                        continue;
                      if (QFile::rename(staged, target))
                        {
                          result.target = target;
                          break;
                        }
                      if (QFileInfo::exists(target) || QFileInfo(target).isSymLink()) continue;
                      result.error = QStringLiteral("Cannot publish the converted template.");
                      break;
                    }
                  if (result.target.isEmpty() && result.error.isEmpty()) result.error = QStringLiteral("No available output filename.");
                }
            }
        }
      results.append(result);
    }
  return results;
}
}
