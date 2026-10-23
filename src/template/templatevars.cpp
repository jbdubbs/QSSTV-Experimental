#include "templatevars.h"

#include <QCoreApplication>

namespace tpl
{

namespace
{
QString tr(const char *s)
{
  return QCoreApplication::translate("tpl::Macros", s);
}
}  // namespace

const QVector<MacroInfo> &macroTable()
{
  // built on first use so the translations follow the application's language
  static const QVector<MacroInfo> table = {
    {'c', tr("To call"), tr("Call sign of the station you are sending to (TX tab)"), QStringLiteral("W1AW")},
    {'r', tr("Report"), tr("RSV report (TX tab)"), QStringLiteral("595")},
    {'o', tr("To operator"), tr("Name of the operator you are sending to (TX tab)"), QStringLiteral("Hiram")},
    {'m', tr("My call"), tr("Your call sign (Settings)"), QStringLiteral("N0CALL")},
    {'f', tr("First name"), tr("Your first name (Settings)"), QStringLiteral("Alex")},
    {'n', tr("Last name"), tr("Your last name (Settings)"), QStringLiteral("Morgan")},
    {'q', tr("QTH"), tr("Your location name (Settings)"), QStringLiteral("Newington")},
    {'l', tr("Locator"), tr("Your grid locator (Settings)"), QStringLiteral("FN31pr")},
    {'t', tr("Time"), tr("Current UTC time, hh:mm"), QStringLiteral("12:34")},
    {'d', tr("Date"), tr("Current UTC date, yyyy/MM/dd"), QStringLiteral("2026/10/22")},
    {'v', tr("Version"), tr("QSSTV version"), QStringLiteral("10.1")},
    {'x', tr("Comment 1"), tr("First comment line (TX tab)"), QStringLiteral("Comment 1")},
    {'y', tr("Comment 2"), tr("Second comment line (TX tab)"), QStringLiteral("Comment 2")},
    {'z', tr("Comment 3"), tr("Third comment line (TX tab)"), QStringLiteral("Comment 3")},
    {'s', tr("MER"), tr("Signal quality (MER, dB) of the last received DRM picture"), QStringLiteral("21")},
  };
  return table;
}

QHash<QChar, QString> sampleVars()
{
  QHash<QChar, QString> v;
  for (const MacroInfo &m : macroTable()) v.insert(m.tag, m.sample);
  return v;
}

QString unknownMacros(const QString &text)
{
  QString unknown;
  QHash<QChar, QString> known = sampleVars();
  for (int i = 0; i + 1 < text.size(); i++)
    {
      if (text.at(i) != QLatin1Char('%')) continue;
      const QChar tag = text.at(i + 1);
      i++;                                    // a "%%" is a literal percent and its second % is not a tag start
      if (tag == QLatin1Char('%') || known.contains(tag) || unknown.contains(tag)) continue;
      unknown.append(tag);
    }
  return unknown;
}

}  // namespace tpl
