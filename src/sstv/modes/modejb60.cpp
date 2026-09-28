/***************************************************************************
 *   JB60 mode: half-time (about 61 s) PD120-class SSTV mode              *
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
#include "modejb60.h"
#include <algorithm>

namespace
{
  const unsigned int kWidth=640;

  // sub line (segment) layout of one line pair, in sample slots
  enum {SEG_Y0,SEG_Y1,SEG_CR,SEG_CB,NUMSEGS};
  const unsigned int kSegCount[NUMSEGS]={320,320,320,224};
  const unsigned int kSegOffset[NUMSEGS]={0,320,640,960};
  const unsigned int kTotalSlots=1184;

  const unsigned int kLumaSamples=kWidth/2;

  // transmit prefilter: 5 tap diamond, removes the diagonal detail the quincunx lattice can't carry
  const float kPrefCentre=0.5f;
  const float kPrefSide=0.125f;

  // receive: edge directed luma interpolation and luma guided chroma upsampling
  const float kGuideSigma=20.0f;   // luminance difference (0..255) that halves the trust in a chroma sample
  const float kGuideFloor=0.02f;   // keeps plain linear interpolation as the fallback

  /*!
    Columns covered by chroma sample j of n samples across the picture width
  */
  void chromaFootprint(unsigned int n,unsigned int j,unsigned int &c0,unsigned int &c1)
  {
    double pw=(double)kWidth/(double)n;
    c0=(unsigned int)floor(j*pw+1e-9);
    int e=(int)ceil((j+1)*pw-1e-9)-1;
    if(e>(int)kWidth-1) e=(int)kWidth-1;
    c1=(unsigned int)e;
    if(c1<c0) c1=c0;
  }

  inline unsigned char clampByte(float v)
  {
    int i=(int)lroundf(v);
    return (unsigned char)(i<0 ? 0 : (i>255 ? 255 : i));
  }
}

modeJB60::modeJB60(esstvMode m,unsigned int len,bool tx,bool narrowMode): modeBase(m,len,tx,narrowMode)
{
  slot=0;
  prevSample=0;
  for(int d=0;d<256;d++)
    {
      float x=d/kGuideSigma;
      guideLut[d]=expf(-x*x);
    }
}

modeJB60::~modeJB60()
{
}

void modeJB60::setupParams(double clock)
{
  slot=(getLineLength(mode,clock)-fp-bp-syncDuration)/kTotalSlots;
  visibleLineLength=slot*kTotalSlots;
  // (re)initialise the receive state: init() is called again after a slant correction and the
  // picture is replayed from the start
  rowY.assign(kWidth,128);
  curY0.assign(kSegCount[SEG_Y0],128);
  curY1.assign(kSegCount[SEG_Y1],128);
  curCr.assign(kSegCount[SEG_CR],128);
  curCb.assign(kSegCount[SEG_CB],128);
  prevY0=curY0;
  prevY1=curY1;
  prevCr=curCr;
  prevCb=curCb;
}

void modeJB60::calcPixelPositionTable(unsigned int segment,bool tx)
{
  unsigned int i;
  int ofx=tx ? 1 : 0;
  DSPFLOAT lineStart=start+bp+kSegOffset[segment]*slot;
  for(i=0;i<kSegCount[segment];i++)
    {
      pixelPositionTable[i]=(unsigned int)round(lineStart+(i+ofx)*slot);
    }
  segmentPixels=kSegCount[segment];
}

modeBase::embState modeJB60::rxSetupLine()
{
  start=lineTimeTableRX[lineCounter];
  switch(subLine)
    {
    case 0:
      debugState=stBP;
      markerFloat=start+bp;
      marker=(unsigned int)round(markerFloat);
      return MBRXWAIT;
    case 1:
      debugState=stColorLine0;
      calcPixelPositionTable(SEG_Y0,false);
      markerFloat+=kSegCount[SEG_Y0]*slot;
      pixelArrayPtr=yArrayPtr;
      return MBPIXELS;
    case 2:
      debugState=stColorLine1;
      calcPixelPositionTable(SEG_Y1,false);
      markerFloat+=kSegCount[SEG_Y1]*slot;
      pixelArrayPtr=greenArrayPtr;
      return MBPIXELS;
    case 3:
      debugState=stColorLine2;
      calcPixelPositionTable(SEG_CR,false);
      markerFloat+=kSegCount[SEG_CR]*slot;
      pixelArrayPtr=redArrayPtr;
      return MBPIXELS;
    case 4:
      debugState=stColorLine3;
      calcPixelPositionTable(SEG_CB,false);
      markerFloat+=kSegCount[SEG_CB]*slot;
      pixelArrayPtr=blueArrayPtr;
      return MBPIXELS;
    case 5:
      debugState=stFP;
      markerFloat+=fp;
      marker=(unsigned int)round(markerFloat);
      return MBRXWAIT;
    case 6:
      debugState=stSync;
      markerFloat+=syncDuration;
      marker=(unsigned int)round(markerFloat);
      syncPosition=marker;
      return MBSYNC;
    default:
      return MBENDOFLINE;
    }
}

modeBase::embState modeJB60::txSetupLine()
{
  start=lineTimeTableTX[lineCounter];
  switch(subLine)
    {
    case 0:
      calcPixelPositionTable(SEG_Y0,true);
      pixelArrayPtr=yArrayPtr;
      return MBPIXELS;
    case 1:
      calcPixelPositionTable(SEG_Y1,true);
      pixelArrayPtr=greenArrayPtr;
      return MBPIXELS;
    case 2:
      calcPixelPositionTable(SEG_CR,true);
      pixelArrayPtr=redArrayPtr;
      return MBPIXELS;
    case 3:
      calcPixelPositionTable(SEG_CB,true);
      pixelArrayPtr=blueArrayPtr;
      return MBPIXELS;
    case 4:
      txFreq=lowerFreq;
      txDur=(unsigned int)rint(fp);
      return MBTXGAP;
    case 5:
      txFreq=syncFreq;
      txDur=(unsigned int)rint(syncDuration);
      return MBTXGAP;
    case 6:
      txFreq=lowerFreq;
      txDur=(unsigned int)rint(bp);
      return MBTXGAP;
    default:
      return MBENDOFLINE;
    }
}

/*!
  Receive one slot. The base class picks a single demodulator sample; here the picked sample and
  the one before it are averaged. That centres the measurement in the slot and reduces noise.
  The slot length is not the picture width divided into the line, so the base implementation
  (which uses pixelDuration) can't be used.
*/
bool modeJB60::getPixels()
{
  int color;
  double dev=activeSSTVParam->deviation*2;
  double fc=activeSSTVParam->subcarrier;
  if(sampleCounter>=pixelPositionTable[pixelCounter]+(slot/2))
    {
      double avg=((double)sample+(double)prevSample)/2.;
      color=128+lround((avg-fc)*255./dev);
      if(color<0) color=0;
      if(color>255) color=255;
      pixelArrayPtr[pixelCounter]=(unsigned char)color;
      pixelCounter++;
      prevSample=sample;
      if(pixelCounter>=segmentPixels) return true;
      return false;
    }
  prevSample=sample;
  return false;
}

/*!
  Transmit side: takes pair p=lineCounter (image rows 2p and 2p+1).
*/
void modeJB60::getLine()
{
  const unsigned int p=lineCounter;
  const int lastRow=(int)activeSSTVParam->numberOfDisplayLines-1;
  unsigned int *line[4];
  float lum[4][kWidth];
  unsigned int c,k;
  int i;

  // rows 2p-1 .. 2p+2, rows outside the picture repeat the edge row
  for(i=0;i<4;i++)
    {
      int r=(int)(2*p)-1+i;
      if(r<0) r=0;
      if(r>lastRow) r=lastRow;
      line[i]=txImage()->getScanLineAddress(r);
      for(c=0;c<kWidth;c++)
        {
          unsigned int t=line[i][c];
          lum[i][c]=(59*qGreen(t)+30*qRed(t)+11*qBlue(t))/100.f;
        }
    }

  // luminance: quincunx samples of the prefiltered picture (even columns on row 2p, odd on row 2p+1)
  for(i=0;i<2;i++)
    {
      unsigned char *out=(i==0) ? yArrayPtr : greenArrayPtr;
      const float *above=lum[i];
      const float *cur=lum[i+1];
      const float *below=lum[i+2];
      for(k=0;k<kLumaSamples;k++)
        {
          unsigned int col=2*k+i;
          unsigned int cw=(col>0) ? col-1 : 0;
          unsigned int ce=(col<kWidth-1) ? col+1 : kWidth-1;
          out[k]=clampByte(kPrefCentre*cur[col]+kPrefSide*(above[col]+below[col]+cur[cw]+cur[ce]));
        }
    }

  // chrominance: same definitions as PD (Cr=(R-Y)/1.4+127.5, Cb=(B-Y)/1.78+127.5) on the row pair,
  // box averaged over the footprint of each sample
  float crPix[kWidth];
  float cbPix[kWidth];
  for(c=0;c<kWidth;c++)
    {
      unsigned int te=line[1][c];
      unsigned int to=line[2][c];
      float ybar=(lum[1][c]+lum[2][c])/2.f;
      float rbar=(qRed(te)+qRed(to))/2.f;
      float bbar=(qBlue(te)+qBlue(to))/2.f;
      crPix[c]=(rbar-ybar)/1.4f+127.5f;
      cbPix[c]=(bbar-ybar)/1.78f+127.5f;
    }
  for(k=0;k<kSegCount[SEG_CR];k++)
    {
      unsigned int c0,c1;
      float sum=0;
      chromaFootprint(kSegCount[SEG_CR],k,c0,c1);
      for(c=c0;c<=c1;c++) sum+=crPix[c];
      redArrayPtr[k]=clampByte(sum/(c1-c0+1));
    }
  for(k=0;k<kSegCount[SEG_CB];k++)
    {
      unsigned int c0,c1;
      float sum=0;
      chromaFootprint(kSegCount[SEG_CB],k,c0,c1);
      for(c=c0;c<=c1;c++) sum+=cbPix[c];
      blueArrayPtr[k]=clampByte(sum/(c1-c0+1));
    }
}

/*!
  Rebuild a full luminance row from its own quincunx samples (columns 2k+parity) and the samples of the
  rows above and below (opposite parity). A missing pixel has sampled neighbours W,E (same row) and N,S
  (adjacent rows); the two directions are blended by how similar their pair is, so edges are followed
  instead of smeared. A missing row (image edge) mirrors the other one.
*/
void modeJB60::reconstructLuma(unsigned char *out,const std::vector<unsigned char> &own,unsigned int parity,
                               const std::vector<unsigned char> *up,const std::vector<unsigned char> *down)
{
  if(!up) up=down;
  if(!down) down=up;
  for(unsigned int c=0;c<kWidth;c++)
    {
      if((c&1)==parity)
        {
          out[c]=own[c>>1];
          continue;
        }
      int wi,ei;
      if(parity==0)
        {
          wi=(int)(c>>1);
          ei=wi+1;
        }
      else
        {
          ei=(int)(c>>1);
          wi=ei-1;
        }
      if(wi<0) wi=ei;
      if(ei>=(int)kLumaSamples) ei=wi;
      float w=own[wi];
      float e=own[ei];
      float n=(*up)[c>>1];
      float s=(*down)[c>>1];
      float dh=fabsf(w-e)+1.f;
      float dv=fabsf(n-s)+1.f;
      float wh=1.f/(dh*dh);
      float wv=1.f/(dv*dv);
      out[c]=clampByte((wh*(w+e)*0.5f+wv*(n+s)*0.5f)/(wh+wv));
    }
}

/*!
  Upsample one chroma component (c.size() samples) to the full row. Between the two nearest chroma
  samples the linear weight is multiplied by how well the pixel's luminance matches the mean
  luminance under each sample, so chroma edges snap to luminance edges.
*/
void modeJB60::upsampleChroma(const unsigned char *y,const std::vector<unsigned char> &c,unsigned char *out)
{
  const unsigned int n=(unsigned int)c.size();
  float yb[kWidth];
  const double pw=(double)kWidth/(double)n;
  unsigned int j,x;
  for(j=0;j<n;j++)
    {
      unsigned int c0,c1;
      unsigned int sum=0;
      chromaFootprint(n,j,c0,c1);
      for(x=c0;x<=c1;x++) sum+=y[x];
      yb[j]=(float)sum/(float)(c1-c0+1);
    }
  for(x=0;x<kWidth;x++)
    {
      double jf=(x+0.5)/pw-0.5;
      int j0=(int)floor(jf);
      float t=(float)(jf-j0);
      int j1=j0+1;
      if(j0<0) j0=0;
      if(j0>(int)n-1) j0=(int)n-1;
      if(j1<0) j1=0;
      if(j1>(int)n-1) j1=(int)n-1;
      int d0=(int)lroundf(fabsf(y[x]-yb[j0]));
      int d1=(int)lroundf(fabsf(y[x]-yb[j1]));
      if(d0>255) d0=255;
      if(d1>255) d1=255;
      float w0=(1.f-t)*(kGuideFloor+guideLut[d0]);
      float w1=t*(kGuideFloor+guideLut[d1]);
      out[x]=clampByte((w0*c[j0]+w1*c[j1])/(w0+w1));
    }
}

void modeJB60::emitRow(const std::vector<unsigned char> &own,unsigned int parity,
                       const std::vector<unsigned char> *up,const std::vector<unsigned char> *down,
                       const std::vector<unsigned char> &cr,const std::vector<unsigned char> &cb)
{
  reconstructLuma(rowY.data(),own,parity,up,down);
  upsampleChroma(rowY.data(),cr,redArrayPtr);
  upsampleChroma(rowY.data(),cb,blueArrayPtr);
  yuvConversion(rowY.data());   // writes the row at displayLineCounter and advances it
}

/*!
  Receive side, called once per line pair. A row can only be finished once the rows above and below it
  are known, so the picture trails the received data by one row: pair p finishes rows 2p-1 and 2p (and
  row 0 / the last row at the picture edges).
*/
void modeJB60::showLine()
{
  const unsigned int p=lineCounter;
  const bool last=(p+1>=activeSSTVParam->numberOfDataLines);

  curY0.assign(yArrayPtr,yArrayPtr+kSegCount[SEG_Y0]);
  curY1.assign(greenArrayPtr,greenArrayPtr+kSegCount[SEG_Y1]);
  curCr.assign(redArrayPtr,redArrayPtr+kSegCount[SEG_CR]);
  curCb.assign(blueArrayPtr,blueArrayPtr+kSegCount[SEG_CB]);

  if(p==0)
    {
      emitRow(curY0,0,NULL,&curY1,curCr,curCb);
    }
  else
    {
      emitRow(prevY1,1,&prevY0,&curY0,prevCr,prevCb);
      emitRow(curY0,0,&prevY1,&curY1,curCr,curCb);
    }
  if(last)
    {
      emitRow(curY1,1,&curY0,NULL,curCr,curCb);
    }

  prevY0.swap(curY0);
  prevY1.swap(curY1);
  prevCr.swap(curCr);
  prevCb.swap(curCb);
}
