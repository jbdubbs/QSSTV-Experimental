/***************************************************************************
 *   The text macros a template can use (%c, %m, ...): what each one means, *
 *   and sample values for previews. The real values are assembled where    *
 *   they are known (imageViewer::applyTemplate); this table only describes *
 *   them, for the editor's macro palette and for tests.                    *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef TEMPLATEVARS_H
#define TEMPLATEVARS_H

#include <QChar>
#include <QHash>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace tpl
{
/*! Compatible MMSSTV fields, captured at one instant for UTC and local dates/times. */
void addMmsstvVars(QHash<QChar, QString> &vars, const QString &call, const QString &report, const QDateTime &instant);


struct MacroInfo
{
  QChar tag;             //!< the character after the %
  QString name;          //!< short, translated
  QString description;   //!< one sentence, translated
  QString sample;        //!< what the editor shows when previewing macros
};

/*! every macro the TX path defines, in the order the palette lists them */
const QVector<MacroInfo> &macroTable();

/*! sample values for all macros (tag -> sample) */
QHash<QChar, QString> sampleVars();

/*! the macros used in `text` that are not in the table (so they expand to nothing); each tag once, in order */
QString unknownMacros(const QString &text);

}  // namespace tpl

#endif  // TEMPLATEVARS_H
