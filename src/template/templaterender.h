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

/*! Paints the whole template into `target` of the painter's device, mapping the canvas onto it
    (stretched, or aspect preserving when keepAspect is set, as QGraphicsScene::render did). */
void paintDocument(QPainter &p, const TemplateDocument &doc, const QRectF &target, const RenderOptions &opt, bool keepAspect = false);

/*! The template laid over a copy of `base`, canvas stretched to the size of `base`. This is what the TX path
    transmits. The result is never null for a non-null base. */
QImage overlay(const TemplateDocument &doc, const QImage &base, const RenderOptions &opt);

/*! The template on a transparent canvas of its own size (gallery thumbnail source), macros left literal. */
QImage renderThumbnail(const TemplateDocument &doc);

}  // namespace tpl

#endif  // TEMPLATERENDER_H
