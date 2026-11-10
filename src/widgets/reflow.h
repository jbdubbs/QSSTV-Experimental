#ifndef REFLOW_H
#define REFLOW_H

#include <QStringList>

class QWidget;

/*!
  Arranges the widgets of a form for a narrow screen, without a second arrangement of each form: what does not fit in \a width
  (logical pixels) is put in a column. Done from the inside out, so a row is judged by what is in it once its own parts have been
  arranged:
  - a form layout wraps: the label above its field;
  - a grid that does not fit becomes one column, in reading order (the label above its field);
  - a row of widgets that does not fit becomes a column;
  - labels wrap, the text of checkboxes and radio buttons is broken into lines, combo boxes shrink to a few characters
    (a long entry is cut, not the page widened).
  A page is arranged once (the property "reflowed"); the result is not meant to be undone, the screen's shorter side decides
  (uiMode::shortSide()), which does not change when the phone is turned.
*/
void reflowNarrow(QWidget *page,int width);

//! The widgets of \a page that still want more than \a width (for tests/uishots): class, name and minimum width, one per line
QStringList reflowOffenders(QWidget *page,int width);

#endif // REFLOW_H
