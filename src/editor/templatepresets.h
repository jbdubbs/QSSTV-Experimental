/***************************************************************************
 *   Ready-made starting points for the template editor: starter templates  *
 *   for a new template, and color bars to insert. Plain tpl:: data        *
 *   builders (QtGui only), scaled to the size of the canvas they are for.  *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef TEMPLATEPRESETS_H
#define TEMPLATEPRESETS_H

#include <QSize>
#include <QStringList>
#include "templatedoc.h"

namespace templatePresets
{

/*! names of the starter templates; entry 0 is the blank template */
QStringList starterNames();

/*! The starter template with the given index for a canvas of the given size, laid out to keep its proportions. Fonts and distances
    scale with the height of the canvas (they are designed for 496 pixels). An index out of range gives the blank template. */
tpl::TemplateDocument makeStarter(int index, const QSize &canvas);

/*! names of the color bar styles */
QStringList barNames();

/*! A bar across the full width of the canvas, at its bottom or top, filled with the gradient of the given style. It is anchored
    to span the width and to its edge. An unknown style gives the first. */
tpl::Element makeBar(int style, bool atBottom, const QRectF &canvas);

}  // namespace templatePresets

#endif  // TEMPLATEPRESETS_H
