#include "extviewer.h"
#include "ui_extviewer.h"
#include <QFileInfo>
#include <QDebug>
#include <QSettings>
#include <QScreen>

extViewer::extViewer(QWidget *parent) :   QDialog(parent),   ui(new Ui::extViewer)
{
  ui->setupUi(this);
  activeMovie=false;
  setModal(false);
  setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
}

extViewer::~extViewer()
{
  delete ui;
}


void extViewer::setup(QString fn)
{
  int fw,fh;
  // we want the original image
  ui->imViewer->stretch=true;
  ui->imViewer->setType(imageViewer::EXTVIEW);
  // synchronous load so the size is known (background decode leaves the image null)
  ui->imViewer->openImage(fn,false,false,false,false);
  fileName=fn;
  QFileInfo fi(fn);
  fw=ui->imViewer-> getImagePtr()->width();
  fh=ui->imViewer->getImagePtr()->height();
  ui->lineEdit->setText(QString("%1 %2x%3").arg(fi.fileName()).arg(fw).arg(fh));
  restoreSize(fw,fh);
}

void extViewer::setup(const QImage &img,const QString &title)
{
  ui->imViewer->stretch=true;
  ui->imViewer->setType(imageViewer::EXTVIEW);
  ui->imViewer->openImage(img);
  fileName.clear();
  ui->lineEdit->setText(QString("%1 %2x%3").arg(title).arg(img.width()).arg(img.height()));
  restoreSize(img.width(),img.height());
}

void extViewer::restoreSize(int fw,int fh)
{
  QSettings qSettings;
  qSettings.beginGroup("EXTVIEWER");
  int sw=qSettings.value("width",-1).toInt();
  int sh=qSettings.value("height",-1).toInt();
  bool maximized=qSettings.value("maximized",false).toBool();
  qSettings.endGroup();
  if ((sw>0) && (sh>0))
    {
      resize(sw,sh);
    }
  else
    {
      QRect avail=screen()->availableGeometry();
      resize(qMin(fw,avail.width()),qMin(fh,avail.height()));
    }
  if (maximized) setWindowState(Qt::WindowMaximized);
}

void extViewer::done(int r)
{
  QSettings qSettings;
  qSettings.beginGroup("EXTVIEWER");
  QSize sz=isMaximized() ? normalGeometry().size() : size();
  if (sz.isValid() && !sz.isEmpty())
    {
      qSettings.setValue("width",sz.width());
      qSettings.setValue("height",sz.height());
    }
  qSettings.setValue("maximized",isMaximized());
  qSettings.endGroup();
  QDialog::done(r);
}

