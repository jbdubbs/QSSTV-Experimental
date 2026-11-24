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
#include "modeavt.h"
#include "configparams.h"
// one number is 1 startbit + 16 databits.
// the header of the AVT modes: 32 words of a 1900 Hz pulse and 16 bits, each 9.7646 ms (2048/20 Bd), then 0.305 ms before the first line.
// (MMSSTV.EXE's own transmission, of a picture of known content, 2026-11-23: its last word's 1600 Hz tail runs straight into the picture, about 0.3 ms after
// the last bit, no gap tone. Its receiver draws the picture 2.9 ms (7.6 px in AVT 90) to the left of where it was transmitted: it starts the first line a third of a
// bit after the tone leaves the 1600 Hz of the last bit, which it also does with our own old 3.255 ms gap tone. The picture is placed where it was sent.)
#define BITTIME 0.0097646
#define WORDTIME (17*BITTIME)
#define HEADERGAP 0.00030514375
#define VISTOIMAGECORRECTION 0 // samples at 12 kHz added to the start of the image that a picture without a readable header gets from its VIS code (the receiver starts 16 ms before the end of the code, where the stop bit ends, as for every picture that a VIS code starts)
#define STARTCORRECTION -11   // samples at 12 kHz that the filters of the receiver delay the end of the code
#include <QString>



QString stateStr[WAITSTART+1]=
{
    "D1900","D1900END","DELAYHALF","DELAYFULL","BITS","CALCDELAY","WAITSTART"
};

modeAVT::modeAVT(esstvMode m,unsigned int len, bool tx,bool narrowMode):modeBase(m,len,tx,narrowMode)
{
  avtTrailerDetect=true;
  trailerState=D1900;
  code=0;
  duration=0;
  startOffset=0;
  clockStarted=false;
}


modeAVT:: ~modeAVT()
{
}


void modeAVT::setupParams(double clock)
{
  visibleLineLength=(getLineLength(mode,clock))/3.;
}


modeBase::eModeBase modeAVT::process(quint16 *demod,unsigned int syncPos,bool goToSync,unsigned int rxPos)
{
  unsigned int i=0;
  unsigned char a,b;
  if(goToSync && !clockStarted)
    {
      // the receiver starts where the stop bit of the VIS code ends (syncPos into this block): the header follows it, and the image follows the header
      clockStarted=true;
      startOffset=rxPos+syncPos;     // the sample counter of that place
    }
  if(avtTrailerDetect)
  {
      for(;i<length;i++)
        {
          // The picture that a VIS code started runs to its end, like any other: when no word of the header could be read, the image starts
          // where the header ends
          if(trailerState!=WAITSTART && rxPos+i>=startOffset+(unsigned int)(((32*WORDTIME+HEADERGAP)*rxClock/SUBSAMPLINGFACTOR)+VISTOIMAGECORRECTION))
            {
              avtTrailerDetect=false;
              debugState=stColorLine0;
              return modeBase::process(demod,i,true,rxPos);
            }
          duration++;
          sample=demod[i];
          debugStatePtr[i]=debugState;
          avgSample+=0.9*(sample-avgSample);
          switch(trailerState)
            {
                case D1900: // wait for 1900 if detected; wait for end
                  if(fabs(avgSample-1900.) <50.)
                    {
                      debugState=st1900B;
                      count++;
                    }
                  else
                    {
                      if (count >0) count --;
                      else duration=0;
                    }
                  if(count>50)
                    {
                      count=10;
                      addToLog(QString("D1900 start: %1").arg(rxPos+i-50),LOGMODES);
                      switchTrailerState(D1900END);
                    }
                break;
                case D1900END: //  wait for end 1900
                  if(fabs(avgSample-1900.) <50.)
                    {
                      if(count<10) count++;
                      // a tone longer than a sync pulse is the leader of the header: wait for its end, then look for the pulses
                    }
                  else
                    {
                      debugState=st1900E;
                      if (count >0) count --;
                    }
                  if (count==0) // end of 1900 detected
                    {
                      duration-=5;
                      addToLog(QString("duration: %1, sampleCounter: %2").arg(duration).arg(rxPos+i-10),LOGMODES);
                      if((duration<(unsigned int)(0.011*rxClock/SUBSAMPLINGFACTOR)) && (duration>(unsigned int)(0.009*rxClock/SUBSAMPLINGFACTOR)))
                        {
                          bitCounter=0;
                          count=10;
                          switchTrailerState(DELAYHALF);
                        }
                       else
                        {

                         switchTrailerState(D1900);   // not a pulse of the header (the leader, the VIS code): keep looking
                         duration=0;
                        }
                    }
                break;
                case DELAYHALF:
                  debugState=stHALF;
                  count++;
                  code=0;
                  if(count>=(unsigned int)(round((BITTIME/2)*rxClock/SUBSAMPLINGFACTOR))) switchTrailerState(BITS);
                break;
                case BITS:
                  debugState=stBITS+bitCounter;
                  code=code<<1;
                  if(avgSample>1900.) code|=0x0001;
                  bitCounter++;
                  if (bitCounter==16) switchTrailerState(CALCDELAY);
                  else
                    {
                      count=0;
                      switchTrailerState(DELAYFULL);
                    }
                break;
                case DELAYFULL:
                  debugState=stFULL;
                  count++;
                  if(count>=(unsigned int)(round(BITTIME*rxClock/SUBSAMPLINGFACTOR))) switchTrailerState(BITS);
                break;
                case CALCDELAY:
                  //check if
                  a=code>>8;
                  b=(code&0xFF)^0xFF;
                  addToLog(QString("avtcode =%1 mode=%2,pos=%3").arg(QString::number(code,16)).arg((code&0xE000)>>13).arg((code&0x1F00)>>8),LOGMODES);
                  count=0;
                  duration=0;
                  if(a!=b)
                    {
                      switchTrailerState(D1900);
                      break;
                    }
                   a&=0x1F;
                   delay=(unsigned int)((((31-a)*WORDTIME+BITTIME/2+HEADERGAP)*rxClock/SUBSAMPLINGFACTOR)+STARTCORRECTION);
                   switchTrailerState(WAITSTART);
                break;
                case WAITSTART:
                  debugState=stWAIT;
                  delay--;
                  if(delay==0)
                    {
                      avtTrailerDetect=false;
                      debugState=stColorLine0;
                    return modeBase::process(demod,i,true,rxPos);
                    }
                break;
            }
        }

  }


  else
    {
      return modeBase::process(demod,0,false,rxPos);
    }
  return MBRUNNING;
}


void  modeAVT::switchTrailerState(eTrailerState newState)
{
  if(trailerState!=newState)
    {
      addToLog(QString("Swtching from %1 to %2").arg(stateStr[trailerState]).arg(stateStr[newState]) ,LOGMODES);
    }
  trailerState=newState;
}



modeBase::embState modeAVT::rxSetupLine()
{	
  start=lineTimeTableRX[lineCounter];
//	addToLog(QString("modeAVT: subLine %1").arg(subLine),LOGMODES);
	
  switch(subLine)
		{
			case 0:
        calcPixelPositionTable(REDLINE,false);
        pixelArrayPtr=redArrayPtr;
				return MBPIXELS;
      case 1:
        calcPixelPositionTable(GREENLINE,false);
        pixelArrayPtr=greenArrayPtr;
				return MBPIXELS;
      case 2:
        calcPixelPositionTable(BLUELINE,false);
        pixelArrayPtr=blueArrayPtr;
				return MBPIXELS;
			break;
			default:
				return MBENDOFLINE;
		}
}

void modeAVT::calcPixelPositionTable(unsigned int colorLine,bool tx)
{
	unsigned int i;
	int ofx=0;
	if(tx) ofx=1;
	debugState=stColorLine0+colorLine;
	
	switch (colorLine)
		{
      case REDLINE:

			break;
      case GREENLINE:
        start+=(visibleLineLength);
			break;
      case BLUELINE:
        start+=(2.*visibleLineLength);
			break;
		}
  for(i=0;i<activeSSTVParam->numberOfPixels;i++)
		{
      pixelPositionTable[i]=(unsigned int)round(start+(((float)(i+ofx)*visibleLineLength)/activeSSTVParam->numberOfPixels));
		}
}


modeBase::embState modeAVT::txSetupLine()
{
  // the line is red, green and blue, each a third of it, without gaps and without a sync pulse (the sync is the header sent before the image)
  start=lineTimeTableTX[lineCounter];
  switch(subLine)
		{
			case 0:
				calcPixelPositionTable(REDLINE,true);
				pixelArrayPtr=redArrayPtr;
				return MBPIXELS;
			case 1:
				calcPixelPositionTable(GREENLINE,true);
				pixelArrayPtr=greenArrayPtr;
				return MBPIXELS;
			case 2:
				calcPixelPositionTable(BLUELINE,true);
				pixelArrayPtr=blueArrayPtr;
				return MBPIXELS;
			default:
				return MBENDOFLINE;
		}
}
