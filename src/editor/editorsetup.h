/***************************************************************************
 *   Glue between the template editor window (which knows nothing about    *
 *   the application) and the application's globals: where the templates   *
 *   are, the SSTV mode sizes, the operator's macro values, the current     *
 *   transmit picture.                                                      *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef EDITORSETUP_H
#define EDITORSETUP_H

#include <QHash>
#include <QVector>
#include "canvassizedialog.h"

class TemplateEditor;

/*! the picture size of every SSTV mode, one entry per distinct size ("PD120, PD180 (640 × 496)") */
QVector<SizePreset> sstvSizePresets();
/*! macro values for the editor's preview: the operator's own where known, samples otherwise */
QHash<QChar, QString> editorMacroValues();
/*! templates folder, mode sizes, macro values and the transmit picture */
void configureTemplateEditor(TemplateEditor *ed);

#endif  // EDITORSETUP_H
