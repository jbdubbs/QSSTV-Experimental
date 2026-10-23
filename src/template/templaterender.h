/***************************************************************************
 *   Headless template renderer. paintElement() is the only code that      *
 *   draws a template item: the TX overlay, the gallery thumbnails and the *
 *   template editor all go through it.                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#ifndef TEMPLATERENDER_H
#define TEMPLATERENDER_H

#include <QHash>
#include <QImage>
#include <QPainter>
#include "templatedoc.h"

namespace tpl
{

struct RenderOptions
{
  QHash<QChar, QString> vars;    //!< macro values (%c, %m, ...); an unknown %x expands to nothing, %% to %
  QImage replay;                 //!< picture shown in the replay slot; null = hatched placeholder
  bool expandMacros = true;      //!< false: macros stay literal (thumbnails, editor)
};

/*! text with the %x macros replaced */
QString expandMacros(const QString &text, const QHash<QChar, QString> &vars);

/*! Paints one item with its own transform, in the coordinate system of the template canvas. */
void paintElement(QPainter &p, const Element &e, const RenderOptions &opt);

/*! Geometry helpers for the editor. All use the same text layout as paintElement(), so what the editor outlines is
    what the renderer draws. */

/*! bounding rectangle of a text item's outline in item coordinates (the text is drawn from the origin); macros are
    expanded per opt. Never empty: an empty text gets a small box so it can still be selected. */
QRectF textBounds(const Element &e, const RenderOptions &opt = RenderOptions());

/*! the lines of the item's text as they are drawn: split at the line breaks of the text, and for text in a box wrapped to the box
    (macros expanded per opt) */
QStringList textLines(const Element &e, const RenderOptions &opt = RenderOptions());

/*! The box that holds the text of `e` exactly as laid out without a box (every line with its full advance width, from the
    first line's ascent to the last line's descent), with the baseline start of the text at the origin: giving a free text this
    rectangle and textBox = true leaves it where it is and as big as it is. */
QRectF naturalTextBox(const Element &e, const RenderOptions &opt = RenderOptions());

/*! the item's own extent in item coordinates: `rect` for shapes, images and the replay slot, the text outline for
    text, a band of the pen's width along the middle of `rect` for a line */
QRectF localBounds(const Element &e, const RenderOptions &opt = RenderOptions());

/*! item coordinates -> canvas coordinates (what paintElement() sets up before drawing) */
QTransform sceneTransform(const Element &e);

/*! localBounds() as a polygon in canvas coordinates (a parallelogram once the item is rotated or sheared) */
QPolygonF sceneOutline(const Element &e, const RenderOptions &opt = RenderOptions());

/*! Anchored layout (TemplateDocument::Anchored): the canvas is scaled by one factor to fit `target`, which leaves extra room on
    one axis. anchoredScale() is that factor, anchoredCanvasSize() the size of the target in units of the canvas (never
    smaller than the canvas, equal to it on the tighter axis), and anchoredElement() the item moved or grown according to its
    anchors so that it follows the edges of the target. */
double anchoredScale(const QRectF &canvas, const QSizeF &target);
QSizeF anchoredCanvasSize(const QRectF &canvas, const QSizeF &target);
Element anchoredElement(const Element &e, const QRectF &canvas, const QSizeF &effective);

/*! Paints the whole template into `target` of the painter's device, mapping the canvas onto it
    (stretched; one scale factor with the items placed by their anchors when the document is Anchored; or aspect preserving
    when keepAspect is set, as QGraphicsScene::render did). */
void paintDocument(QPainter &p, const TemplateDocument &doc, const QRectF &target, const RenderOptions &opt, bool keepAspect = false);

/*! The template laid over a copy of `base`, canvas stretched to the size of `base`. This is what the TX path
    transmits. The result is never null for a non-null base. */
QImage overlay(const TemplateDocument &doc, const QImage &base, const RenderOptions &opt);

/*! The template on a transparent canvas of its own size (gallery thumbnail source), macros left literal. */
QImage renderThumbnail(const TemplateDocument &doc);

}  // namespace tpl

#endif  // TEMPLATERENDER_H
