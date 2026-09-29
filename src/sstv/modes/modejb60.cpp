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
#include "videofilterselection.h"
#include "chromadeconvolution.h"
#include "chromaedgeboost.h"
#include <algorithm>
#include <vector>

namespace
{
  const unsigned int kWidth=640;

  // sub line (segment) layout of one line pair, in sample slots
  enum {SEG_L,SEG_D,SEG_CR,SEG_CB,NUMSEGS};
  const unsigned int kSegCount[NUMSEGS]={640,144,224,176};
  const unsigned int kSegOffset[NUMSEGS]={0,640,784,1008};
  const unsigned int kTotalSlots=1184;

  // vertical detail channel: level = 128 + sign(d)*kDMax*(|d|/kDMax)^kDGamma, so small differences get more of the swing
  const float kDMax=127.0f;
  const float kDGamma=0.6f;

  // receive: luma guided chroma upsampling
  const float kGuideSigma=20.0f;   // luminance difference (0..255) that halves the trust in a chroma sample
  const float kGuideFloor=0.02f;   // keeps plain linear interpolation as the fallback

  // transmit: luma guided chroma downsampling. Uses the same guideLut (kGuideSigma) but a much higher floor than
  // RX's: RX's floor only has to keep two-tap interpolation from collapsing, but TX's floor sets how far one
  // footprint's output can be pulled from the flat average of its (2-4) pixels, and a straddling footprint's
  // *rounded byte* is what matters -- pulling it far enough to cross to a different rounded value creates a
  // steeper inter-slot transition than flat averaging, which the channel's video filter turns into visible
  // ringing (measured: a real card image and a mixed luma+colour edge test both got a *wider*, not narrower,
  // 10-90% chroma edge). This is a threshold effect tied to byte rounding, not a smooth trade: a sweep over this
  // floor (0.02 through 50, loopback --image medge321/card) showed no middle ground -- below about a 1.1:1
  // weight ratio it reproduces flat averaging almost exactly (no measurable effect either way); above about a
  // 1.3:1 ratio it reliably provokes the same ringing. TODO(jb60-color-smear phase 1): needs a formulation that
  // bounds the inter-slot *step*, not just one footprint's own weights, before this is worth shipping -- see the
  // jb60-color-smear-ideas memory. Left at a safe (near-unity, no measured effect) value for now.
  const float kDownsampleFloor=10.0f;

  /*!
    Columns covered by sample j of n samples across the picture width
  */
  void columnFootprint(unsigned int n,unsigned int j,unsigned int &c0,unsigned int &c1)
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

  // TX idea 6: cap-and-hold regularized-inverse pre-emphasis for the narrow RX video filter,
  // slot-domain (~5.26 kHz), 5 taps, unity DC gain by construction. Gmax=6dB, fc~600Hz (matches
  // the narrow filter's own -6dB point). See jb60-color-smear-ideas memory, TX idea 6, for the
  // derivation and the real-chain sweep that picked this Gmax/length. Unconditional -- part of
  // JB60's spec, not a setting: shipped first as an opt-in checkbox, then hardcoded on once an
  // extended SNR sweep (5 dB down to -10 dB, two images) found only a trivial, image-inconsistent
  // regression (-0.01 to -0.02 dB) confined to SNRs where the picture is already unusable static
  // (SSIM<0.1 on either test image), against a real win (+0.05 to +0.31 dB) everywhere realistic.
  const float kPreEmphH0=1.698f, kPreEmphH1=-0.2133f, kPreEmphH2=-0.1357f;

  void applyChromaPreEmphasis(unsigned char *arr,unsigned int n)
  {
    if(n<1) return;
    std::vector<unsigned char> src(arr,arr+n);   // read from a copy, not partially-overwritten neighbours
    auto at=[&](int i)->float
      {
        if(i<0) i=0;
        if(i>=(int)n) i=(int)n-1;
        return (float)src[i];
      };
    for(unsigned int k=0;k<n;k++)
      {
        float v=kPreEmphH0*at((int)k)
               +kPreEmphH1*(at((int)k-1)+at((int)k+1))
               +kPreEmphH2*(at((int)k-2)+at((int)k+2));
        arr[k]=clampByte(v);   // contains any overshoot -- the idea-5-precedent safety net
      }
  }

  // TX idea 12, attempt 2 (jb60-color-smear-ideas memory, idea 12): attempt 1 (cascading idea 6's own
  // filter with itself at detected edges) was a clean no-win -- measuring the raw channel response fresh
  // (tests/jb60_loopback --dump-slots, pre-emphasis disabled) and re-running idea 6's own cap-and-hold
  // design at higher Gmax shows *why*: step-response overshoot grows almost linearly with Gmax regardless
  // of tap count (6dB~0.42, 8dB~0.62, 10dB~0.87, 12dB~1.19 of the step height) -- a cascade of two 6dB
  // passes (~12dB combined) was never going to avoid that, since the overshoot is intrinsic to demanding
  // that much gain from a cap-and-hold shelf, not an artifact of cascading specifically.
  //
  // Attempt 2 instead fits a fresh, independent, properly-designed kernel at a real (not doubled) target
  // Gmax, applied to the SAME raw (pre-idea-6) box-averaged slot sequence idea 6 itself starts from --
  // not cascaded on top of idea 6's own output -- and blends it against idea 6's baseline output per-slot
  // by the same smoothly-tapered edge-detection weight as attempt 1, so a slot with no real transition
  // nearby still reduces to exactly today's shipped behaviour. Gmax=10dB, 7 taps (L=3), fit by the exact
  // method documented for idea 6/4 (equality-constrained weighted least squares against
  // min(1/|H(f)|,Gmax), DC pinned to 1.0, target held flat past ~1100Hz where the measurement is
  // noise-dominated) -- see tests/jb60_loopback/README.md for a from-scratch reproduction script if this
  // needs re-deriving.
  const float kEdgeH0=2.7535f, kEdgeH1=-0.3808f, kEdgeH2=-0.3047f, kEdgeH3=-0.1913f;
  const float kEdgeBoostThreshold=15.0f;      // slot-to-slot (2-apart) jump, in counts, treated as "a real edge"
  const unsigned int kEdgeBoostSmoothHalf=2;  // triangular smoothing half-width on the edge-weight itself

  void applyChromaEdgeBoost(const unsigned char *raw,unsigned char *arr,unsigned int n)
  {
    if(n<2) return;
    auto at=[&](int i)->float
      {
        if(i<0) i=0;
        if(i>=(int)n) i=(int)n-1;
        return (float)raw[i];
      };
    std::vector<float> boosted(n);
    for(unsigned int k=0;k<n;k++)
      boosted[k]=kEdgeH0*at((int)k)
                +kEdgeH1*(at((int)k-1)+at((int)k+1))
                +kEdgeH2*(at((int)k-2)+at((int)k+2))
                +kEdgeH3*(at((int)k-3)+at((int)k+3));

    std::vector<float> rawW(n,0.f);
    for(unsigned int k=0;k<n;k++)
      {
        float d=fabsf(at((int)k+1)-at((int)k-1));
        rawW[k]=d/kEdgeBoostThreshold;
        if(rawW[k]>1.f) rawW[k]=1.f;
      }
    std::vector<float> w(n,0.f);
    for(unsigned int k=0;k<n;k++)
      {
        float sum=0.f,wsum=0.f;
        for(int t=-(int)kEdgeBoostSmoothHalf;t<=(int)kEdgeBoostSmoothHalf;t++)
          {
            float tw=(float)(kEdgeBoostSmoothHalf+1-abs(t));   // triangular taper
            int i=(int)k+t; if(i<0) i=0; if(i>=(int)n) i=(int)n-1;
            sum+=tw*rawW[i];
            wsum+=tw;
          }
        w[k]=sum/wsum;
      }
    for(unsigned int k=0;k<n;k++)
      arr[k]=clampByte((1.f-w[k])*(float)arr[k]+w[k]*boosted[k]);
  }

  // RX idea 4: regularized (Wiener-style) inverse of the combined TX-pre-emphasis + channel slot-
  // domain response, applied to already-demodulated Cr/Cb (opt-in, chromaDeconvolutionEnabled()).
  // Symmetric (zero group delay), 7 taps. Matched to the narrow filter only -- the call site in
  // showLine() must not apply this when Cr/Cb came from the wide track instead.
  //
  // Design (see tests/jb60_loopback/README.md, "RX idea 4" section, for the full derivation and
  // sweep tables): H(f) was measured through the real chain (--dump-slots), not from raw taps, as a
  // Y-luma-held-constant Cr/Cb step so downsampleChroma()'s guided weights reduce to a plain box
  // average -- this response already includes the unconditional TX pre-emphasis above, i.e. it's
  // the *combined* response per the design-target decision in the RX idea 4 plan. The inverse is
  // Hinv(f)=|H(f)|/(|H(f)|^2+K), K=0.07, with DC pinned to an exact 1.0 (not the Wiener-shrunk
  // value -- the channel's own DC gain is already ~1, so flat/already-correct colour must pass
  // through unchanged) and the target held flat past 1100 Hz, where the measurement itself becomes
  // noise-dominated (matches the narrow filter's own -15dB@804Hz/null~1150Hz). Fit to a 7-tap
  // symmetric FIR by weighted least squares over the cosine basis. Tap-count sweep (3/5/7/9 at this
  // K): gains grow with diminishing returns past 7, matching idea 6's own precedent. K sweep at 7
  // taps: broad flat optimum 0.05-0.1, regressing below baseline outside about 0.02-0.3 -- 0.07
  // chosen near that optimum with a smaller centre tap than the 0.05 peak (less noise-amplification
  // risk for a ~0.01dB difference in clean-signal gain). SNR sweep (card and a hard-edged colour
  // image, 40dB down to 0dB): a real noise crossover, around 22-25dB SNR (small, growing losses
  // below it; small, consistent gains above) -- unlike idea 6's TX-side pre-emphasis, this is an
  // RX-side inverse and so amplifies whatever noise already arrived along with genuine signal, per
  // its own original risk framing. Default off; the RX checkbox tooltip says "about 25 dB".
  const unsigned int kDeconvHalfTaps=3;
  const float kDeconvKernel[kDeconvHalfTaps+1]={1.186478f, 0.040282f, -0.025199f, -0.108322f};

  void applyChromaDeconvolution(unsigned char *arr,unsigned int n)
  {
    if(n<1) return;
    std::vector<unsigned char> src(arr,arr+n);   // read from a copy, not partially-overwritten neighbours
    auto at=[&](int i)->float
      {
        if(i<0) i=0;
        if(i>=(int)n) i=(int)n-1;
        return (float)src[i];
      };
    for(unsigned int k=0;k<n;k++)
      {
        float v=kDeconvKernel[0]*at((int)k);
        for(unsigned int t=1;t<=kDeconvHalfTaps;t++)
          v+=kDeconvKernel[t]*(at((int)k-(int)t)+at((int)k+(int)t));
        arr[k]=clampByte(v);
      }
  }

  inline unsigned char encodeD(float d)
  {
    float a=fabsf(d);
    if(a>kDMax) a=kDMax;
    float v=kDMax*powf(a/kDMax,kDGamma);
    return clampByte(128.f+(d<0 ? -v : v));
  }

  /*!
    Linear interpolation of n samples (sample j centred at (j+0.5)*width/n) at column x
  */
  inline float interpolate(const float *s,unsigned int n,unsigned int x)
  {
    double pw=(double)kWidth/(double)n;
    double jf=(x+0.5)/pw-0.5;
    int j0=(int)floor(jf);
    float t=(float)(jf-j0);
    int j1=j0+1;
    if(j0<0) j0=0;
    if(j0>(int)n-1) j0=(int)n-1;
    if(j1<0) j1=0;
    if(j1>(int)n-1) j1=(int)n-1;
    return s[j0]*(1.f-t)+s[j1]*t;
  }
}

modeJB60::modeJB60(esstvMode m,unsigned int len,bool tx,bool narrowMode): modeBase(m,len,tx,narrowMode)
{
  slot=0;
  prevSample=0;
  prevSampleWide=0;
  for(int d=0;d<256;d++)
    {
      float x=d/kGuideSigma;
      guideLut[d]=expf(-x*x);
      float u=(d-128)/kDMax;
      if(u>1.f) u=1.f;
      if(u<-1.f) u=-1.f;
      float v=kDMax*powf(fabsf(u),1.f/kDGamma);
      dDecodeLut[d]=(u<0) ? -v : v;
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
  curL.assign(kSegCount[SEG_L],128);
  curD.assign(kSegCount[SEG_D],128);
  curCr.assign(kSegCount[SEG_CR],128);
  curCb.assign(kSegCount[SEG_CB],128);
  prevL=curL;
  prev2L=curL;
  prevD=curD;
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
      calcPixelPositionTable(SEG_L,false);
      markerFloat+=kSegCount[SEG_L]*slot;
      pixelArrayPtr=yArrayPtr;
      return MBPIXELS;
    case 2:
      debugState=stColorLine1;
      calcPixelPositionTable(SEG_D,false);
      markerFloat+=kSegCount[SEG_D]*slot;
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
      calcPixelPositionTable(SEG_L,true);
      pixelArrayPtr=yArrayPtr;
      return MBPIXELS;
    case 1:
      calcPixelPositionTable(SEG_D,true);
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
  // Cr/Cb only: when the "Wide Video Filter" setting is on, read the wide-filter track instead of the narrow
  // one. L and D (any other debugState) always use the narrow, noise-robust track -- see videofilterselection.h.
  const bool chromaWide=(debugState==stColorLine2 || debugState==stColorLine3) && wideVideoFilterEnabled();
  const quint16 s=chromaWide ? sampleWide : sample;
  const quint16 ps=chromaWide ? prevSampleWide : prevSample;
  if(sampleCounter>=pixelPositionTable[pixelCounter]+(slot/2))
    {
      double avg=((double)s+(double)ps)/2.;
      color=128+lround((avg-fc)*255./dev);
      if(color<0) color=0;
      if(color>255) color=255;
      pixelArrayPtr[pixelCounter]=(unsigned char)color;
      pixelCounter++;
      prevSample=sample;
      prevSampleWide=sampleWide;
      if(pixelCounter>=segmentPixels) return true;
      return false;
    }
  prevSample=sample;
  prevSampleWide=sampleWide;
  return false;
}

/*!
  Transmit side helper: the rows 2*pair and 2*pair+1 of the picture (a pair outside the picture repeats the
  edge pair). Fills the quantized mean luminance l (what the receiver will see as L), and, when not NULL,
  the luminance of both rows and the mean red and blue.
*/
void modeJB60::txPairLuma(int pair,unsigned char *l,float *ya,float *yb,float *rBar,float *bBar)
{
  const int lastPair=(int)activeSSTVParam->numberOfDataLines-1;
  if(pair<0) pair=0;
  if(pair>lastPair) pair=lastPair;
  unsigned int *rowA=txImage()->getScanLineAddress(2*pair);
  unsigned int *rowB=txImage()->getScanLineAddress(2*pair+1);
  for(unsigned int c=0;c<kWidth;c++)
    {
      unsigned int ta=rowA[c],tb=rowB[c];
      float a=(59*qGreen(ta)+30*qRed(ta)+11*qBlue(ta))/100.f;
      float b=(59*qGreen(tb)+30*qRed(tb)+11*qBlue(tb))/100.f;
      l[c]=clampByte((a+b)/2.f);
      if(ya) ya[c]=a;
      if(yb) yb[c]=b;
      if(rBar) rBar[c]=(qRed(ta)+qRed(tb))/2.f;
      if(bBar) bBar[c]=(qBlue(ta)+qBlue(tb))/2.f;
    }
}

/*!
  Transmit side, takes pair p=lineCounter (image rows 2p and 2p+1). D is coded against the prediction
  the receiver will make from the quantized L of the pairs on either side, so both ends use the same numbers.
*/
void modeJB60::getLine()
{
  const int p=(int)lineCounter;
  unsigned char lPrev[kWidth],lCur[kWidth],lNext[kWidth];
  float ya[kWidth],yb[kWidth],rBar[kWidth],bBar[kWidth];
  float dRes[kWidth],crPix[kWidth],cbPix[kWidth],lMean[kWidth];
  unsigned int c,k;

  txPairLuma(p-1,lPrev,NULL,NULL,NULL,NULL);
  txPairLuma(p+1,lNext,NULL,NULL,NULL,NULL);
  txPairLuma(p,lCur,ya,yb,rBar,bBar);

  for(c=0;c<kWidth;c++)
    {
      yArrayPtr[c]=lCur[c];
      dRes[c]=(ya[c]-yb[c])/2.f-((float)lPrev[c]-(float)lNext[c])/8.f;
      lMean[c]=(ya[c]+yb[c])/2.f;
      crPix[c]=(rBar[c]-lMean[c])/1.4f+127.5f;      // same definitions as PD: Cr=(R-Y)/1.4+127.5
      cbPix[c]=(bBar[c]-lMean[c])/1.78f+127.5f;     //                        Cb=(B-Y)/1.78+127.5
    }
  // box average over the footprint of each sample
  for(k=0;k<kSegCount[SEG_D];k++)
    {
      unsigned int c0,c1;
      float sum=0;
      columnFootprint(kSegCount[SEG_D],k,c0,c1);
      for(c=c0;c<=c1;c++) sum+=dRes[c];
      greenArrayPtr[k]=encodeD(sum/(c1-c0+1));
    }
  downsampleChroma(crPix,lMean,kSegCount[SEG_CR],redArrayPtr);
  downsampleChroma(cbPix,lMean,kSegCount[SEG_CB],blueArrayPtr);
  std::vector<unsigned char> rawCr,rawCb;   // idea 12's own kernel starts from the same raw input idea 6 does
  if(chromaEdgeBoostEnabled())
    {
      rawCr.assign(redArrayPtr,redArrayPtr+kSegCount[SEG_CR]);
      rawCb.assign(blueArrayPtr,blueArrayPtr+kSegCount[SEG_CB]);
    }
  applyChromaPreEmphasis(redArrayPtr,kSegCount[SEG_CR]);
  applyChromaPreEmphasis(blueArrayPtr,kSegCount[SEG_CB]);
  if(chromaEdgeBoostEnabled())
    {
      applyChromaEdgeBoost(rawCr.data(),redArrayPtr,kSegCount[SEG_CR]);
      applyChromaEdgeBoost(rawCb.data(),blueArrayPtr,kSegCount[SEG_CB]);
    }
}

/*!
  Transmit side: one chroma sample is the luma-guided mean of its footprint of source columns, instead of a
  flat box average. Pixels closer in luminance to the footprint's centre column count more, the same
  similarity kernel (guideLut/kGuideSigma/kGuideFloor) that upsampleChroma already uses on the way back out.
  A footprint with uniform luma reduces to the old flat average (every weight equal); one that straddles a
  real luma edge is pulled toward the colour on the anchor's side instead of blending across the edge, so the
  channel's fixed time-domain blur smears a value closer to a genuine step rather than an already-blended one.
*/
void modeJB60::downsampleChroma(const float *pix,const float *lum,unsigned int n,unsigned char *out)
{
  unsigned int c,k;
  for(k=0;k<n;k++)
    {
      unsigned int c0,c1;
      columnFootprint(n,k,c0,c1);
      unsigned int cc=(c0+c1)/2;
      float anchor=lum[cc],sum=0,wsum=0;
      for(c=c0;c<=c1;c++)
        {
          int d=(int)lroundf(fabsf(lum[c]-anchor));
          if(d>255) d=255;
          float w=kDownsampleFloor+guideLut[d];
          sum+=w*pix[c];
          wsum+=w;
        }
      out[k]=clampByte(sum/wsum);
    }
}

/*!
  Upsample one chroma component (n samples) to the full row. Between the two nearest chroma
  samples the linear weight is multiplied by how well the pixel's luminance matches the mean
  luminance under each sample, so chroma edges snap to luminance edges.
*/
void modeJB60::upsampleChroma(const unsigned char *y,const unsigned char *c,unsigned int n,unsigned char *out)
{
  float yb[kWidth];
  const double pw=(double)kWidth/(double)n;
  unsigned int j,x;
  for(j=0;j<n;j++)
    {
      unsigned int c0,c1;
      unsigned int sum=0;
      columnFootprint(n,j,c0,c1);
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

/*!
  Rebuild the two rows of one pair. lPrev and lNext are the L lines of the pairs before and after (the
  pair itself at the picture edges); the vertical detail is the transmitted residual plus the prediction
  from them. Writes the rows at displayLineCounter through yuvConversion.
*/
void modeJB60::emitPair(const unsigned char *lPrev,const unsigned char *l,const unsigned char *lNext,
                        const unsigned char *d,const unsigned char *cr,const unsigned char *cb)
{
  float dExp[kSegCount[SEG_D]];
  float dFull[kWidth];
  unsigned int c,k;
  for(k=0;k<kSegCount[SEG_D];k++) dExp[k]=dDecodeLut[d[k]];
  for(c=0;c<kWidth;c++)
    {
      dFull[c]=interpolate(dExp,kSegCount[SEG_D],c)+((float)lPrev[c]-(float)lNext[c])/8.f;
    }
  for(int row=0;row<2;row++)
    {
      for(c=0;c<kWidth;c++)
        {
          rowY[c]=clampByte((row==0) ? l[c]+dFull[c] : l[c]-dFull[c]);
        }
      upsampleChroma(rowY.data(),cr,kSegCount[SEG_CR],redArrayPtr);
      upsampleChroma(rowY.data(),cb,kSegCount[SEG_CB],blueArrayPtr);
      yuvConversion(rowY.data());   // writes the row at displayLineCounter and advances it
    }
}

/*!
  Receive side, called once per line pair. A pair can only be finished once the L line of the next pair is
  known, so the picture trails the received data by one pair: pair p finishes pair p-1 (and, at the end of
  the picture, itself as well).
*/
void modeJB60::showLine()
{
  const unsigned int p=lineCounter;
  const bool last=(p+1>=activeSSTVParam->numberOfDataLines);

  curL.assign(yArrayPtr,yArrayPtr+kSegCount[SEG_L]);
  curD.assign(greenArrayPtr,greenArrayPtr+kSegCount[SEG_D]);
  curCr.assign(redArrayPtr,redArrayPtr+kSegCount[SEG_CR]);
  curCb.assign(blueArrayPtr,blueArrayPtr+kSegCount[SEG_CB]);
  // RX idea 4: opt-in, and only meaningful against the narrow filter this kernel was measured
  // against -- when the Cr/Cb wide-filter track is in use instead, skip it rather than apply a
  // mismatched inverse (the UI also grays out the checkbox in that case; this is the behavioral
  // guard that still applies even if settings are edited outside the UI, e.g. the test harness
  // override or a hand-edited config file).
  if(chromaDeconvolutionEnabled() && !wideVideoFilterEnabled())
    {
      applyChromaDeconvolution(curCr.data(),kSegCount[SEG_CR]);
      applyChromaDeconvolution(curCb.data(),kSegCount[SEG_CB]);
    }

  if(p>0)
    {
      // finish pair p-1; before the first pair there is nothing above it, so it repeats its own L
      emitPair((p>1) ? prev2L.data() : prevL.data(),prevL.data(),curL.data(),prevD.data(),prevCr.data(),prevCb.data());
    }
  if(last)
    {
      // nothing follows the last pair: it repeats its own L below
      emitPair((p>0) ? prevL.data() : curL.data(),curL.data(),curL.data(),curD.data(),curCr.data(),curCb.data());
    }

  prev2L.swap(prevL);
  prevL.swap(curL);
  prevD.swap(curD);
  prevCr.swap(curCr);
  prevCb.swap(curCb);
}
