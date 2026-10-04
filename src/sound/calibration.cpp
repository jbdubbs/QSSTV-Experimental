/**************************************************************************
*   Copyright (C) 2000-2019 by Johan Maes                                 *
*   on4qz@telenet.be                                                      *
*   https://www.qsl.net/o/on4qz                                           *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "calibration.h"
#include "ui_calibration.h"
#include "calibrationmethod.h"
#include "calibrationwwv.h"
#include "calibrationntp.h"
#include <QPushButton>
#include <QTabBar>

/**
 * \class calibration
 *
 * Dialog with one tab per calibration method. A method measures the sample rate of the soundcard against an
 * external time reference (the PC's own clock is not one, so it is not offered). If the OK button is pressed
 * the clocks of the active method are available through getRXClock() and getTXClock() and the caller stores them.
 *
 * Adding a method: derive from calibrationMethod and add it to init().
 **/

calibration::calibration(QWidget *parent) : QDialog(parent),  ui(new Ui::calibration)
{
  ui->setupUi(this);
  rxCardClock=0;
  txCardClock=0;
  init();
}

calibration::~calibration()
{
  stopAll();
  delete ui;
}

/**
 * @brief register the calibration methods; one line per method, in tab order
 */
void calibration::init()
{
  addMethod(new calibrationWwv(this));
  addMethod(new calibrationNtp(this));
  // addMethod(new ...);   // third method
  connect(ui->methodTabs,SIGNAL(currentChanged(int)),this,SLOT(slotTabChanged(int)));
  ui->methodTabs->tabBar()->setVisible(methods.count()>1);
  slotResultChanged();
  // the wrapped labels of a page can need more height than the .ui size leaves: never start smaller than the content
  resize(qMax(width(),sizeHint().width()),qMax(height(),sizeHint().height()));
}

void calibration::addMethod(calibrationMethod *m)
{
  if(methods.count()>=MAXCALIBRATIONMETHODS)
    {
      delete m;
      return;
    }
  methods.append(m);
  ui->methodTabs->addTab(m,m->title());
  connect(m,SIGNAL(resultChanged()),this,SLOT(slotResultChanged()));
}

calibrationMethod *calibration::activeMethod()
{
  int i=ui->methodTabs->currentIndex();
  if(i<0 || i>=methods.count()) return nullptr;
  return methods.at(i);
}

void calibration::stopAll()
{
  for(int i=0;i<methods.count();i++) methods.at(i)->stop();
}

void calibration::slotTabChanged(int)
{
  // only one method may use the soundcard at a time
  for(int i=0;i<methods.count();i++)
    if(methods.at(i)!=activeMethod()) methods.at(i)->stop();
  slotResultChanged();
}

void calibration::slotResultChanged()
{
  calibrationMethod *m=activeMethod();
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m!=nullptr && m->hasResult());
}

void calibration::accept()
{
  calibrationMethod *m=activeMethod();
  if(m==nullptr || !m->hasResult()) return;
  rxCardClock=m->rxClockResult();
  txCardClock=m->txClockResult();
  stopAll();
  QDialog::accept();
}

void calibration::reject()
{
  stopAll();
  QDialog::reject();
}
