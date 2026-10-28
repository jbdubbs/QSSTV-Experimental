/***************************************************************************
 *   Template document model: what a QSSTV-Experimental image template is,  *
 *   independent of any widget. Used by the headless renderer (TX overlay, *
 *   gallery thumbnails) and by the template editor.                       *
 *                                                                         *
 *   This file depends on QtCore/QtGui only (no application globals), so   *
 *   it can be built and tested on its own (tests/template).               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#ifndef TEMPLATEDOC_H
#define TEMPLATEDOC_H

#include <QColor>
#include <QFont>
#include <QGradient>
#include <QImage>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <QVector>

namespace tpl
{

/*! Hard limits enforced by both loaders (legacy and JSON). A template is untrusted input: it can come from a
    forum download and the gallery parses every file in the templates folder, so a corrupt or hostile file must
    never be able to exhaust memory or CPU. */
namespace limits
{
const int maxItems = 1000;
const double maxCoord = 20000.0;          //!< |x|,|y|,w,h
const int maxImageSide = 4096;            //!< embedded images are scaled down to this on load
const int maxTextLength = 4096;
const int maxStringLength = 128;          //!< ids, font families
const int maxGradientStops = 16;
const double maxShadowOffset = 200.0;     //!< |dx|,|dy| of a shadow
const double maxShadowBlur = 100.0;
const qint64 maxFileBytes = 64LL * 1024 * 1024;
}

/*! file extension of the native format (without dot) */
extern const char *const nativeExtension;

struct GradientStop
{
  double pos = 0;                         //!< 0..1
  QColor color;
};

/*! Linear, radial or conical gradient laid over the bounding rectangle of the item (the text outline's bounding
    rectangle for text). The angle follows the legacy editor: degrees, 0 = east. */
struct Gradient
{
  enum Kind { None, Linear, Radial, Conical };
  Kind kind = None;
  int angle = 0;
  QVector<GradientStop> stops;
  bool isActive() const { return kind != None && !stops.isEmpty(); }
  QGradient build(const QRectF &r) const;  //!< port of the legacy buildGradient()
};

struct Outline
{
  QColor color = Qt::black;
  double width = 1.0;
  Qt::PenJoinStyle join = Qt::MiterJoin;
  Qt::PenCapStyle cap = Qt::SquareCap;
  Qt::PenStyle style = Qt::SolidLine;
  double miterLimit = 2.0;
  QPen toPen() const;
};

struct FontSpec
{
  QString family;
  double pointSize = 12;                  //!< at the 96 dpi reference, so a template looks the same on every screen
  int weight = 400;                       //!< 1..1000 (QFont::Weight)
  bool italic = false;
  bool underline = false;
  bool strikeOut = false;
  int stretch = 0;                        //!< QFont::stretch, 0 = the font's own
  double letterSpacing = 0;               //!< extra pixels between letters
  QFont toFont() const;                   //!< outline strategy forced: text is drawn as a path, never hinted bitmaps
  static FontSpec fromFont(const QFont &f);
};

enum class ItemType { Text, Rect, Ellipse, Line, Image, Replay };

/*! How an item follows the picture it is laid over when the template's layout is Anchored (TemplateDocument::layout).
    Left/Top, Center/Middle and Right/Bottom keep the item's distance (scaled) to that edge or the middle; Span keeps
    both edges, so the item grows with the picture; Scale grows position and size in proportion (the old stretch). */
enum class AnchorH { Left, Center, Right, Span, Scale };
enum class AnchorV { Top, Middle, Bottom, Span, Scale };

enum class TextAlign { Left, Center, Right };
enum class TextVAlign { Top, Middle, Bottom };

/*! Drop shadow of an item: the item drawn again in `color`, moved by (dx, dy) in canvas directions, optionally blurred. */
struct Shadow
{
  bool on = false;
  QColor color = QColor(0, 0, 0, 160);
  double dx = 2;
  double dy = 2;
  double blur = 0;                        //!< radius in canvas pixels, 0 = hard edge
};

struct Element
{
  ItemType type = ItemType::Rect;
  QString id;
  QPointF pos;                            //!< item origin; text is drawn from its baseline start at the origin
  QRectF rect = QRectF(0, 0, 100, 100);   //!< item-local rectangle (rotation/shear center; shape for the others)
  double rotation = 0;                    //!< degrees, about the center of rect
  double skewH = 0;                       //!< QTransform::shear factors
  double skewV = 0;
  double opacity = 1.0;
  bool locked = false;
  QColor fill = Qt::white;
  Gradient gradient;                      //!< replaces fill for text, rectangle and ellipse when active
  Outline outline;
  QString text;                           //!< may contain %x macros
  FontSpec font;
  QImage image;
  AnchorH anchorH = AnchorH::Left;        //!< only used when the document layout is Anchored
  AnchorV anchorV = AnchorV::Top;
  double radius = 0;                      //!< rectangle corner radius (0 = square corners)
  Shadow shadow;
  // Text in a box: `rect` is the box, the text is wrapped / aligned / shrunk inside it. Off = the text is drawn from its
  // baseline start at the origin, as in the legacy editor.
  bool textBox = false;
  TextAlign align = TextAlign::Left;
  TextVAlign valign = TextVAlign::Top;
  bool wrap = false;                      //!< break lines at spaces to the width of the box
  bool shrinkToFit = false;               //!< make the font smaller (never larger) until the text fits the box
  QTransform itemTransform() const;       //!< shear, then rotate, both about the center of rect
};

struct TemplateDocument
{
  enum Layout { Stretch, Anchored };
  QRectF canvas = QRectF(0, 0, 320, 256);  //!< the area that is mapped onto the target picture
  Layout layout = Stretch;                 //!< Stretch: canvas stretched over the target (legacy behavior)
  QVector<Element> items;                  //!< paint order, first = bottom
  bool sampledTransparency = false;         //!< remove the sampled color after composing the complete overlay
  QColor background = QColor(248, 248, 248);
  QPointF transparencyPoint;
  bool fromLegacy = false;                 //!< imported from the binary .templ format
  bool hasReplay() const;
};

struct LoadResult
{
  bool ok = false;
  QString error;
  QStringList warnings;
  TemplateDocument doc;
};

/*! Reads a template: JSON (.qtempl) or the legacy binary format (.templ), detected from the content. */
LoadResult loadFile(const QString &fileName);
/*! Same, remembering the last file (path, size, modification time) so repeated TX renders do not re-parse it. */
LoadResult loadFileCached(const QString &fileName);
LoadResult loadJson(const QByteArray &data);
LoadResult loadLegacy(const QByteArray &data);        //!< templatelegacy.cpp
bool looksLikeTemplate(const QByteArray &head);       //!< cheap sniff of the first bytes
QByteArray toJson(const TemplateDocument &doc);
bool saveFile(const QString &fileName, const TemplateDocument &doc, QString *error = nullptr);

/*! Chooses the anchors from where the item sits: a frame covering most of an axis spans it, otherwise the third of the
    canvas that holds the frame's center decides (left/center/right, top/middle/bottom). Text that is not in a box has no
    frame, so its origin is used; a turned item is placed by where it really is and never spans. Used when a template becomes
    Anchored and for new items in an anchored template. An item that touches one side (within 5% of the canvas) follows that side
    even when its center is in the middle third. */
void autoAnchor(Element &e, const QRectF &canvas);

/*! An edit moved or resized an item: when its anchors on an axis were still the ones autoAnchor() picks for where it was, they
    follow it to where it is now. Anchors the user chose, or changed in this very edit, stay. */
void reanchor(const Element &before, Element &after, const QRectF &canvas);

/*! clamps every value into the supported range (used by both loaders; exposed for the editor) */
void sanitize(Element &e);
void sanitize(TemplateDocument &d);

QString colorToString(const QColor &c);
QColor colorFromString(const QString &s, const QColor &fallback);

}  // namespace tpl

#endif  // TEMPLATEDOC_H
