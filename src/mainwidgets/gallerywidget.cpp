#include "gallerywidget.h"
#include "ui_gallerywidget.h"
#include "configparams.h"
#include "logging.h"
#include "appglobal.h"
#include "filewatcher.h"
#include <QSplashScreen>
#include "dispatch/dispatcher.h"
#include <QFileInfo>
#include <QStatusBar>
#include "txwidget.h"
#include "uimode.h"
#include "imagematrix.h"
#include <QComboBox>
#include <QTabBar>



galleryWidget::galleryWidget(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::galleryWidget)
{
  ui->setupUi(this);
  ui->tabWidget->setCurrentIndex(0);
  ui->templateMatrix->setSortFlag(QDir::Name);
  if(uiMode::touch())
    {
      // six tabs with scroll arrows are no way to choose on a phone: one box that names the page, and the pages built when asked for
      m_lazy=true;
      ui->tabWidget->tabBar()->hide();
      ui->tabWidget->setDocumentMode(true);
      QComboBox *selector=new QComboBox(this);
      selector->setObjectName("gallerySelector");
      for(int i=0;i<ui->tabWidget->count();i++) selector->addItem(ui->tabWidget->tabText(i));
      ui->verticalLayout->insertWidget(0,selector);
      connect(selector,QOverload<int>::of(&QComboBox::currentIndexChanged),ui->tabWidget,&QTabWidget::setCurrentIndex);
      connect(ui->tabWidget,&QTabWidget::currentChanged,this,[selector](int i)
        {
          QSignalBlocker block(selector);
          selector->setCurrentIndex(i);
        });
      connect(ui->tabWidget,&QTabWidget::currentChanged,this,&galleryWidget::slotTabShown);
    }
}

imageMatrix *galleryWidget::matrixAt(int index) const
{
  switch(index)
    {
    case 0: return ui->rxSSTVMatrix;
    case 1: return ui->rxDRMMatrix;
    case 2: return ui->txSSTVMatrix;
    case 3: return ui->txDRMMatrix;
    case 4: return ui->txStockMatrix;
    default: return ui->templateMatrix;
    }
}

imageViewer::thumbType galleryWidget::typeAt(int index) const
{
  static const imageViewer::thumbType types[6]={imageViewer::RXSSTVTHUMB,imageViewer::RXDRMTHUMB,imageViewer::TXSSTVTHUMB,
                                                imageViewer::TXDRMTHUMB,imageViewer::TXSTOCKTHUMB,imageViewer::TEMPLATETHUMB};
  return types[qBound(0,index,5)];
}

QString galleryWidget::pathAt(int index) const
{
  switch(index)
    {
    case 0: return rxSSTVImagesPath;
    case 1: return rxDRMImagesPath;
    case 2: return txSSTVImagesPath;
    case 3: return txDRMImagesPath;
    case 4: return txStockImagesPath;
    default: return templatesPath;
    }
}

int galleryWidget::indexOf(imageViewer::thumbType itype) const
{
  for(int i=0;i<6;i++) if(typeAt(i)==itype) return i;
  return -1;
}

void galleryWidget::buildMatrix(int index)
{
  matrixAt(index)->init(galleryRows,galleryColumns,pathAt(index),typeAt(index));
  m_ready[index]=true;
}

void galleryWidget::slotTabShown(int index)
{
  if(index>=0 && index<6 && !m_ready[index]) buildMatrix(index);
}

void galleryWidget::matrixListChanged(int index)
{
  if(m_ready[index]) matrixAt(index)->changed();   // a page that has not been built is scanned when it is
}

galleryWidget::~galleryWidget()
{
  writeSettings();
  delete ui;

}


void galleryWidget::init()
{
  readSettings();
}


void galleryWidget::changedMatrix()
{
  changedMatrix(imageViewer::RXSSTVTHUMB);
  changedMatrix(imageViewer::RXDRMTHUMB);
  changedMatrix(imageViewer::TXSSTVTHUMB);
  changedMatrix(imageViewer::TXDRMTHUMB);
  changedMatrix(imageViewer::TXSTOCKTHUMB);
  changedMatrix(imageViewer::TEMPLATETHUMB);
}

void galleryWidget::slotDirChanged(QString dn)
{
  if(recursiveScanDirs) {
    fileWatcherPtr->addPathRecursive(dn);
  }
  if(dn.startsWith(txStockImagesPath))
    {
      matrixListChanged(4);
    }
  if(dn.startsWith(templatesPath))
    {
      matrixListChanged(5);
      txWidgetPtr->setupTemplatesComboBox();
    }
}


void galleryWidget::changedMatrix(imageViewer::thumbType itype)
{
  const int i=indexOf(itype);
  if(i<0) return;
  // a page nobody has looked at is built when somebody does (with the settings that are then current)
  if(m_lazy && !m_ready[i] && i!=ui->tabWidget->currentIndex() && i!=5) return;
  buildMatrix(i);
}



/*!
  reads the settings (saved images for tx,rx,templates)
*/

void galleryWidget::readSettings()
{
  QSettings qSettings;
  qSettings.beginGroup ("Gallery");

  splashMessage(QString( "Loading RX images" ));
  qApp->processEvents();
  if(m_lazy)
    {
      buildMatrix(0);   // the page that is shown first, and the templates (the Transmit page lists them)
      buildMatrix(5);
      return;
    }
  buildMatrix(0);
  buildMatrix(1);

  splashMessage(QString( "Loading TX images" ));
  qApp->processEvents();
  buildMatrix(2);
  buildMatrix(3);
  buildMatrix(4);

  splashMessage(QString( "Loading Templates" ));
  buildMatrix(5);
}

/*!
  writes the settings (saved images for tx,rx,templates)
*/
void galleryWidget::writeSettings()
{
  QSettings qSettings;
  qSettings.beginGroup ( "Gallery" );
  qSettings.endGroup();
}

/*!
  setup of the user interface
*/



/*!
  closeEvent signals the dispatcher to initiate  program exit.
*/


/*!
  get the filename of a template

  \param[in] tm index of template
  \return QString containing filename, check with QString.isNull for validity
*/

QString galleryWidget::getTemplateFileName ( int tm )
{
  int i;
  QString str;
  QStringList slTemp;
  if(tm>=0)
    {
      for(i=0;i<ui->templateMatrix->getFileList().count();i++)
        {
          str=ui->templateMatrix->getFileList().at(i).absoluteFilePath();
          slTemp.append(str);
        }
      slTemp.sort();
      return tm<slTemp.count() ? slTemp.at(tm) : QString();   // the list can be shorter than a remembered index
    }
  else return QString();
}

const QStringList &galleryWidget::getFilenames()
{
  QString str;
  sl.clear();
  int i;
  for(i=0;i<ui->templateMatrix->getFileList().count();i++)
    {
      str=getTemplateFileName (i);
      QFileInfo fi(str);
      sl.append(fi.baseName());
    }
  return sl;
}

/*!
  loads an image in the rximages gallery in a new position

  \param[in] fn the filename

*/

void galleryWidget::putRxImage ( QString fn )
{
  if(transmissionModeIndex==TRXSSTV)
    {
      statusBarPtr->showMessage ( "Saved: "+fn );
      matrixListChanged(0);
    }
  else
    {
      statusBarPtr->showMessage ( "Saved: "+fn+QString("  (%1dB)").arg(lastAvgMER,0,'f',0) );
      matrixListChanged(1);
    }
}

void galleryWidget::txImageChanged()
{
  if(transmissionModeIndex==TRXSSTV) matrixListChanged(2);
  else matrixListChanged(3);
}

void galleryWidget::txStockImageChanged()
{
  matrixListChanged(4);
}


/*!
  loads an image in the rximages gallery in a new position

  \return QString containing filename, check with QString.isEmpty for validity

*/






bool galleryWidget::leaveSelectMode()
{
  bool was=false;
  for(imageMatrix *m:findChildren<imageMatrix *>()) was|=m->leaveSelectMode();
  return was;
}
