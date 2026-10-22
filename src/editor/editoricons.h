/***************************************************************************
 *   The template editor's icons: monochrome SVGs (editor/icons) drawn in   *
 *   the palette's text colour, so they stay readable in dark mode.         *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef EDITORICONS_H
#define EDITORICONS_H

#include <QIcon>
#include <QPalette>
#include <QString>

namespace editorIcons
{
/*! the icon `name` (the file name without .svg) in the colours of `pal`; a null icon for an unknown name */
QIcon icon(const QString &name, const QPalette &pal);
/*! all icon names, for the tests */
QStringList names();
}  // namespace editorIcons

#endif  // EDITORICONS_H
