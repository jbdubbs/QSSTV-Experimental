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
  JB60 sends 640x496 pictures in 248 line pairs at PD120's pixel rate (190 us/slot, so the same
  audio bandwidth) but with about half the samples:

  - Y0 (row 2p) and Y1 (row 2p+1) carry luminance on a quincunx lattice: 320 samples per row,
    even columns on the even row and odd columns on the odd row. The receiver rebuilds the
    missing pixels with edge directed interpolation.
  - Cr (320 samples) and Cb (224 samples) are shared by both rows of the pair. The receiver
    upsamples them guided by the reconstructed luminance.

  Line layout (trailing sync like PD): bp, Y0, Y1, Cr, Cb, fp, sync.
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
  void reconstructLuma(unsigned char *out,const std::vector<unsigned char> &own,unsigned int parity,
                       const std::vector<unsigned char> *up,const std::vector<unsigned char> *down);
  void upsampleChroma(const unsigned char *y,const std::vector<unsigned char> &c,unsigned char *out);
  void emitRow(const std::vector<unsigned char> &own,unsigned int parity,
               const std::vector<unsigned char> *up,const std::vector<unsigned char> *down,
               const std::vector<unsigned char> &cr,const std::vector<unsigned char> &cb);

  DSPFLOAT slot;                       //!< duration of one sample slot (in samples of the local clock)
  quint16 prevSample;                  //!< previous demodulator sample (RX slot averaging)
  float guideLut[256];                 //!< luminance similarity weight for chroma upsampling
  std::vector<unsigned char> rowY;     //!< reconstructed luminance row
  std::vector<unsigned char> curY0,curY1,curCr,curCb;
  std::vector<unsigned char> prevY0,prevY1,prevCr,prevCb;
};

#endif
