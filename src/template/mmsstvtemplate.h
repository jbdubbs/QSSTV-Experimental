#ifndef MMSSTVTEMPLATE_H
#define MMSSTVTEMPLATE_H
#include <QColor>
#include <QFont>
#include <QImage>
#include <QLineF>
#include <QRectF>
#include <QStringList>
#include <QVector>

namespace Mmsstv {
struct Item {
  enum Kind { Line, Rectangle, Text, Image, Replay, Ellipse } kind = Rectangle;
  QRectF rect;
  QLineF line;
  QColor color = Qt::black, fill = Qt::transparent;
  int penStyle = 0, penWidth = 1, rotation = 0;
  int gradientDirection = -1; // -1: solid; 0: left-to-right; 270: top-to-bottom
  QVector<QColor> gradientColors;
  QString text;
  QFont font;
  QImage image;
};
struct Document {
  QSize size = QSize(320,256);
  bool colorKey = false;
  QColor background = QColor(248,248,248);
  QPoint transparencyPoint;
  QVector<Item> items;
  QStringList warnings;
};
// Failure leaves the caller's document untouched.
bool readTemplate(const QString &fileName, Document *document, QString *error);
QString convertMacros(const QString &text, QStringList *warnings);
}
#endif
