/***************************************************************************
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
#include "imageviewer.h"
#include "appglobal.h"
#include "logging.h"
#include "configparams.h"
#include "dispatcher.h"
#include "dirdialog.h"
#include "extviewer.h"
#include "jp2io.h"
#include "supportfunctions.h"
#include <configdialog.h>
#include "drm.h"
#include "txwidget.h"
#include "gallerywidget.h"

#ifdef IMAGETESTVIEWER
#include "templateviewer.h"
#endif

#include <QStatusBar>
#include <QtGui>
#include <QMenu>

#define RATIOSCALE 1.
#define MAXSEGMENTS 4


/**
  \class imageViewer

  The image is stored in it's original format and size.
  All interactions are done on the original image.
  A scaled version is used to display the contents.
*/	


imageViewer::imageViewer(QWidget *parent): QLabel(parent)
{
  addToLog("image creation",LOGIMAG);
  validImage=false;
  orgWidth=0;
  orgHeight=0;
  gridCols=1;
  gridRows=1;
  activeSeg=0;
  selected=false;
  dragging=false;
  dragMoved=false;
  segImages.resize(MAXSEGMENTS);
  segFiles.resize(MAXSEGMENTS);
  setFrameStyle(QFrame::Sunken | QFrame::Panel);
  QBrush b;
  QPalette palette;
  b.setTexture(QPixmap::fromImage(QImage(":/icons/transparency.png")));
  palette.setBrush(QPalette::Active,QPalette::Base,b);
  palette.setBrush(QPalette::Inactive,QPalette::Base,b);
  palette.setBrush(QPalette::Disabled,QPalette::Base,b);
  setPalette(palette);
  setBackgroundRole(QPalette::Base);
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  setAspectMode(Qt::IgnoreAspectRatio);
  setBackgroundRole(QPalette::Dark);

  popup=new QMenu (this);
  newAct = new QAction(tr("&New"),this);
  connect(newAct, SIGNAL(triggered()), this, SLOT(slotNew()));
  loadAct = new QAction(tr("&Load"), this);
  connect(loadAct, SIGNAL(triggered()), this, SLOT(slotLoad()));
  toTXAct = new QAction(tr("&To TX"), this);
  connect(toTXAct, SIGNAL(triggered()), this, SLOT(slotToTX()));
  toTXMenu = new QMenu(tr("&To TX"), this);
  connect(toTXMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotToTXSegment(QAction*)));
  editAct = new QAction(tr("&Edit"), this);
  connect(editAct, SIGNAL(triggered()), this, SLOT(slotEdit()));
  printAct = new QAction(tr("&Print"), this);
  connect(printAct, SIGNAL(triggered()), this, SLOT(slotPrint()));
  uploadAct = new QAction(tr("&Upload to FTP"), this);
  connect(uploadAct, SIGNAL(triggered()), this, SLOT(slotUploadFTP()));
  deleteAct = new QAction(tr("&Delete"), this);
  connect(deleteAct, SIGNAL(triggered()), this, SLOT(slotDelete()));
  viewAct = new QAction(tr("&View"), this);
  connect(viewAct, SIGNAL(triggered()), this, SLOT(slotView()));
  propertiesAct = new QAction(tr("Propert&ies"), this);
  connect(propertiesAct, SIGNAL(triggered()), this, SLOT(slotProperties()));
  zoomInAct = new QAction(tr("Zoom In (&+)"), this);
  connect(zoomInAct, SIGNAL(triggered()), this, SLOT(slotZoomIn()));
  zoomOutAct = new QAction(tr("Zoom Out (&-)"), this);
  connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(slotZoomOut()));
  copyAct = new QAction(tr("&Copy"), this);
  copyAct->setShortcut(QKeySequence::Copy);
  copyAct->setShortcutContext(Qt::WidgetShortcut);
  connect(copyAct, SIGNAL(triggered()), this, SLOT(slotCopy()));
  pasteAct = new QAction(tr("&Paste"), this);
  pasteAct->setShortcut(QKeySequence::Paste);
  pasteAct->setShortcutContext(Qt::WidgetShortcut);
  connect(pasteAct, SIGNAL(triggered()), this, SLOT(slotPaste()));
  setFocusPolicy(Qt::ClickFocus);
  connect(configDialogPtr,SIGNAL(bgColorChanged()), SLOT(slotBGColorChanged()));
  clickTimer.setSingleShot(true);
  clickTimer.setInterval(40);
  connect(&clickTimer, SIGNAL(timeout()), this, SLOT(slotLeftClick()));

  init(RXIMG);
  activeMovie=false;
  stretch=false;
  //
}

imageViewer::~imageViewer()
{
}

void imageViewer::init(thumbType tp)
{
  setScaledContents(false);
  setAlignment(Qt::AlignCenter);
  setAutoFillBackground(true);
  slotBGColorChanged();
  addToLog(QString("image creation %1").arg(tp),LOGIMAG);
  setType(tp);
  setPixmap(QPixmap());
  clear();
}

bool imageViewer::openImage(QString &filename,QString start,bool ask,bool showMessage,bool temitSignal,bool fromCache,bool background)
{
  //background=false;
  tempFilename=filename;
  emitSignal=temitSignal;
  QFile fi(tempFilename);
  QFileInfo finf(tempFilename);

  jp2IO jp2;
  editorScene ed;
  bool success=false;
  cacheHit=false;

  if(activeMovie)
    {
      activeMovie=false;
      qm.stop();
    }
  if (tempFilename.isEmpty()&&!ask) return false;
  if(ask)
    {
      dirDialog dd(static_cast<QWidget *>(this),"Browse");
      tempFilename=dd.openFileName(start,pictureNameFilter());
    }
  if(tempFilename.isEmpty())
    {
      imageFileName="";
      return false;
    }

  if(fromCache)
    {
      cachePath=finf.absolutePath()+"/cache/";
      QDir dd(cachePath);
      if(!dd.exists())
        {
          dd.mkpath(cachePath);
        }
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
      cacheFileName=cachePath+finf.baseName()+finf.created().toString()+".png";
#else
      cacheFileName=cachePath+finf.baseName()+finf.birthTime().toString()+".png";
#endif
      if(tempImage.load(cacheFileName))
        {
          cacheHit=true;
          success=true;
          orgWidth=tempImage.text("orgWidth").toInt();
          orgHeight=tempImage.text("orgHeight").toInt();
        }
      else
        {
          success=false;
        }
    }
  if(!success)
    {
      if(jp2.check(tempFilename))
        {
          if(background)
            {
              threadIm = new QThread;
              threadIm->setObjectName("jpeg2000");
              jp2Ptr=new jp2IO;
              jp2Ptr->setParams(&tempImage,tempFilename,fromCache);
              jp2Ptr->moveToThread(threadIm);
              connect(threadIm, SIGNAL(started()), jp2Ptr, SLOT(slotStart()));
              connect(jp2Ptr, SIGNAL(done(bool,bool)), this,   SLOT(slotJp2ImageDone(bool,bool)));
              connect( threadIm, SIGNAL(finished()), jp2Ptr, SLOT(deleteLater()));
              connect( threadIm, SIGNAL(finished()), threadIm, SLOT(deleteLater()));
              threadIm->start();
              return true;
            }
          else
            {
              tempImage=jp2.decode(tempFilename);
              if(!tempImage.isNull())
                {
                  success=true;
                }
            }
        }
      else if(loadImageAutoRotate(tempImage,tempFilename))
        {
          success=true;
        }
      else if(ed.load(fi))
        {
          success=true;
          tempImage=QImage(ed.renderImage(0,0)->copy());
        }
    }




  return processImageDisplay(success,showMessage,fromCache);
}

bool  imageViewer::processImageDisplay(bool success,bool showMessage,bool fromCache)
{
  displayMBoxEvent *stmb=0;
  if(!success)
    {
      if(showMessage)
        {
          stmb= new displayMBoxEvent("Image Loader",QString("Unable to load image:\n%1").arg(tempFilename));
          QApplication::postEvent( dispatcherPtr, stmb );  // Qt will delete it when done
        }
      validImage=false;
      imageFileName="";
      return false;
    }

  if(fromCache)
    {
      sourceImage=QImage();

      if(!cacheHit)
        {
          orgWidth=tempImage.width();
          orgHeight=tempImage.height();
          tempImage=tempImage.scaledToWidth(120, Qt::FastTransformation);
          // save cacheImage for next time
          tempImage.setText("orgWidth",QString::number(orgWidth));
          tempImage.setText("orgHeight",QString::number(orgHeight));
          tempImage.save(cacheFileName,"PNG");
        }
      stretch=true;
      displayedImage=tempImage;
    }
  else
    {
      sourceImage=tempImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
      QPainter painter(&sourceImage);
      painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
      painter.fillRect(sourceImage.rect(), imageBackGroundColor);
      painter.end();
      orgWidth=tempImage.width();
      orgHeight=tempImage.height();
      displayedImage=sourceImage;
      if(gridActive())
        {
          captureSegment(sourceImage,tempFilename);
          applyTemplate();   // stitch the segments into the frame
        }
      //#ifdef IMAGETESTVIEWER
      //      imageTestViewer(&displayedImage,"processImage");
      //#endif
    }
  view=QRect();
  imageFileName=tempFilename;

  QFileInfo finfo(tempFilename);
  if (finfo.suffix().toLower()=="gif")
    {
      //we will try a animated gif
      qm.setFileName(tempFilename);
      if(qm.isValid())
        {
          if(qm.frameCount()>1)
            {
              activeMovie=true;
              setMovie(&qm);
              qm.start();
              displayedImage=QImage();
            }
          else
            {
              displayImage();  // we have a single image gif
            }
        }
    }
  else
    {
      displayImage();
    }
  validImage=true;
  if (emitSignal) emit imageChanged();
  return true;
}


void imageViewer::slotJp2ImageDone(bool success,bool fromCache)
{
  cacheHit=false; // force creation of cache file
  processImageDisplay(success,false,fromCache);
  threadIm->exit(0);
}


bool imageViewer::openImage(QString &filename,bool showMessage,bool emitSignal,bool fromCache,bool background)
{
  return openImage(filename,"",false,showMessage,emitSignal,fromCache,background);
}

bool imageViewer::openImage(QImage im)
{
  imageFileName="";
  if(!im.isNull())
    {
      validImage=true;
      sourceImage=im;
      displayedImage=im;
      compressedImageData.clear();
      if(gridActive())
        {
          captureSegment(im,QString());
          applyTemplate();
        }
      displayImage();
      return true;
    }
  validImage=false;
  return false;
}

bool imageViewer::openImage(QByteArray *ba)
{
  QImage tempImage;
  QBuffer buffer(ba);
  buffer.open(QIODevice::ReadOnly);
  if(loadImageAutoRotate(tempImage,&buffer))
    {
      return  openImage(tempImage.convertToFormat(QImage::Format_ARGB32_Premultiplied));
    }
  validImage=false;
  return false;
}

void imageViewer::clear()
{
  validImage=false;
  orgWidth=0;
  orgHeight=0;
  imageFileName.clear();
  sourceImage=QImage();
  displayedImage=QImage();
  for(int i=0;i<segImages.size();i++) segImages[i]=QImage();
  for(int i=0;i<segFiles.size();i++) segFiles[i].clear();
  compressedImageData.clear();
  view=QRect();
  setPixmap(QPixmap());
  targetWidth=0;
  targetHeight=0;
  templateFileName.clear();
  useTemplate=false;
}

bool imageViewer::hasValidImage()
{
  return validImage;
}

void imageViewer::createImage(QSize sz,QColor fill,bool scale)
{
  clear();
  displayedImage=QImage(sz,QImage::Format_ARGB32_Premultiplied);
  if(!displayedImage.isNull())
    {
      displayedImage.fill(fill);
      useCompression=false;
    }
  stretch=scale;
  displayImage();
  emit imageChanged();
}

//void imageViewer::copy(imageViewer *src)
//{
//  imageFileName=src->imageFileName;
//  ttype=src->ttype;
//  openImage(imageFileName,false,false,false);
//}



QRgb *imageViewer::getScanLineAddress(int line)
{
  return (QRgb *)displayedImage.scanLine(line);
}



// RX picture only: the "Smooth Display Scaling" option (RX tab) can switch smoothing off 
Qt::TransformationMode imageViewer::scaleMode() const
{
  if((ttype==RXIMG) && !rxSmoothDisplayScaling) return Qt::FastTransformation;
  return Qt::SmoothTransformation;
}

void imageViewer::displayImage()
{
  if(displayedImage.isNull())
    {
      return;
    }
  if (view.isNull()) {
      if(hasScaledContents() || (displayedImage.width()>width()) || (displayedImage.height()>height()) || stretch || ttype==TXIMG)
        {
          setPixmap(withGridOverlay(QPixmap::fromImage(displayedImage.scaled(width()-2,height()-2,Qt::KeepAspectRatio,scaleMode()))));
        }
      else
        {
          setPixmap(withGridOverlay(QPixmap::fromImage(displayedImage)));
        }

    }
  else
    {
      QImage im = displayedImage.copy(view);
      setPixmap(QPixmap::fromImage(im.scaled(width()-2,height()-2,Qt::KeepAspectRatio,scaleMode())));

    }

}

void imageViewer::zoom(const QPoint centre, int dlevel)
{
  addToLog(QString("centre=%1,%2 dlevel=%3").arg(centre.x()).arg(centre.y()).arg(dlevel),LOGIMAG);
  if (view.isNull()) {
      view=displayedImage.rect();
    }

  while ((dlevel!=0) && (view.width()<=displayedImage.width())) {
      if (dlevel>0) {
          // halve the size of the viewed area
          if ((view.width()>96) && (view.height()>96)) {
              addToLog("zoom in",LOGIMAG);
              view.adjust(+view.width()/4, +view.height()/4, -view.width()/4, -view.height()/4);
            }
          dlevel--;
        }
      else {
          // double the size of the viewed area
          addToLog("zoom out",LOGIMAG);
          view.adjust(-view.width()/2, -view.height()/2, +view.width()/2, +view.height()/2);
          dlevel++;
        }
    }

  if ((view.width()  > displayedImage.width()) ||
      (view.height() > displayedImage.height())
      ) {
      view=displayedImage.rect();
    }
  else {
      view.moveCenter(centre);

      // ensure the view is within the image
      if (view.x()<0) view.moveLeft(0);
      if (view.y()<0) view.moveTop(0);
      if (view.x()+view.width() > displayedImage.width()) view.moveRight(displayedImage.width());
      if (view.y()+view.height() > displayedImage.height()) view.moveBottom(displayedImage.height());
    }
  addToLog(QString("View:%1,%2,%3,%4").arg(view.x()).arg(view.y()).arg(view.width()).arg(view.height()),LOGIMAG);
  displayImage();
}

QPoint imageViewer::mapToImage(const QPoint &pos)
{
  QRect cr = contentsRect();

  cr.adjust(margin(),margin(),-margin(),-margin());
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QPixmap p=pixmap(Qt::ReturnByValue);
  QRect aligned = QStyle::alignedRect(QApplication::layoutDirection(), QFlag(alignment()), p.size(), cr);
#else
  QRect aligned = QStyle::alignedRect(QApplication::layoutDirection(), QFlag(alignment()), pixmap()->size(), cr);
#endif
  QRect inter = aligned.intersected(cr);

  QPoint c = pos;
  c-=inter.topLeft();

  if (view.isNull())
    {
      c.setX(displayedImage.width()  * c.x()/inter.width());
      c.setY(displayedImage.height() * c.y()/inter.height());
    }
  else {
      c.setX(view.width()  * c.x()/inter.width());
      c.setY(view.height() * c.y()/inter.height());
    }

  c+=view.topLeft();
  return c;
}

void imageViewer::setType(thumbType tp)
{
  ttype=tp;
  switch(ttype)
    {
    case RXIMG:
    case EXTVIEW:
    case PREVIEW:
    case RXSSTVTHUMB:
      imageFilePath=rxSSTVImagesPath;
      break;
    case RXDRMTHUMB:
      imageFilePath=rxDRMImagesPath;
      break;

    case TXSSTVTHUMB:
      imageFilePath=txSSTVImagesPath;
      break;
    case TXDRMTHUMB:
      imageFilePath=txDRMImagesPath;
      break;
    case TXIMG:
    case TXSTOCKTHUMB:
      imageFilePath=txStockImagesPath;
      break;
    case TEMPLATETHUMB:
      imageFilePath=templatesPath;
      break;

    }
  if((tp==RXSSTVTHUMB) || (tp==RXDRMTHUMB) || (tp==TXSSTVTHUMB) || (tp==TXDRMTHUMB) ||(tp==TXSTOCKTHUMB) ||(tp==TEMPLATETHUMB))
    {
      setScaledContents(false);
      setAlignment(Qt::AlignCenter);
    }
  popup->removeAction(newAct);
  popup->removeAction(loadAct);
  popup->removeAction(toTXAct);
  popup->removeAction(editAct);
  popup->removeAction(printAct);
  popup->removeAction(uploadAct);
  popup->removeAction(deleteAct);
  popup->removeAction(viewAct);
  popup->removeAction(propertiesAct);
  popup->removeAction(copyAct);
  popup->removeAction(pasteAct);
  switch(tp)
    {
    case EXTVIEW:
      popup->addAction(zoomInAct);
      popup->addAction(zoomOutAct);
      popup->addAction(propertiesAct);
      break;

    case RXIMG:
      popup->addAction(viewAct);
      popup->addAction(propertiesAct);
      break;

    case TXIMG:
      popup->addAction(newAct);
      popup->addAction(loadAct);
      popup->addAction(editAct);
      popup->addAction(printAct);
      popup->addAction(viewAct);
      popup->addAction(propertiesAct);
      popup->addAction(deleteAct);   // removes the image (or grid segment) from TX, the file is kept
      break;

    case PREVIEW:
      popup->addAction(loadAct);
      popup->addAction(toTXAct);
      popup->addAction(viewAct);
      popup->addAction(propertiesAct);
      break;

    case RXSSTVTHUMB:
    case RXDRMTHUMB:
      popup->addAction(uploadAct);
      popup->addAction(toTXAct);
      popup->addAction(printAct);
      popup->addAction(deleteAct);
      popup->addAction(viewAct);
      popup->addAction(propertiesAct);
      break;
    case TXSSTVTHUMB:
    case TXDRMTHUMB:
      popup->addAction(toTXAct);
      popup->addAction(printAct);
      popup->addAction(deleteAct);
      popup->addAction(viewAct);
      popup->addAction(propertiesAct);
      break;

    case TXSTOCKTHUMB:
    case TEMPLATETHUMB:
      popup->addAction(newAct);
      popup->addAction(loadAct);
      popup->addAction(toTXAct);
      popup->addAction(editAct);
      popup->addAction(printAct);
      popup->addAction(deleteAct);
      popup->addAction(viewAct);
      popup->addAction(propertiesAct);
      break;
    }
  if(tp==TXIMG || tp==TXSTOCKTHUMB)
    {
      popup->addSeparator();
      popup->addAction(copyAct);
      popup->addAction(pasteAct);
    }
  else
    {
      popup->addSeparator();
      popup->addAction(copyAct);
    }
  popupEnabled=true;

}

void imageViewer::mousePressEvent( QMouseEvent *e )
{
  if (e->button() == Qt::LeftButton)
    {
      if (e->type() == QEvent::MouseButtonDblClick)
        {
          clickTimer.stop();
          if (ttype==EXTVIEW)
            {
              //              if (pixmap())
              if(hasValidImage())
                {
                  QPoint c = mapToImage(e->pos());
                  if (e->modifiers() & Qt::ShiftModifier)
                    zoom(c, -1);
                  else
                    zoom(c, +1);
                }
            }
          else
            {
              if (hasValidImage()) slotView();
            }
        }
      else if (e->type() == QEvent::MouseButtonPress)
        {
          setFocus();
          if(isGalleryThumb()) emit thumbClicked(this,e->modifiers());
          if(gridActive())
            {
              selectSegment(segmentAt(e->pos()));
              displayImage();
            }
          if (ttype==EXTVIEW)
            {
              //              if (pixmap())
              if(hasValidImage())
                {
                  clickPos = mapToImage(e->pos());
                  dragging=true;
                  dragMoved=false;
                  dragView=view;
                  dragImagePt=clickPos;
                }
            }
        }
    }
  else if (e->button() == Qt::RightButton)
    {
      if(popupEnabled)
        {
          setFocus();
          if(isGalleryThumb() && !selected) emit thumbClicked(this,Qt::NoModifier);   // right-click outside the selection selects just this one
          if(gridActive())
            {
              selectSegment(segmentAt(e->pos()));
              displayImage();
            }
          copyAct->setEnabled(gridActive() ? !segImages.value(activeSeg).isNull() : (hasValidImage() || !displayedImage.isNull()));
          pasteAct->setEnabled(!clipboardImage().isNull());
          if(ttype==TXIMG) deleteAct->setEnabled(gridActive() ? !segImages.value(activeSeg).isNull() : hasValidImage());
          //              if (pixmap())
          // with a TX grid the To TX entry becomes a submenu: Auto / Grid 1..N
          popup->removeAction(toTXMenu->menuAction());
          toTXAct->setVisible(true);
          int nseg=txWidgetPtr->txGridSegments();
          if(nseg>1 && popup->actions().contains(toTXAct))
            {
              toTXMenu->clear();
              toTXMenu->addAction(tr("Auto"))->setData(-1);
              for(int i=0;i<nseg;i++) toTXMenu->addAction(tr("Grid %1").arg(i+1))->setData(i);
              popup->insertMenu(toTXAct,toTXMenu);
              toTXAct->setVisible(false);
            }
          if(hasValidImage())

            clickPos = mapToImage(e->pos());
          popup->popup(QCursor::pos());
        }
    }
}

// EXTVIEW: the wheel zooms about the cursor
void imageViewer::wheelEvent(QWheelEvent *e)
{
  if(ttype!=EXTVIEW || !hasValidImage() || displayedImage.isNull())
    {
      QLabel::wheelEvent(e);
      return;
    }
  int dy=e->angleDelta().y();
  if(dy==0) return;
  QPoint pos=e->position().toPoint();
  QPoint p=mapToImage(pos);
  QRect before=view.isNull() ? displayedImage.rect() : view;
  // smooth zoom: 1.15x per standard wheel notch (120 units), down to a 48 pixel wide view
  double factor=qPow(1.15,dy/120.0);
  double nw=qBound(48.0,before.width()/factor,(double)displayedImage.width());
  double scale=nw/before.width();
  QSize ns(qMax(1,qRound(before.width()*scale)),qMax(1,qRound(before.height()*scale)));
  if(ns.width()>=displayedImage.width() || ns.height()>=displayedImage.height())
    {
      view=QRect();   // fully zoomed out
    }
  else
    {
      // keep the image point under the cursor where it was
      double fx=(p.x()-before.x())/(double)before.width();
      double fy=(p.y()-before.y())/(double)before.height();
      QRect v(QPoint(p.x()-qRound(fx*ns.width()),p.y()-qRound(fy*ns.height())),ns);
      v.moveLeft(qBound(0,v.x(),displayedImage.width()-v.width()));
      v.moveTop(qBound(0,v.y(),displayedImage.height()-v.height()));
      view=v;
    }
  displayImage();
  e->accept();
}

// EXTVIEW: dragging with the left button pans the zoomed image
void imageViewer::mouseMoveEvent(QMouseEvent *e)
{
  if(ttype!=EXTVIEW || !dragging || !(e->buttons() & Qt::LeftButton) || dragView.isNull())
    {
      QLabel::mouseMoveEvent(e);
      return;
    }
  QRect cur=view;
  view=dragView;
  QPoint now=mapToImage(e->pos());   // same mapping as at press time
  view=cur;
  QPoint d=dragImagePt-now;
  if(!dragMoved && d.manhattanLength()<3) return;
  if(!dragMoved) setCursor(Qt::ClosedHandCursor);
  dragMoved=true;
  QRect v=dragView.translated(d);
  v.moveLeft(qBound(0,v.x(),displayedImage.width()-v.width()));
  v.moveTop(qBound(0,v.y(),displayedImage.height()-v.height()));
  view=v;
  displayImage();
}

void imageViewer::mouseReleaseEvent(QMouseEvent *e)
{
  if(ttype==EXTVIEW && dragging && e->button()==Qt::LeftButton)
    {
      dragging=false;
      unsetCursor();
      if(!dragMoved) clickTimer.start();   // a plain click still recentres, a drag does not
      return;
    }
  QLabel::mouseReleaseEvent(e);
}

void imageViewer::slotLeftClick()
{
  switch (ttype)
    {
    case EXTVIEW:
      zoom(clickPos, 0);
      break;
    default:
      break;
    }
}

void imageViewer::slotDelete()
{
  if(selected && isGalleryThumb())
    {
      emit deleteSelected();
      return;
    }
  if(ttype==TXIMG)
    {
      clearTxImage();
      return;
    }
  int exit=QMessageBox::Yes;
  if(imageFileName.isEmpty()) return;
  if(confirmDeletion)
    {
      exit=QMessageBox::question(this,"Delete file","Do you want to delete the file and\n move it to the trash folder?",QMessageBox::Yes|QMessageBox::No);
    }
  if(exit==QMessageBox::Yes)
    {
      trash(imageFileName,true);
    }

  imageFileName="";
  emit layoutChanged();
}

// TX image: drop the image (or just the selected grid segment) without touching any file
void imageViewer::clearTxImage()
{
  if(gridActive())
    {
      segImages[activeSeg]=QImage();
      segFiles[activeSeg].clear();
      selectSegment(activeSeg);
      applyTemplate();
    }
  else
    {
      if(!hasValidImage()) return;
      QImage blank((targetWidth>0 && targetHeight>0) ? QSize(targetWidth,targetHeight) : QSize(320,256),QImage::Format_ARGB32_Premultiplied);
      blank.fill(imageBackGroundColor);
      openImage(blank);
      validImage=true;
    }
  displayImage();
  emit imageChanged();
}

void imageViewer::slotEdit()
{
  if(imageFileName.isEmpty())
    {
      slotLoad();
      if (imageFileName.isEmpty()) return;
    }
  callEditorEvent *ce = new callEditorEvent( this,imageFileName );
  QApplication::postEvent(dispatcherPtr, ce );  // Qt will delete it when done
}


void imageViewer::slotLoad()
{
  QString fileNameTmp;
  dirDialog dd((QWidget *)this,"Browse");
  fileNameTmp=dd.openFileName(imageFilePath,pictureNameFilter());
  if(fileNameTmp.isEmpty()) return;
  if(openImage(fileNameTmp,true,false,false,true))
    {
      if((ttype==TXSTOCKTHUMB) || (ttype==TEMPLATETHUMB))
        {
          // the gallery is a directory listing: import the file so it persists across restarts
          QFileInfo src(fileNameTmp);
          QDir dstDir(imageFilePath);
          if(src.absoluteDir()!=dstDir)
            {
              QString dst=dstDir.filePath(src.fileName());
              for(int i=1;QFile::exists(dst);i++)
                {
                  dst=dstDir.filePath(QString("%1-%2.%3").arg(src.completeBaseName()).arg(i).arg(src.suffix()));
                }
              if(QFile::copy(fileNameTmp,dst))
                {
                  fileNameTmp=dst;
                }
              else
                {
                  addToLog(QString("Unable to copy %1 to %2").arg(fileNameTmp).arg(dst),LOGIMAG);
                }
            }
        }
      imageFileName=fileNameTmp;
      if(ttype==TEMPLATETHUMB)
        {
          templatesChangedEvent *ce = new templatesChangedEvent( );
          QApplication::postEvent(dispatcherPtr, ce );  // Qt will delete it when done
        }
      else if((ttype==TXIMG) ||(ttype==PREVIEW))
        {
          emit imageChanged();
        }

    }
}



void imageViewer::slotNew()
{
  callEditorEvent *ce = new callEditorEvent( this,NULL);
  QApplication::postEvent(dispatcherPtr, ce );  // Qt will delete it when done
}


void imageViewer::slotPrint()
{
}


void imageViewer::slotUploadFTP()
{
  QString remoteDir;
  switch (ttype) {
    case RXSSTVTHUMB:
      remoteDir = ftpRemoteSSTVDirectory;
      break;
    case RXDRMTHUMB:
      remoteDir = ftpRemoteDRMDirectory;
      break;
    default:
      break;
    }
  if (!remoteDir.isEmpty())
    dispatcherPtr->uploadToRXServer(remoteDir, imageFileName);
}


void imageViewer::slotView()
{
  // parentless and non-modal so the window manager treats it as an independent window
  bool haveFile=!imageFileName.isEmpty() && QFileInfo::exists(imageFileName);
  QImage img;
  if(!haveFile)
    {
      // live RX image has no file: show the in-memory image
      img=sourceImage.isNull() ? displayedImage : sourceImage;
      if(gridActive()) img=segImages.value(activeSeg);   // only the selected segment
      if(img.isNull()) return;
    }
  extViewer *vm=new extViewer(nullptr);
  vm->setAttribute(Qt::WA_DeleteOnClose);
  if(haveFile) vm->setup(imageFileName);
  else vm->setup(img,tr("RX image"));
  vm->show();
}


void imageViewer::slotBGColorChanged()
{
  QPalette mpalette;
  mpalette.setColor(QPalette::Window,backGroundColor);
  setBackgroundRole(QPalette::Window);
  mpalette.setColor(QPalette::WindowText, Qt::yellow);
  setPalette(mpalette);
}

void imageViewer::slotProperties()
{
  // the live RX image is built in memory, so orgWidth/orgHeight were never set for it
  int orgWidth=this->orgWidth;
  int orgHeight=this->orgHeight;
  if(orgWidth<=0 || orgHeight<=0)
    {
      const QImage &img=gridActive() ? segImages.value(activeSeg) : (sourceImage.isNull() ? displayedImage : sourceImage);
      orgWidth=img.width();
      orgHeight=img.height();
    }
  if(orgWidth<=0 || orgHeight<=0)
    {
      QMessageBox::information(this,"Image Properties","No image",QMessageBox::Ok);
      return;
    }
  QFileInfo fi(imageFileName);
  if(fi.exists())
    {
      QMessageBox::information(this,"Image Properties",
                               "File: " + imageFileName
                               + "\n File size:     " + QString::number(fi.size())
                               + "\n Image width:   " + QString::number(orgWidth)
                               + "\n Image height:  " + QString::number(orgHeight)
                               + "\n Last Modified: " + fi.lastModified().toString()
                               ,QMessageBox::Ok);
    }
  else
    {
      QMessageBox::information(this,"Image Properties",
                               " Image width:   " + QString::number(orgWidth)
                               + "\n Image height:  " + QString::number(orgHeight)
                               ,QMessageBox::Ok);
    }

}

void imageViewer::slotZoomIn()
{
  zoom(clickPos,+1);
}

void imageViewer::slotZoomOut()
{
  zoom(clickPos,-1);
}


void imageViewer::slotToTX()
{
  toTx(-1);
}

void imageViewer::slotToTXSegment(QAction *a)
{
  toTx(a->data().toInt());
}

// seg: TX grid segment, -1 = first empty one (else 0)
void imageViewer::toTx(int seg)
{
  if(selected && isGalleryThumb())
    {
      emit toTxSelected(seg);
      return;
    }
  moveToTxEvent *mt=0;
  addToLog(QString("ToTx: %1").arg(imageFileName),LOGTXMAIN);
  mt=new moveToTxEvent(imageFileName,seg);
  QApplication::postEvent(dispatcherPtr, mt); // Qt will delete it when done
}


void imageViewer::save(QString fileName,QString fmt,bool convertRGB, bool source)
{
  QImage im;
  if(source)
    {
      if(sourceImage.isNull()) return;
    }
  else
    {
      if(displayedImage.isNull()) return;
    }
  if(!convertRGB)
    {
      if(source) im=sourceImage;
      else im=displayedImage;
    }
  else
    {
      if(source) im=sourceImage.convertToFormat(QImage::Format_RGB32);
      else im=displayedImage.convertToFormat(QImage::Format_RGB32);
    }
  im.save(fileName,fmt.toUpper().toLatin1().data());
}

bool imageViewer::copyToBuffer(QByteArray *ba)
{
  QImage im;
  QImage cvimg;
  jp2IO jp2;
  int fileSize;
  cvimg=displayedImage.convertToFormat(QImage::Format_RGB32);
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
  int compressionRatio=round(cvimg.byteCount()/compressSize);
#else
  int compressionRatio=round(cvimg.sizeInBytes()/compressSize);
#endif
  if(displayedImage.isNull())
    {
      return false;
    }
  cvimg=displayedImage.convertToFormat(QImage::Format_RGB32);
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
  compressionRatio=cvimg.byteCount()/compressSize;
#else
  compressionRatio=cvimg.sizeInBytes()/compressSize;
#endif

  if (compressedImageData.isEmpty())
    {
      compressedImageData=jp2.encode(cvimg,im,fileSize,compressionRatio);
    }
  *ba = compressedImageData;
  if (compressedImageData.isEmpty())
    {
      return false;
    }
  return true;
}


uint  imageViewer:: setSize(int tcompressSize, bool usesCompression)
{
  compressSize=tcompressSize; //always set it
  if(!usesCompression)
    {
      applyTemplate();
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
      fileSize=sourceImage.byteCount();
#else
      fileSize=sourceImage.sizeInBytes();
#endif
    }
  else
    {
      fileSize=applyTemplate();
    }
  return fileSize;

}

bool imageViewer::reload()
{
  return openImage(imageFileName ,true,false,false,true);
}


void imageViewer::setParam(QString templateFn,bool usesTemplate,int width,int height)
{
  targetWidth=width;
  targetHeight=height;
  templateFileName=templateFn;
  useTemplate=usesTemplate;
  applyTemplate();
  displayImage();
}

void imageViewer::setAspectMode(Qt::AspectRatioMode mode)
{
  aspectRatioMode = mode;
}

int imageViewer::applyTemplate()
{
  //  qDebug() << "applyTemplate";
  QImage *resultImage;
  jp2IO jp2;
  QImage overlayedImage;
  int tWidth=targetWidth,tHeight=targetHeight;
  int compRatio;
  int byteCount;

  if(gridActive() && transmissionModeIndex==TRXSSTV) sourceImage=composeGrid();
  if(sourceImage.isNull()) return 0;
  QFile fi(templateFileName);
  if(ttype!=TXIMG) return 0;
  editorScene tscene(0);
  resultImage=&sourceImage;
  if(transmissionModeIndex==TRXDRM)
    {
      useCompression=true;
    }
  else
    {
      useCompression=false;
    }
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
  compRatio=((sourceImage.convertToFormat(QImage::Format_RGB32).byteCount()*3)/4)/compressSize; // first estimate without template
#else
  compRatio=((sourceImage.convertToFormat(QImage::Format_RGB32).sizeInBytes()*3)/4)/compressSize; // first estimate without template
#endif
  if (tWidth==0 && tHeight==0 && useCompression && (sourceImage.width()>1000 || sourceImage.height()>1000))
    {
      // if this is going DRM, and its not already a small image
      // and the size slider is set for smaller sizes
      // we can pre-scale the image to smaller dimensions to
      // improve the compression speed
      // Changing the ratio makes the slider work consistently
      // About every 50 is where the compression ratio stops
      // making much difference to the size. It's also the point where
      // you are losing significant detail due to compression anyway.
      addToLog(QString("CompressionRatio=%1").arg(compRatio),LOGIMAG);
      if (compRatio > 150) {

          tWidth =sourceImage.width() / 4;
          tHeight=sourceImage.height() / 4;
          //          compRatio = ((compRatio - 151) * 3) + 45;
        }
      else if (compRatio > 100) {
          tWidth =sourceImage.width() / 3;
          tHeight=sourceImage.height() / 3;
          //          compRatio -= 73;
        }
      else if (compRatio > 50) {
          tWidth =sourceImage.width() / 2;
          tHeight=sourceImage.height() / 2;
          //          compRatio -= 38;
        }
    }

  if((fi.fileName().isEmpty())  || (!useTemplate))
    {
      addToLog(QString("No Template, targetW,H=%1,%2").arg(targetWidth).arg(targetHeight), LOGIMAG);
      if(tWidth!=0 && tHeight!=0)
        {
          QImage scaledImage = QImage(sourceImage
                                      .scaled(tWidth,
                                              tHeight,
                                              aspectRatioMode,
                                              Qt::SmoothTransformation
                                              )
                                      );
          // Crop to intended dimensions at the centre of the image
          displayedImage = QImage(scaledImage
                                  .copy((scaledImage.width()-tWidth)/2,
                                        (scaledImage.height()-tHeight)/2,
                                        tWidth,
                                        tHeight
                                        )
                                  );


        }
      else
        {
          displayedImage=sourceImage;
        }
      resultImage=&displayedImage;
    }
  else
    {
      addToLog("apply template",LOGIMAG);
      //  sconvert cnv;

      if((!fi.fileName().isEmpty())  && (useTemplate))
        {
          tscene.load(fi);
          tscene.addConversion('c',toCall,true);
          tscene.addConversion('r',rsv);
          tscene.addConversion('o',toOperator);
          tscene.addConversion('t',QDateTime::currentDateTime().toUTC().toString("hh:mm"));
          tscene.addConversion('d',QDateTime::currentDateTime().toUTC().toString("yyyy/MM/dd"));
          tscene.addConversion('m',myCallsign);
          tscene.addConversion('q',myQth);
          tscene.addConversion('l',myLocator);
          tscene.addConversion('n',myLastname);
          tscene.addConversion('f',myFirstname);
          tscene.addConversion('v',qsstvVersion);
          tscene.addConversion('x',comment1);
          tscene.addConversion('y',comment2);
          tscene.addConversion('z',comment3);
          tscene.addConversion('s',QString::number(lastAvgMER,'g',2));

          addToLog(QString("Template size=%1,%2, SourceW,H=%3,%4 TargetW,H=%5,%6").
                   arg(tscene.width()).arg(tscene.height()).
                   arg(sourceImage.width()).arg(sourceImage.height()).
                   arg(tWidth).arg(tHeight),
                   LOGIMAG);
          if(tWidth!=0 && tHeight!=0)
            {
              QImage scaledImage = QImage(sourceImage
                                          .scaled(tWidth,
                                                  tHeight,
                                                  aspectRatioMode,
                                                  Qt::SmoothTransformation
                                                  )
                                          );
              scaledImage=scaledImage.convertToFormat(QImage::Format_ARGB32);
              overlayedImage= QImage(scaledImage
                                     .copy((scaledImage.width()-tWidth)/2,
                                           (scaledImage.height()-tHeight)/2,
                                           tWidth,
                                           tHeight
                                           )
                                     );
              tscene.overlay(&overlayedImage);
            }
          else
            {
              tscene.overlay(&sourceImage);
            }
          resultImage=tscene.getImagePtr();
          addToLog(QString("resultImageW,H=%1,%2").arg(resultImage->width()).arg(resultImage->height()), LOGIMAG);
        }
    }
  if(useCompression)
    {
      bool useOriginal=false;
      int fileSize;
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
      byteCount=(resultImage->convertToFormat(QImage::Format_RGB32).byteCount()*3)/4;
#else
      byteCount=(resultImage->convertToFormat(QImage::Format_RGB32).sizeInBytes()*3)/4;
#endif
      compRatio=byteCount/compressSize;



      // jp2io only uses the RGB component of the RGB32 data
      // so the compression ratio is calculated for 3/4 of the image size
      // so its better to calculate the end result for 3/4 of the source imag

      compressedImageData=jp2.encode(resultImage->convertToFormat(QImage::Format_RGB32),compressedImage,fileSize,compRatio);
      //#ifdef IMAGETESTVIEWER
      //      imageTestViewer(&compressedImage,"compressedImage");
      //#endif

      //      qDebug() << "byteCount image" << byteCount
      //               << "compRatio"  << compRatio << "filesize" << fileSize  << "ratio"   << (float)byteCount/(float)fileSize;

      if (!useTemplate && !imageFileName.isEmpty())
        {
          QFile original(imageFileName);
          if (original.open(QIODevice::ReadOnly) && (original.size() < fileSize))
            {
              //              qDebug() << "original size" << original.size();
              useOriginal=true;
              compressedFilename=imageFileName;
              addToLog(QString("Using original image data (%1 bytes)").arg(original.size()),LOGIMAG);
              statusBarPtr->showMessage("Using original Image (smaller)");
              compressedImageData = original.readAll();
              displayedImage.load(imageFileName);
            }
        }
      if (!useOriginal)
        {
          displayedImage=compressedImage;
          int pos;
          pos=imageFileName.lastIndexOf(".",-1);
          compressedFilename=imageFileName.left(pos)+".jp2";
          statusBarPtr->showMessage("");
          addToLog(QString("Image Compressed to %1 bytes").arg(compressedImageData.size()),LOGIMAG);
          addToLog(QString("displayedImageW,H=%1,%2  compRatio=%3").
                   arg(displayedImage.width()).arg(displayedImage.height()).
                   arg(compRatio),
                   LOGIMAG);
        }
      addToLog(QString("compressed size %1").arg(compressedImageData.size()),LOGIMAG);
      return compressedImageData.size();
    }
  else
    {
      displayedImage=resultImage->convertToFormat(QImage::Format_ARGB32);
      compressedImageData.clear();
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
      return displayedImage.byteCount();
#else
      return displayedImage.sizeInBytes();
#endif
    }
}

int imageViewer::diplayedImageBytecount()
{
#if (QT_VERSION < QT_VERSION_CHECK(5,10,0))
  return (displayedImage.byteCount()*3)/4;
#else
  return (displayedImage.sizeInBytes()*3)/4;
#endif
}


// ---- stitched grid (TX image on modes larger than PD160) ----

void imageViewer::setGrid(int cols,int rows)
{
  if(ttype!=TXIMG) return;
  if(cols<1) cols=1;
  if(rows<1) rows=1;
  if((cols==gridCols) && (rows==gridRows)) return;
  bool wasGrid=gridActive();
  int n=cols*rows;
  // segments keep their slot (0..3) whatever the layout; slots beyond the layout stay remembered
  segImages.resize(MAXSEGMENTS);
  segFiles.resize(MAXSEGMENTS);
  if(wasGrid)
    {
      if(n==1 && !segImages[0].isNull())
        {
          // back to a single image: segment 0 becomes the image again
          sourceImage=segImages[0];
          imageFileName=segFiles[0];
        }
    }
  else if(!sourceImage.isNull())
    {
      // the single image becomes segment 0
      segImages[0]=sourceImage;
      segFiles[0]=imageFileName;
    }
  gridCols=cols;
  gridRows=rows;
  selectSegment(0);
  applyTemplate();
  displayImage();
  emit imageChanged();
}

int imageViewer::segmentAt(const QPoint &pos)
{
  if(displayedImage.isNull()) return activeSeg;
  QPoint p=mapToImage(pos);
  int c=qBound(0,p.x()*gridCols/qMax(1,displayedImage.width()),gridCols-1);
  int r=qBound(0,p.y()*gridRows/qMax(1,displayedImage.height()),gridRows-1);
  return r*gridCols+c;
}

void imageViewer::selectSegment(int seg)
{
  if(!gridActive()) return;
  activeSeg=qBound(0,seg,gridCols*gridRows-1);
  imageFileName=segFiles.value(activeSeg);
  const QImage seg_im=segImages.value(activeSeg);
  orgWidth=seg_im.width();
  orgHeight=seg_im.height();
}

void imageViewer::captureSegment(const QImage &im,const QString &fn)
{
  if(activeSeg>=segImages.size()) return;
  segImages[activeSeg]=im;
  segFiles[activeSeg]=fn;
}

QImage imageViewer::composeGrid()
{
  int w=targetWidth,h=targetHeight;
  if(w<=0 || h<=0)
    {
      w=gridCols*512;
      h=gridRows*400;
    }
  QImage out(w,h,QImage::Format_ARGB32_Premultiplied);
  out.fill(imageBackGroundColor);
  QPainter painter(&out);
  for(int r=0;r<gridRows;r++)
    {
      for(int c=0;c<gridCols;c++)
        {
          const QImage seg=segImages.value(r*gridCols+c);
          if(seg.isNull()) continue;
          QRect cell(c*w/gridCols,r*h/gridRows,(c+1)*w/gridCols-c*w/gridCols,(r+1)*h/gridRows-r*h/gridRows);
          // same Stretch/Crop/Fit handling as a whole image, applied per cell
          QImage scaled=seg.scaled(cell.size(),aspectRatioMode,Qt::SmoothTransformation);
          painter.setClipRect(cell);
          painter.drawImage(cell.x()+(cell.width()-scaled.width())/2,cell.y()+(cell.height()-scaled.height())/2,scaled);
        }
    }
  painter.end();
  return out;
}

QPixmap imageViewer::withGridOverlay(const QPixmap &pm)
{
  if(!gridActive() || pm.isNull()) return pm;
  QPixmap out=pm;
  QPainter p(&out);
  int w=out.width(),h=out.height();
  QPen pen(Qt::white);
  pen.setStyle(Qt::DashLine);
  p.setPen(pen);
  for(int c=1;c<gridCols;c++) p.drawLine(c*w/gridCols,0,c*w/gridCols,h);
  for(int r=1;r<gridRows;r++) p.drawLine(0,r*h/gridRows,w,r*h/gridRows);
  // outline the selected segment
  int ac=activeSeg%gridCols,ar=activeSeg/gridCols;
  QRect cell(ac*w/gridCols,ar*h/gridRows,(ac+1)*w/gridCols-ac*w/gridCols,(ar+1)*h/gridRows-ar*h/gridRows);
  p.setPen(QPen(Qt::yellow,2));
  p.drawRect(cell.adjusted(1,1,-1,-1));
  p.end();
  return out;
}

void imageViewer::resizeEvent(QResizeEvent *)
{
  displayImage();
}

QImage *imageViewer::getDisplayedImage()
{
  return &displayedImage;
}

//void imageViewer::slotTest()
//{
//  qDebug() << "alpha" << imageBackGroundColor.alpha() << "red" << imageBackGroundColor.red();
////  createImage(QSize(800,600),imageBackGroundColor,true);
//  QFile fi;
//  dirDialog d(this,"Open File");
//  QString s=d.openFileName(txStockImagesPath,"*.png");
//  if (s.isNull()) return ;
//  if (s.isEmpty()) return ;
//  fi.setFileName(s);
//  displayedImage.load(s);
//  setPixmap(QPixmap::fromImage(displayedImage));
//}


#ifdef IMAGETESTVIEWER
void imageViewer::imageTestViewer(QImage *im,QString infoStr)
{
  Q_UNUSED(infoStr);
  if(inStartup) return;
  //  QImage imc=*im;

  //  QPainter painter(im);
  //  painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  //  painter.fillRect(imc.rect(), imageBackGroundColor);
  //  painter.end();
  templateViewer tv;
  //  qDebug() << infoStr << im->width() << im->height() << "format" << im->format();


  tv.setImage(im);
  tv.exec();
}
#endif


QImage imageViewer::clipboardImage()
{
  const QMimeData *md=QGuiApplication::clipboard()->mimeData();
  if(!md) return QImage();
  if(md->hasImage())
    {
      QImage im=qvariant_cast<QImage>(md->imageData());
      if(!im.isNull()) return im;
    }
  if(md->hasUrls())
    {
      const QList<QUrl> urls=md->urls();
      for(const QUrl &u: urls)
        {
          if(!u.isLocalFile()) continue;
          QImage im(u.toLocalFile());
          if(!im.isNull()) return im;
        }
    }
  return QImage();
}

void imageViewer::slotCopy()
{
  if(gridActive())
    {
      // copy only the selected segment's own image
      QImage seg=segImages.value(activeSeg);
      if(!seg.isNull()) QGuiApplication::clipboard()->setImage(seg);
      return;
    }
  // the live RX image is painted straight into displayedImage and never sets validImage/sourceImage
  if(!hasValidImage() && displayedImage.isNull()) return;
  QImage im;
  if((ttype!=RXIMG) && (ttype!=TXIMG) && !imageFileName.isEmpty())
    {
      im.load(imageFileName);   // thumbnails only hold a downscaled copy; use the full file
    }
  if(im.isNull()) im=sourceImage.isNull() ? displayedImage : sourceImage;
  if(im.isNull()) return;
  // the receiver writes bare RGB values into displayedImage, so the alpha channel is not meaningful
  // (save() flattens to RGB32 for the same reason); an alpha-carrying copy pastes as blank/transparent
  if(ttype==RXIMG) im=im.convertToFormat(QImage::Format_RGB32);
  QGuiApplication::clipboard()->setImage(im);
}

void imageViewer::slotPaste()
{
  if((ttype!=TXIMG) && (ttype!=TXSTOCKTHUMB)) return;
  QImage im=clipboardImage();
  if(im.isNull()) return;
  // the stock gallery is a directory listing, so pasted images are stored there (as snapshots are)
  QDir dir(txStockImagesPath);
  QString base=QString("clipboard-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
  QString dst=dir.filePath(base+".png");
  for(int i=1;QFile::exists(dst);i++)
    {
      dst=dir.filePath(QString("%1-%2.png").arg(base).arg(i));
    }
  if(!im.save(dst,"PNG"))
    {
      addToLog(QString("Unable to save clipboard image to %1").arg(dst),LOGIMAG);
      return;
    }
  if(ttype==TXIMG) txWidgetPtr->setImage(dst);
  galleryWidgetPtr->txStockImageChanged();
}

void imageViewer::keyPressEvent(QKeyEvent *e)
{
  // shortcuts mirror the popup: only act if the action is offered for this type
  if(e->matches(QKeySequence::Copy) && popup->actions().contains(copyAct))
    {
      slotCopy();
      return;
    }
  if(e->matches(QKeySequence::Delete) && popup->actions().contains(deleteAct))
    {
      if(ttype!=TXIMG || (gridActive() ? !segImages.value(activeSeg).isNull() : hasValidImage())) slotDelete();
      return;
    }
  if(e->matches(QKeySequence::Paste) && popup->actions().contains(pasteAct))
    {
      slotPaste();
      return;
    }
  QLabel::keyPressEvent(e);
}

void imageViewer::setSelected(bool sel)
{
  if(selected==sel) return;
  selected=sel;
  updateFrame();
  update();
}

// selected gallery thumbnail: accent tint, two-tone ring and a check badge, so it
// reads clearly even on an accent-coloured cell background
void imageViewer::paintEvent(QPaintEvent *e)
{
  QLabel::paintEvent(e);
  if(!selected || !isGalleryThumb()) return;
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QColor accent=palette().color(QPalette::Highlight);
  QColor tint=accent;
  tint.setAlpha(45);
  p.fillRect(rect(),tint);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(accent,3));
  p.drawRoundedRect(QRectF(rect()).adjusted(1.5,1.5,-1.5,-1.5),3,3);
  p.setPen(QPen(Qt::white,1));
  p.drawRoundedRect(QRectF(rect()).adjusted(3.5,3.5,-3.5,-3.5),2,2);
  int d=qBound(12,qMin(width(),height())/5,22);
  QRectF badge(7,7,d,d);
  p.setPen(QPen(Qt::white,2));
  p.setBrush(accent);
  p.drawEllipse(badge);
  p.setPen(QPen(Qt::white,qMax(1.5,d/8.0),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
  QPainterPath tick;
  tick.moveTo(badge.left()+d*0.27,badge.top()+d*0.53);
  tick.lineTo(badge.left()+d*0.44,badge.top()+d*0.69);
  tick.lineTo(badge.left()+d*0.74,badge.top()+d*0.34);
  p.setBrush(Qt::NoBrush);
  p.drawPath(tick);
}

// thin highlighted box while focused (a selected gallery thumbnail is drawn in paintEvent)
void imageViewer::updateFrame()
{
  QPalette p=palette();
  if(hasFocus() && !(selected && isGalleryThumb()))
    {
      p.setColor(QPalette::WindowText,p.color(QPalette::Highlight));
      setPalette(p);
      setFrameStyle(QFrame::Box | QFrame::Plain);
      setLineWidth(2);
    }
  else
    {
      p.setColor(QPalette::WindowText,QApplication::palette().color(QPalette::WindowText));
      setPalette(p);
      setFrameStyle(QFrame::Sunken | QFrame::Panel);
      setLineWidth(1);
    }
}

int imageViewer::firstEmptySegment() const
{
  if(!gridActive()) return -1;
  for(int i=0;i<gridCols*gridRows;i++) if(segImages.value(i).isNull()) return i;
  return 0;
}

bool imageViewer::loadSegment(const QString &file,int seg)
{
  QString fn=file;
  if(!gridActive()) return openImage(fn,true,true,false,true);
  selectSegment(seg);
  bool ok=openImage(fn,true,false,false,false);   // synchronous so the segment index stays valid
  displayImage();
  emit imageChanged();
  return ok;
}

// fills grid segments 0.. with the first files (extra files are ignored)
int imageViewer::loadSegments(const QStringList &files)
{
  if(ttype!=TXIMG) return 0;
  if(!gridActive())
    {
      if(files.isEmpty()) return 0;
      QString fn=files.first();
      return openImage(fn,true,true,false,true) ? 1 : 0;
    }
  int n=0;
  int count=qMin(files.count(),gridCols*gridRows);
  for(int i=0;i<count;i++)
    {
      selectSegment(i);
      QString fn=files.at(i);
      if(openImage(fn,true,false,false,false)) n++;   // synchronous so the segment index stays valid
    }
  selectSegment(0);
  displayImage();
  emit imageChanged();
  return n;
}

void imageViewer::focusInEvent(QFocusEvent *e)
{
  QLabel::focusInEvent(e);
  updateFrame();
}

void imageViewer::focusOutEvent(QFocusEvent *e)
{
  QLabel::focusOutEvent(e);
  updateFrame();
}
