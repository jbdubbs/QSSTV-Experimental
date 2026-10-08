#include "testpatternselection.h"
#include "ui_testpatternselection.h"

testPatternSelection::testPatternSelection(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::testPatternSelection)
{
  ui->setupUi(this);
}

testPatternSelection::~testPatternSelection()
{
  delete ui;
}

etpSelect testPatternSelection::getSelection()
{
 if(ui->pm5544RadioButton->isChecked()) return TPPM5544;
 else if(ui->indianHeadRadioButton->isChecked()) return TPINDIANHEAD;
 return TPSMPTE;
}
