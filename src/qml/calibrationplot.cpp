#include "calibrationplot.h"
#include "calibrationcontroller.h"
#include <QPainter>
#include <QObject>
#include <cmath>
#include <algorithm>

#define WATERFALLROWS 400       // most recent ticks shown in the waterfall

CalibrationPlot::CalibrationPlot(QQuickItem *parent) : QQuickPaintedItem(parent)
{
  setAntialiasing(false);
}

void CalibrationPlot::setKind(int k)
{
  if(k==m_kind) return;
  m_kind=k;
  emit kindChanged();
  update();
}

void CalibrationPlot::setRevision(int r)
{
  if(r==m_revision) return;
  m_revision=r;
  emit revisionChanged();
  update();
}

// one trace of the internet time plot: dots as offsets in ms, the fit as a red line, a flat green line for a perfect clock
static void drawTrace(QPainter &p,const QRect &r,const calTrace &t,const QString &name)
{
  p.setPen(QColor(40,40,90));
  p.drawRect(r.adjusted(0,0,-1,-1));
  if(t.x.size()<2)
    {
      p.setPen(QColor(150,150,180));
      p.drawText(r,Qt::AlignCenter,QObject::tr("%1: waiting for samples").arg(name));
      return;
    }
  const int left=4,right=4,top=4,bottom=14;
  const double x0=t.x.front(),x1=t.x.back();
  const double span=std::max(x1-x0,1.0);
  // centre on the mean of the inliers
  double mean=0;
  int n=0;
  for(size_t i=0;i<t.x.size();i++) if(t.in[i]) {mean+=t.y[i];n++;}
  mean=(n>0) ? mean/n : 0;
  double ymax=1.0;                      // at least +/- 1 ms
  for(size_t i=0;i<t.x.size();i++) if(t.in[i]) ymax=std::max(ymax,1.3*std::fabs(t.y[i]-mean));
  const double pw=r.width()-left-right;
  const double ph=r.height()-top-bottom;
  auto xOf=[&](double x){return r.left()+left+pw*(x-x0)/span;};
  auto yOf=[&](double y){return r.top()+top+ph*(0.5-0.5*(y-mean)/ymax);};
  p.setPen(QPen(QColor(0,200,0),1));
  p.drawLine(QPointF(xOf(x0),yOf(mean)),QPointF(xOf(x1),yOf(mean)));
  for(size_t i=0;i<t.x.size();i++)
    {
      const QColor c=t.in[i] ? QColor(255,255,255) : QColor(150,60,60);
      p.fillRect(QRectF(xOf(t.x[i])-1.5,yOf(t.y[i])-1.5,3.0,3.0),c);
    }
  if(t.valid)
    {
      double xc=0;
      int m=0;
      for(size_t i=0;i<t.x.size();i++) if(t.in[i]) {xc+=t.x[i];m++;}
      xc=(m>0) ? xc/m : x0;
      p.setPen(QPen(QColor(255,60,60),1));
      p.drawLine(QPointF(xOf(x0),yOf(mean+t.slope*(x0-xc))),QPointF(xOf(x1),yOf(mean+t.slope*(x1-xc))));
    }
  p.setPen(QColor(150,150,180));
  p.drawText(QRect(r.left(),r.bottom()-bottom,r.width(),bottom),Qt::AlignCenter,
             QObject::tr("%1   +/- %2 ms   |   %3 s").arg(name).arg(ymax,0,'f',1).arg(span,0,'f',0));
}

static void paintNtp(QPainter &p,const QRect &all,const CalibrationController *c)
{
  p.fillRect(all,QColor(10,10,40));
  const int h=all.height()/2;
  drawTrace(p,QRect(all.left(),all.top(),all.width(),h),c->ntpTrace(),QObject::tr("time servers"));
  drawTrace(p,QRect(all.left(),all.top()+h,all.width(),all.height()-h),c->audioTrace(),QObject::tr("sound device"));
}

// the tick waterfall as in MMSSTV: one row per second, the tick drawn at its offset from where a perfect clock would put it
static void paintWwv(QPainter &p,const QRect &all,const CalibrationController *c)
{
  p.fillRect(all,QColor(10,10,40));
  const wwvTickFit *fit=c->wwvFit();
  if(fit==nullptr || fit->points().empty())
    {
      p.setPen(QColor(150,150,180));
      p.drawText(all,Qt::AlignCenter,QObject::tr("waiting for time ticks"));
      return;
    }
  const std::vector<wwvFitPoint> &pts=fit->points();
  const double nominal=fit->nominalRate();
  const size_t first=(pts.size()>WATERFALLROWS) ? pts.size()-WATERFALLROWS : 0;
  // reference: the first tick on the fitted line (or the first tick shown); offsets are relative to a perfect clock
  double ref=pts[first].position;
  for(size_t i=first;i<pts.size();i++) if(pts[i].inlier) {ref=pts[i].position;break;}
  const double t0=pts[first].position;
  const double t1=pts.back().position;
  const double tspan=std::max(t1-t0,10.0*nominal);
  std::vector<double> dx(pts.size());
  double maxDx=0.02*nominal;                  // at least +/- 20 ms
  for(size_t i=first;i<pts.size();i++)
    {
      const double d=pts[i].position-ref;
      dx[i]=d-std::round(d/nominal)*nominal;
      if(pts[i].inlier) maxDx=std::max(maxDx,1.3*std::fabs(dx[i]));
    }
  maxDx=std::min(maxDx,0.5*nominal);
  const int w=all.width();
  const int h=all.height();
  const int left=all.left()+4,right=4,top=all.top()+4,bottom=16;
  const double pw=w-4-right;
  const double ph=h-4-bottom;
  auto xOf=[&](double d){return left+pw*(0.5+0.5*d/maxDx);};
  auto yOf=[&](double pos){return top+ph*(pos-t0)/tspan;};
  // reference line (a perfect clock) and 10 ms grid
  p.setPen(QColor(40,40,90));
  for(double ms=-1000.0*maxDx/nominal;ms<=1000.0*maxDx/nominal;ms+=10.0)
    {
      const double m=std::round(ms/10.0)*10.0;
      if(std::fabs(m)>1000.0*maxDx/nominal) continue;
      p.drawLine(QPointF(xOf(m*1e-3*nominal),top),QPointF(xOf(m*1e-3*nominal),top+ph));
    }
  p.setPen(QPen(QColor(0,200,0),1));
  p.drawLine(QPointF(xOf(0),top),QPointF(xOf(0),top+ph));
  for(size_t i=first;i<pts.size();i++)
    {
      const QColor col=pts[i].inlier ? QColor(255,255,255) : QColor(150,60,60);
      p.fillRect(QRectF(xOf(dx[i])-1.5,yOf(pts[i].position)-1.0,3.0,2.0),col);
    }
  if(fit->result().valid)
    {
      // the fitted line, drawn as the offset of fitted tick k from the nominal grid
      const double a=fit->intercept();
      const double b=fit->slope();
      auto fitDx=[&](double pos){const double d=(a+b*std::round((pos-a)/b))-ref;return d-std::round(d/nominal)*nominal;};
      // follow the line through the first and last positions shown (no wrap: both are near the centre)
      const double d0=fitDx(t0),d1=fitDx(t1);
      if(std::fabs(d0)<=maxDx && std::fabs(d1)<=maxDx)
        {
          p.setPen(QPen(QColor(255,60,60),1));
          p.drawLine(QPointF(xOf(d0),yOf(t0)),QPointF(xOf(d1),yOf(t1)));
        }
    }
  p.setPen(QColor(150,150,180));
  p.drawText(QRect(all.left(),all.top()+h-bottom,w,bottom),Qt::AlignCenter,
             QObject::tr("+/- %1 ms   |   %2 s").arg(1000.0*maxDx/nominal,0,'f',0).arg(tspan/nominal,0,'f',0));
}

// the received picture with the fitted line in red, so that it can be seen whether the reception was clean
static void paintSstv(QPainter &p,const QRect &all,const CalibrationController *c)
{
  p.fillRect(all,QColor(10,10,40));
  const QImage &image=c->sstvImage();
  if(image.isNull())
    {
      p.setPen(QColor(150,150,180));
      p.drawText(all,Qt::AlignCenter,QObject::tr("no picture received yet"));
      return;
    }
  const QSize sz=image.size().scaled(all.size(),Qt::KeepAspectRatio);
  const QRect r(QPoint(all.left()+(all.width()-sz.width())/2,all.top()+(all.height()-sz.height())/2),sz);
  p.drawImage(r,image);
  const slantFitResult &fit=c->sstvFit();
  if(fit.valid)
    {
      p.setPen(QPen(QColor(255,60,60),1));
      const double sx=double(r.width())/image.width(),sy=double(r.height())/image.height();
      const double y0=0,y1=image.height()-1;
      p.drawLine(QPointF(r.left()+(fit.intercept+0.5)*sx,r.top()+(y0+0.5)*sy),
                 QPointF(r.left()+(fit.intercept+fit.slope*y1+0.5)*sx,r.top()+(y1+0.5)*sy));
    }
}

void CalibrationPlot::paint(QPainter *painter)
{
  const CalibrationController *c=CalibrationController::instance();
  const QRect all(0,0,int(width()),int(height()));
  if(all.isEmpty()) return;
  painter->setFont(QFont(painter->font().family(),9));
  switch(m_kind)
    {
    case 0: paintNtp(*painter,all,c); break;
    case 1: paintWwv(*painter,all,c); break;
    default: paintSstv(*painter,all,c); break;
    }
}
