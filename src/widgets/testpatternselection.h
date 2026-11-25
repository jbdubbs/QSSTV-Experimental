#ifndef TESTPATTERNSELECTION_H
#define TESTPATTERNSELECTION_H

#include <QDialog>
#include "testpattern.h"

namespace Ui {
class testPatternSelection;
}

class testPatternSelection : public QDialog
{
  Q_OBJECT

public:
  explicit testPatternSelection(QWidget *parent = 0);
  ~testPatternSelection();
  etpSelect getSelection();

private:
  Ui::testPatternSelection *ui;
};

#endif // TESTPATTERNSELECTION_H
