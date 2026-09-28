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
#ifndef MODEJB60_H
#define MODEJB60_H

#include "modebase.h"
#include <vector>

/*!
  JB60 sends 640x496 pictures in 248 line pairs at PD120's pixel rate (190 us/slot, so the same audio
  bandwidth) with about half the samples of PD120. The saving is vertical, never horizontal: the receive
  video filter smears every slot over about four slots in time, so a slot that covers more than one
  picture pixel makes the picture blurrier in proportion. Each line pair is:

  - L  (640 slots): the mean luminance of the two rows, one slot per pixel exactly as PD120 sends a row.
  - D  (144 slots): the vertical detail (row 2p minus row 2p+1, halved), sent as a residual against the
    prediction (L(p-1)-L(p+1))/8 that the receiver makes from the neighbouring L lines, box averaged
    horizontally and companded. Vertical detail is small except at horizontal edges, which extend
    horizontally, so the coarse horizontal sampling costs little.
  - Cr (224 slots) and Cb (176 slots): chrominance of the pair, defined as in PD. The receiver upsamples
    them guided by the luminance.

  The receiver rebuilds rows 2p and 2p+1 as L +/- D. A pair needs the L lines on both sides, so the
  picture trails the received data by one pair.

  Line layout (trailing sync like PD): bp, L, D, Cr, Cb, fp, sync.
*/
class modeJB60 : public modeBase
{
public:
  modeJB60(esstvMode m, unsigned int len, bool tx, bool narrowMode);
  ~modeJB60();
  bool getPixels();
protected:
  embState rxSetupLine();
  embState txSetupLine();
  void setupParams(double clock);
  void showLine();
  void getLine();
private:
  void calcPixelPositionTable(unsigned int segment,bool tx);
  void txPairLuma(int pair,unsigned char *l,float *ya,float *yb,float *rBar,float *bBar);
  void upsampleChroma(const unsigned char *y,const unsigned char *c,unsigned int n,unsigned char *out);
  void emitPair(const unsigned char *lPrev,const unsigned char *l,const unsigned char *lNext,
                const unsigned char *d,const unsigned char *cr,const unsigned char *cb);

  DSPFLOAT slot;                       //!< duration of one sample slot (in samples of the local clock)
  quint16 prevSample;                  //!< previous demodulator sample (RX slot averaging)
  float guideLut[256];                 //!< luminance similarity weight for chroma upsampling
  float dDecodeLut[256];               //!< expands a received D level back to a luminance difference
  std::vector<unsigned char> rowY;     //!< reconstructed luminance row
  std::vector<unsigned char> curL,curD,curCr,curCb;
  std::vector<unsigned char> prevL,prevD,prevCr,prevCb;
  std::vector<unsigned char> prev2L;
};

#endif
