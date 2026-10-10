#include "imagematrix.h"
#include "configparams.h"
#include <QDir>
#include <QDebug>
#include <iostream>
#include <QMessageBox>
#include "txwidget.h"
#include "utils/supportfunctions.h"

#define MINCOLSIZE 32
#define MINROWSIZE 26
#define MAXCOLSIZE 64
#define MAXROWSIZE 52


imageMatrix::imageMatrix(QWidget *parent) :  QWidget(parent)
{

  parentPtr=parent;
  parentPtr->resize(511, 300);
  anchorViewer=NULL;
  verticalLayout = NULL;
  horizontalLayout=NULL;
  sortFlags=QDir::Time;
}

imageMatrix::~imageMatrix()
{
// if(verticalLayout!=NULL) delete verticalLayout;
// if( horizontalLayout!=NULL) delete horizontalLayout;
}

void imageMatrix::setupLayout()
{
  anchorViewer=NULL;
  if(verticalLayout!=NULL) delete verticalLayout;
  verticalLayout = new QVBoxLayout(parentPtr);
  verticalLayout->setObjectName(QString::fromUtf8("vt1"));
  verticalLayout->setSpacing(2);
  verticalLayout->setContentsMargins(1, 1, 1, 1);
  gridLayout = new QGridLayout();
  gridLayout->setSpacing(1);
  gridLayout->setObjectName(QString::fromUtf8("gridLo"));
  gridLayout->setSizeConstraint(QLayout::SetNoConstraint);
  verticalLayout->addLayout(gridLayout);

  horizontalLayout = new QHBoxLayout();
  horizontalLayout->setObjectName(QString::fromUtf8("htl"));
  horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
  horizontalLayout->addItem(horizontalSpacer);

  beginPushButton = new QToolButton(this);
  beginPushButton->setObjectName(QString::fromUtf8("beginPushButton"));
  QIcon icon2;
  icon2.addFile(QString::fromUtf8(":/icons/tb_gal_begin.png"), QSize(), QIcon::Normal, QIcon::Off);
  beginPushButton->setIcon(icon2);
  beginPushButton->setIconSize(QSize(32,32));
  horizontalLayout->addWidget(beginPushButton);

  prevPushButton = new QToolButton(this);
  prevPushButton->setObjectName(QString::fromUtf8("prevPushButton"));
  QIcon icon;
  icon.addFile(QString::fromUtf8(":/icons/tb_gal_prev.png"), QSize(), QIcon::Normal, QIcon::Off);
  prevPushButton->setIcon(icon);
  prevPushButton->setIconSize(QSize(32,32));
  horizontalLayout->addWidget(prevPushButton);
  pageLabel=new QLabel;
  horizontalLayout->addWidget(pageLabel);

//  horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
//  horizontalLayout->addItem(horizontalSpacer_2);
  nextPushButton = new QToolButton(this);
  nextPushButton->setObjectName(QString::fromUtf8("nextPushButton"));
  QIcon icon1;
  icon1.addFile(QString::fromUtf8(":/icons/tb_gal_next.png"), QSize(), QIcon::Normal, QIcon::Off);
  nextPushButton->setIcon(icon1);
  nextPushButton->setIconSize(QSize(32,32));
  horizontalLayout->addWidget(nextPushButton);

  endPushButton = new QToolButton(this);
  endPushButton->setObjectName(QString::fromUtf8("endPushButton"));
  QIcon icon3;
  icon3.addFile(QString::fromUtf8(":/icons/tb_gal_end.png"), QSize(), QIcon::Normal, QIcon::Off);
  endPushButton->setIcon(icon3);
  endPushButton->setIconSize(QSize(32,32));
  horizontalLayout->addWidget(endPushButton);

  horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
  horizontalLayout->addItem(horizontalSpacer_3);
  verticalLayout->addLayout(horizontalLayout);
  styleToolButtons(this);
  connect(prevPushButton,SIGNAL(clicked()),SLOT(slotPrev()));
  connect(nextPushButton,SIGNAL(clicked()),SLOT(slotNext()));
  connect(beginPushButton,SIGNAL(clicked()),SLOT(slotBegin()));
  connect(endPushButton,SIGNAL(clicked()),SLOT(slotEnd()));
}


void imageMatrix::init(int numRows, int numColumns, QString dir,imageViewer::thumbType tt)
{
  int i,j;
  rows=numRows;
  columns=numColumns;
  dirPath=dir;
  imageViewer *imv;
  setupLayout();
  for(i=0;i<rows;i++)
    {
      for(j=0;j<columns;j++)
        {
          imv = new imageViewer(this);
          imv->setType(tt);
          gridLayout->addWidget(imv, i, j, 1, 1);
          connect(imv,SIGNAL(layoutChanged()),SLOT(slotLayoutChanged()));
          connect(imv,SIGNAL(thumbClicked(imageViewer*,Qt::KeyboardModifiers)),SLOT(slotThumbClicked(imageViewer*,Qt::KeyboardModifiers)));
          connect(imv,SIGNAL(deleteSelected()),SLOT(slotDeleteSelected()));
          connect(imv,SIGNAL(toTxSelected(int)),SLOT(slotToTxSelected(int)));
        }
    }
  for (i=0;i<rows;i++)
    {
      gridLayout->setRowMinimumHeight(i,MINROWSIZE);
      gridLayout->setRowStretch(i,0);
    }

  for (i=0;i<columns;i++)
    {
      gridLayout->setColumnMinimumWidth(i,MINCOLSIZE);
      gridLayout->setColumnStretch(i,1);
    }
  currentPage=0;
  getList();
//  displayFiles();
}

bool compareFile(QFileInfo f1, QFileInfo f2) {
    return f1.lastModified() > f2.lastModified();
}

void imageMatrix::getList()
{
    QDateTime listFileTime;
    QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;

    if(!fileList.isEmpty()) {
        fileList.erase(fileList.begin(), fileList.end());
    }

    if(recursiveScanDirs) {
      flags = QDirIterator::Subdirectories;
    }

    QDirIterator it(dirPath, QDir::Files | QDir::NoSymLinks, flags);


    while (it.hasNext()) {
      it.next();
      QFileInfo f(it.fileInfo());
      if(!f.canonicalPath().endsWith("cache")) {
        fileList.append(f);
      }
    }
    std::sort(fileList.begin(), fileList.end(), compareFile);
    numPages=ceil((double)fileList.count()/(double)(rows*columns));
    if(numPages==0) numPages=1;
    slotBegin();
}

QString imageMatrix::getLastFile()
{
  if (fileList.count()>0)
    {
      return fileList.last().absoluteFilePath();
    }
  else return QString();
}

void imageMatrix::clearSelection()
{
  for(int i=0;i<gridLayout->count();i++)
    {
      ((imageViewer *)gridLayout->itemAt(i)->widget())->setSelected(false);
    }
  anchorViewer=NULL;
}

QList<imageViewer *> imageMatrix::selectedViewers()
{
  QList<imageViewer *> l;
  for(int i=0;i<rows;i++)
    {
      for(int j=0;j<columns;j++)
        {
          imageViewer *iv=(imageViewer *)gridLayout->itemAtPosition(i,j)->widget();
          if(iv->isSelected() && !iv->getFilename().isEmpty()) l.append(iv);
        }
    }
  return l;
}

// plain click selects one, Ctrl toggles, Shift selects the range from the last plain/Ctrl click
void imageMatrix::slotThumbClicked(imageViewer *iv,Qt::KeyboardModifiers mods)
{
  if((mods & Qt::ShiftModifier) && anchorViewer)
    {
      int a=gridLayout->indexOf(anchorViewer);
      int b=gridLayout->indexOf(iv);
      if(a>=0 && b>=0)
        {
          if(!(mods & Qt::ControlModifier)) clearSelection();
          anchorViewer=(imageViewer *)gridLayout->itemAt(a)->widget();
          for(int i=qMin(a,b);i<=qMax(a,b);i++)
            {
              imageViewer *v=(imageViewer *)gridLayout->itemAt(i)->widget();
              if(!v->getFilename().isEmpty()) v->setSelected(true);
            }
          return;
        }
    }
  if(mods & Qt::ControlModifier)
    {
      iv->setSelected(!iv->isSelected());
    }
  else
    {
      clearSelection();
      iv->setSelected(true);
    }
  anchorViewer=iv;
}

void imageMatrix::slotDeleteSelected()
{
  QList<imageViewer *> sel=selectedViewers();
  if(sel.isEmpty()) return;
  if(confirmDeletion)
    {
      QString msg=(sel.count()==1) ? QString("Do you want to delete the file and\n move it to the trash folder?")
                                   : QString("Do you want to delete %1 files and\n move them to the trash folder?").arg(sel.count());
      if(QMessageBox::question(this,"Delete file",msg,QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes) return;
    }
  QStringList names;
  foreach(imageViewer *iv,sel) names.append(iv->getFilename());
  foreach(QString fn,names) trash(fn,true);
  anchorViewer=NULL;
  slotLayoutChanged();   // rescan and redraw (this also drops the selection)
}

// several files to TX: they fill the grid segments in order, any surplus is ignored
void imageMatrix::slotToTxSelected(int seg)
{
  QStringList names;
  foreach(imageViewer *iv,selectedViewers()) names.append(iv->getFilename());
  if(names.isEmpty()) return;
  if(names.count()==1) txWidgetPtr->setImageToSegment(names.first(),seg);   // single image: auto/explicit segment
  else txWidgetPtr->setImages(names);
}

void imageMatrix::displayFiles()
{
  int i,j,k;
  QString tempStr;
  clearSelection();
  int offset=currentPage*rows*columns;
  pageLabel->setText(QString("   Page %1 of %2").arg(currentPage+1).arg(numPages).leftJustified(17,' '));
  for(i=0;i<rows;i++)
    {
      for(j=0;j<columns;)
        {
          k=offset+i*columns+j;
          if(k>=fileList.count())
            {
              ((imageViewer *)gridLayout->itemAtPosition(i,j)->widget())->clear();
              j++;
            }
          else
            {
           tempStr=fileList.at(k).absoluteFilePath();
           if(((imageViewer *)gridLayout->itemAtPosition(i,j)->widget())->openImage(tempStr,false,false,true,true))
             {
                j++;
             }
           else
             {
               fileList.removeAt(k);
             }
            }
        }
    }
}

void imageMatrix::changed()
{
    getList();
//    displayFiles();
}


void imageMatrix::slotPrev()
{
  if(currentPage!=0) currentPage--;
  displayFiles();
}

void imageMatrix::slotNext()
{
  currentPage++;
  if(currentPage>=numPages)
    {
      currentPage--;
    }
  displayFiles();
}

void imageMatrix::slotBegin()
{
  currentPage=0;
  displayFiles();
}

void imageMatrix::slotEnd()
{
  currentPage=numPages-1;
  if(currentPage<0) currentPage=0;
  displayFiles();
}

void imageMatrix::slotLayoutChanged()
{
  int curPag=currentPage;
  getList();
  if(curPag>=numPages)
    {
      if(curPag>0) curPag--;
    }
  currentPage=curPag;
  displayFiles();
}
