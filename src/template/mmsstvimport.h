// MMSSTV conversion into the Qt6 document model; no application globals.
#ifndef MMSSTVIMPORT_H
#define MMSSTVIMPORT_H
#include "templatedoc.h"
namespace Mmsstv {
struct Document;
struct ImportResult {
  QString source, target, error;
  QStringList warnings;
};
tpl::LoadResult convertTemplate(const Document &source);
tpl::LoadResult loadTemplate(const QString &fileName);
// Atomic publication, unique filenames; source and existing destination entries stay untouched.
QVector<ImportResult> importTemplates(const QStringList &sources, const QString &directory);
}
#endif
