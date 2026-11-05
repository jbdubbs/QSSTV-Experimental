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
#include "configdialog.h"
#include "ui_configdialog.h"
#include "gallerywidget.h"
#include "mainwindow.h"
#include "txwidget.h"
#include "rxwidget.h"
#include "filewatcher.h"
#include "touchpage.h"
#include "reflow.h"
#include <QScrollArea>
#include <QListWidget>
#include <QScroller>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>


configDialog::configDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::configDialog)
{
  ui->setupUi(this);
  ui->catWidget->attachRigController(rigControllerPtr);
  ui->configTabWidget->setCurrentIndex(0);
  if(uiMode::touch()) setupTouch();
}

/*!
  Touch shell: the thirteen tabs do not fit a phone, so the sections are a list and the section chosen is a page of its own
  (the dialog is maximized by prepareTouchWindow()). The Back key goes from the section to the list, then closes the dialog.
  Sections that a phone does not have (the repeater, the FTP upload) are left out.
*/
void configDialog::setupTouch()
{
  QTabWidget *tabs=ui->configTabWidget;
  for(QWidget *gone:{static_cast<QWidget *>(ui->repeaterTab),static_cast<QWidget *>(ui->ftpTab)})
    tabs->removeTab(tabs->indexOf(gone));
  // every section scrolls, sideways too until it has an arrangement for a narrow screen
  for(int i=0;i<tabs->count();i++)
    {
      const QString title=tabs->tabText(i);
      QWidget *page=tabs->widget(i);
      tabs->removeTab(i);
      tabs->insertTab(i,makeTouchScrollPage2D(page,tabs),title);
    }
  tabs->tabBar()->hide();
  tabs->setDocumentMode(true);

  QListWidget *list=new QListWidget(this);
  list->setFrameShape(QFrame::NoFrame);
  QFont f=list->font();
  f.setPixelSize(18);
  list->setFont(f);
  for(int i=0;i<tabs->count();i++)
    {
      QListWidgetItem *item=new QListWidgetItem(tabs->tabText(i),list);
      item->setSizeHint(QSize(0,uiMode::tapSize+12));
    }
  QScroller::grabGesture(list->viewport(),QScroller::LeftMouseButtonGesture);   // the list is dragged with a finger
  levels=new QStackedWidget(this);
  levels->addWidget(list);
  ui->verticalLayout->replaceWidget(tabs,levels);
  levels->addWidget(tabs);
  levels->setCurrentIndex(0);
  connect(list,&QListWidget::itemClicked,this,[this,tabs,list](QListWidgetItem *item)
    {
      tabs->setCurrentIndex(list->row(item));
      levels->setCurrentIndex(1);
    });
}

void configDialog::showEvent(QShowEvent *e)
{
  if(levels)
    {
      levels->setCurrentIndex(0);
      // the pages for a narrow screen (what a phone is, held either way); a tablet keeps them as they are
      const int shortSide=uiMode::shortSide(this);
      if(uiMode::narrow(shortSide))
        for(int i=0;i<ui->configTabWidget->count();i++)
          if(QScrollArea *sa=qobject_cast<QScrollArea *>(ui->configTabWidget->widget(i))) reflowNarrow(sa->widget(),shortSide-24);
    }
  QDialog::showEvent(e);
}

bool configDialog::backPressed()
{
  if(levels && levels->currentIndex()==1)
    {
      levels->setCurrentIndex(0);
      return true;
    }
  return false;
}

configDialog::~configDialog()
{
  delete ui;
}

void configDialog::readSettings()
{
  ui->cwWidget->readSettings();
  ui->directoryWidget->readSettings();
  ui->drmProfilesWidget->readSettings();
  ui->ftpWidget->readSettings();
  ui->guiWidget->readSettings();
  ui->hybridWidget->readSettings();
  ui->operatorWidget->readSettings();
  ui->repeaterWidget->readSettings();
  ui->catWidget->readSettings();
  ui->gpsWidget->readSettings();
  ui->soundWidget->readSettings();
  ui->waterfallWidget->readSettings();
  ui->freqSelectWidget->readSettings();
}

/**
    Sets the receive and transmit clock (e.g. from a calibration) and shows them in Configuration > Sound.
    soundConfig::writeSettings() takes the clocks from those fields, so they must be updated too or the
    old values would be written back over the new ones.
*/
void configDialog::setSoundClocks(double rx,double tx)
{
  rxClock=rx;
  txClock=tx;
  ui->soundWidget->setParams();
}

void configDialog::writeSettings()
{
  ui->operatorWidget->writeSettings();
  ui->directoryWidget->writeSettings();
  ui->soundWidget->writeSettings();
  ui->guiWidget->writeSettings();
  ui->catWidget->writeSettings();
  ui->gpsWidget->writeSettings();    // after CAT: it checks its port against the CAT and PTT ports
  ui->ftpWidget->writeSettings();
  ui->repeaterWidget->writeSettings();
  ui->hybridWidget->writeSettings();
  ui->drmProfilesWidget->writeSettings();
  ui->cwWidget->writeSettings();
  ui->waterfallWidget->writeSettings();
  soundNeedsRestart=ui->soundWidget->hasChanged();
  guiNeedsRestart=ui->guiWidget->hasChanged();
  ui->freqSelectWidget->writeSettings();
  if(ui->guiWidget->backGroundColorChanged)
    {
      emit bgColorChanged();
      ui->guiWidget->backGroundColorChanged=false;
    }

}

/**
    Opens the configuration dialog
*/

int configDialog::exec()
{
  if(QDialog::exec())
  {
    writeSettings();
    if(ui->guiWidget->smoothChanged)
      {
        ui->guiWidget->smoothChanged=false;
        rxWidgetPtr->getImageViewerPtr()->displayImage();
        txWidgetPtr->getImageViewerPtr()->displayImage();
        galleryWidgetPtr->changedMatrix();
      }
    if(ui->directoryWidget->hasChanged() || ui->guiWidget->hasChanged())
      {
        galleryWidgetPtr->changedMatrix();
      }
    if(ui->drmProfilesWidget->hasChanged())
      {
        txWidgetPtr->reloadProfiles();
      }
    if(ui->directoryWidget->hasChanged())
      {
        fileWatcherPtr->init();
      }
    mainWindowPtr->setSSTVDRMPushButton(transmissionModeIndex==TRXDRM);

    return QDialog::Accepted;
  }
  else
  {
    return QDialog::Rejected;
  }
}


