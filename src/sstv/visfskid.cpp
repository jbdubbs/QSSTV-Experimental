#include "visfskid.h"
#include <QDebug>


const QString fskStateStr[fskIdDecoder::GETID+1]=
{
  "FSKINIT",
  "WAITS1500",
  "WAITE1500",
  "WAITS1900",
  "WAITE1900",
  "WAITS2100",
  "WAITE2100",
  "GETID"
};

const QString visStateStr[visDecoder::GETCODE+1]=
{
  "VISINIT",
  "WAITS1900",
  "WAITE1900",
  "WAITS2100",
  "WAITE2100",
  "WAITSTARTBIT",
  "GETCODE"
};

//const QString retraceStateStr[retraceDetector::RETRACEWAITEND+1]=
//{
//  "RETRACEINIT",
//  "RETRACESTART",
//  "RETRACEEND",
//};



fskDecoder::fskDecoder()
{
  sampleCounter=0;
  syncSampleCounter=0;
}


bool fskDecoder::waitStartFreq(unsigned int freqL,unsigned int freqH)
{
  if((avgFreq>=freqL) && (avgFreq<=freqH))
    {
      if(avgCounter<avgCount) avgCounter++;
      else
        {
          timeoutCounter=0;
          return true;
        }
    }
  else
    {
      if(avgCounter) avgCounter--;
    }
  return false;
}

bool fskDecoder::waitStartFreq(unsigned int freqL, unsigned int freqH, unsigned long maxWait, bool &timeout)
{
  if((avgFreq>=freqL) && (avgFreq<=freqH))
    {
      if(avgCounter<avgCount)
      {
          avgCounter++;
      }
      else
        {
          timeoutCounter=0;
          return true;
        }
    }
  else
    {
      if(avgCounter) avgCounter--;
    }
  if (timeoutCounter++>maxWait)
    {
      timeout=true;
    }
  else
    {
      timeout=false;
    }
  return false;
}

bool fskDecoder::waitEndFreq(unsigned int freqL,unsigned int freqH)
{
  if((avgFreq<freqL) || (avgFreq>freqH))
    {
      if(avgCounter) avgCounter--;
      else return true;
    }
  else
    {
      if(avgCounter<avgCount) avgCounter++;
    }
  return false;
}

bool fskDecoder::waitEndFreq(unsigned int freqL, unsigned int freqH, unsigned long maxWait, bool &timeout)
{
  if((avgFreq<freqL) || (avgFreq>freqH))
    {
      if(avgCounter<avgCount) avgCounter++;
      else
        {
          timeoutCounter=0;
          return true;
        }
    }
  else
    {
      if(avgCounter) avgCounter--;
    }
  if (timeoutCounter++>maxWait)
    {
      timeout=true;
    }
  else
    {
      timeout=false;
    }
  return false;
}




void fskDecoder::init()
{
  avgCounter=0;
  bitCounter=0;
  code=0;
  checksum=0;
  sampleCounter=0;
  timeoutCounter=0;
}



fskIdDecoder::fskIdDecoder()
{
  fskState=FSKINIT;
}




bool fskIdDecoder::assemble(bool reset)
{
  if(reset)
    {
      checksum=0;
      headerFound=false;
      endFound=false;
      fskIDStr.clear();
      return true;
    }
  if(!headerFound) // waiting for start
    {
      if(symbol==0x2A)
        {
          addToLog("headerFound",LOGFSKID);
          headerFound=true;
          return false;
        }
      return true; // indicate resync

    }
  if(symbol==0x01)
    {
      endFound=true;
      return false;
    }
  if(endFound)
    {
      if(checksum!=symbol)
        {
          fskIDStr.clear();
        }
      emit callReceived(fskIDStr);
      fskIDStr.clear();
      return true;
    }
  else
    {
      checksum=checksum^symbol;
      fskIDStr.append(char(symbol+0x20));
      addToLog(QString("fskstr %1 %2").arg(fskIDStr).arg(QString::number(symbol,16)),LOGFSKID);
      if(fskIDStr.length()>9)
        {
          fskIDStr.clear();
          return true;
        }
    }
  return false;
}

void fskIdDecoder::reset()
{
  avgCount=FSKAVGCOUNT;
  switchState(FSKINIT,0);
  init();
}

void fskIdDecoder::switchState(efskState newState,unsigned int i)
{
  Q_UNUSED(i);
  addToLog(QString("%1 to %2 at samplecounter:%3 total: %4")
           .arg(fskStateStr[fskState])
           .arg(fskStateStr[newState])
           .arg(sampleCounter+i)
           .arg(syncSampleCounter+i)
           ,LOGFSKID);
  fskState=newState;
}


void fskIdDecoder::extract(unsigned int syncSampleCtr, bool narrow)
{
  int i;
  if(narrow) return;
  syncSampleCounter=syncSampleCtr;
  bool timeout;
  for(i=0;i<RXSTRIPE;i++)
    {
      avgFreq=freqPtr[i];
      switch (fskState)
        {
        case FSKINIT:
          sampleCounter=0;
          avgCounter=0;
          switchState(WAITSTART1500,i);
          timeoutCounter=0;
          break;
        case WAITSTART1500:
          if(waitStartFreq(1400,1600))
            {
              switchState(WAITEND1500,i);
              startSampleCounter=sampleCounter+i;
            }
          break;
        case WAITEND1500:
          {
            if(waitEndFreq(1400,1600))
              {
                if(((sampleCounter+i-startSampleCounter)>=FSKMIN1500))
                  {
                    switchState(WAITSTART2100,i);
                    timeoutCounter=0;
                  }
                else switchState(FSKINIT,i);
              }
          }
          break;

        case WAITSTART2100:
          if(waitStartFreq(2000,2200,100,timeout))
            {
              switchState(WAITEND2100,i);
              startSampleCounter=sampleCounter+i;
            }
          else if(timeout)
            {
              switchState(FSKINIT,i);
            }
          break;


        case WAITEND2100:
          {
            if(waitEndFreq(2000,2200))
              {
                if(((sampleCounter+i-startSampleCounter)>=FSKMIN2100) && ((sampleCounter+i-startSampleCounter)<2*FSKMIN2100))
                  {
                    switchState(WAITSTART1900,i);
                    timeoutCounter=0;
                  }
                else switchState(FSKINIT,i);
              }
          }
          break;

        case WAITSTART1900:
          if(waitStartFreq(1800,2000,50,timeout))
            {
              switchState(WAITEND1900,i);
              startSampleCounter=sampleCounter+i;
            }
          else if(timeout)
            {
              switchState(FSKINIT,i);
            }
          break;
        case WAITEND1900:
          {
            if((sampleCounter+i-startSampleCounter)>=(FSKBIT/2-avgCount-56))
              {
                switchState(GETID,i);
                startSampleCounter=sampleCounter+i;
                assemble(true);
                bitCounter=0;
                symbol=0;
              }
          }
          break;
        case GETID:
          if((sampleCounter+i-startSampleCounter)>=FSKBIT)
            {

              startSampleCounter=sampleCounter+i;
              symbol>>=1;

              if(avgFreq<2000)
                {
                  symbol|=0x20;
                }
              addToLog(QString("bit %1, %2 - %3 ").arg(bitCounter).arg(sampleCounter+i).arg(QString::number(symbol,2)),LOGFSKID);
              bitCounter++;
              if(bitCounter==6)
                {
                  if(assemble(false))
                    {
                      switchState(FSKINIT,i);
                    }
                  bitCounter=0;
                }
            }
          break;
        }
    }
  sampleCounter+=RXSTRIPE;
}



// VISDECODER
// h1 1900 Hz 300
// p1 1200 Hz 10
// h2 1900 Hz 300
// c1 1200 Hz 30
// d0 1100 Hz 30 (1)  - 1300 Hz (0)
// d1 1100 Hz 30 (1)  - 1300 Hz (0)
// d2 1100 Hz 30 (1)  - 1300 Hz (0)
// d3 1100 Hz 30 (1)  - 1300 Hz (0)
// d4 1100 Hz 30 (1)  - 1300 Hz (0)
// d5 1100 Hz 30 (1)  - 1300 Hz (0)
// d6 1100 Hz 30 (1)  - 1300 Hz (0)
// d7 1100 Hz 30 (1)  - 1300 Hz (0)
// c2 1200 Hz 30
visDecoder::visDecoder()
{
  visState=VISINIT;
}


void visDecoder::reset()
{
  avgCount=VISAVGCOUNT;
  leaderTotal=leaderGap=0;
  switchState(VISINIT,0);
  init();
}

/**
 * @brief visDecoder::leaderEnded the 1900 Hz tone just stopped
 * @return true if enough leader has been seen to look for the start bit; if not, the tone is a short burst (or a dropout in a
 * leader that is still building up) and the decoder waits for it to come back
 */

bool visDecoder::leaderEnded(unsigned int i)
{
  unsigned int run=sampleCounter+i-startSampleCounter;
  if(run>=VISMINLEADERRUN) leaderTotal+=run;
  if(leaderTotal>=VISMINLEADER) return true;
  leaderDropout(i);
  return false;
}

/**
 * @brief visDecoder::leaderDropout the tone that follows the leader did not come: look for the leader again, keeping what has been seen
 */

void visDecoder::leaderDropout(unsigned int i)
{
  avgCounter=0;
  leaderGap=0;
  switchState(WAITSTART1900,i);
}

uint visDecoder::getCode()
{
  return code;
}

void visDecoder::switchState(evisState newState,unsigned int i)
{
  Q_UNUSED(i);
//  addToLog(QString("%1 to %2 at samplecounter:%3 samplecounter%4").arg(visStateStr[visState]).arg(visStateStr[newState]).arg(sampleCounter+i).arg(sampleCounter+i),LOGVISCODE);
  visState=newState;
}


void visDecoder::extract(unsigned int syncSampleCtr, bool narrow)
{
  syncSampleCounter=syncSampleCtr;
  if (narrow) extractNarrow();      // the wide code is read by visToneDecoder, from the audio
}


void visDecoder::extractNarrow()
{

  int i;
  unsigned int syncTemp;
  Q_UNUSED(syncTemp);
  bool timeout=false;
  for(i=0;i<RXSTRIPE;i++)
    {
      syncTemp=syncSampleCounter+i;
      avgFreq=freqPtr[i];
      switch (visState)
        {
        case VISINIT:
          sampleCounter=0;
          avgCounter=0;
          leaderTotal=0;
          switchState(WAITSTART1900,i);
          break;

        case WAITSTART1900:
          if(waitStartFreq(1800,2000))
            {
              switchState(WAITEND1900,i);
              startSampleCounter=sampleCounter+i;
            }
          else if(leaderTotal && (++leaderGap>VISLEADERGAP))
            {
              switchState(VISINIT,i);   // gone for too long to be a dropout in the leader
            }
          break;
        case WAITEND1900:
          {
            if(waitEndFreq(1800,2000))
              {
                if(leaderEnded(i))
                  {
                    switchState(WAITSTART2100,i);
                  }
              }
          }
          break;
        case WAITSTART2100:
          if(waitStartFreq(2000,2200,50,timeout))
            {
              switchState(WAITEND2100,i);
              startSampleCounter=sampleCounter+i;
            }
          else if(timeout)
            {
              leaderDropout(i);
            }
          break;
         case WAITEND2100:
          if(waitEndFreq(2000,2200,1250,timeout))
            {
              switchState(WAITSTARTBIT,i);
              startSampleCounter=sampleCounter+i;
            }
           else if(timeout)
             {
                switchState(VISINIT,i);
             }
            break;
        case WAITSTARTBIT:
          if(waitEndFreq(1800,2000,VISBITNARROW/2,timeout))
              {
                switchState(VISINIT,i);  // too short
              }
            if (timeout)
              {
                validCode=false;
                switchState(GETCODE,i);
                startSampleCounter=sampleCounter+i;
                bitCounter=0;
                bit=1;
                symbol=0;
              }
               break;
        case GETCODE:
          if((sampleCounter+i-startSampleCounter)>=VISBITNARROW)
            {
              startSampleCounter=sampleCounter+i;
              if(avgFreq<2000)
                {
                  symbol|=bit; //24 bits
                }
              addToLog(QString("bit %1, %2 - %3 ").arg(bitCounter).arg(syncSampleCounter+i).arg(QString::number(symbol,2)),LOGFSKID);
              bitCounter++;
              bit<<=1;
              if(bitCounter==24)
                {
                  // check for validity
                  if((symbol&0xFFF)!=0x56D)
                    {
                      validCode=false;
                    }
                  else
                    {
                      quint8 NVIS =((symbol>>12)&0x3F);
                      quint8 XNVIS=((symbol>>18)&0x3F);
                      if((NVIS^0x15)!=XNVIS)
                        {
                          validCode=false;
                        }
                      else
                        {
                          validCode=true;
                          if((mode=lookupVIS(symbol))!=NOTVALID)
                            {
                              emit visCodeNarrowDetected((int)mode,syncSampleCounter+i);
                            }
                        }
                     switchState(VISINIT,i);
                    }
                }
              addToLog(QString("bit %1, %2 - %3 ").arg(bitCounter).arg(syncSampleCounter+i).arg(QString::number(symbol,2)),LOGFSKID);
            }
          break;
        default:
          switchState(VISINIT,i);
          break;
        }
    }
  sampleCounter+=RXSTRIPE;
}




// ---------------------------------------------------------------------------------------------------------------------------
// visToneDecoder

#define VTMS(x)          ((uint)((SAMPLERATE*(x))/1000))
#define VTSTARTNEED      VTMS(15)      // 1200 Hz stronger than 1900 Hz for this long: a start bit (the 10 ms break between the leaders is shorter)
#define VTBITMS          30
#define VTHALFWINDOW     VTMS(5)       // a bit is judged on the 10 ms around its middle
#define VTRISE           VTMS(4)       // from the start of a tone to the moment its strength has risen over the start level
#define VTSTRONG         0.275         // tone strength (1 = a limited tone in the middle of its resonator) that counts as a tone
#define VTMARGIN         0.137         // least difference between 1100 and 1300 Hz that is a bit
#define VTQUIET          VTMS(500)     // idle time after a code
#define VTPICKMS         3             // ms the banks have to read the same code; the one that heard the start bit strongest times it
#define VTBANKSPACING    45            // Hz between the banks of tone detectors
#define VTEXTENDED       0x23          // first byte of the two byte codes (MR, MP and ML modes)
#define VTENDTOLINE0     16            // ms; QSSTV counts the end of a VIS code 16 ms after the start of the first line (see syncprocessor.cpp VISTOSTARTMS)
#define VTLEADNEED       100           // ms of 1900 Hz in the 450 ms before the start bit (the leader is 300 ms; the old decoder wanted 100 as well)
#define VTLEADLEVEL      0.15          // how much stronger 1900 Hz has to be than 1200 Hz for a sample of the leader
#define VTCLEAREDGE      0.30          // rise of the 1200 Hz strength (1 = a full tone) from which the edge of the start bit is believed
#define VTDELAYTRIMLEVEL 2.6           // ms; the same for a start bit found where the strength passes a level (a weak signal): that is later than where it began
#define VTDELAYTRIM      7.2           // ms; what the code is moved by so that the first line is where the sync pulses put it (tune with tests/filedecode, tests/slant and ADOPT pulses_vs_code_ms in the trace)

void visToneDecoder::biquad::lowpass(double f,double q)
{
  double w=2*M_PI*f/SAMPLERATE,al=sin(w)/(2*q),c=cos(w),a0=1+al;
  b0=(1-c)/2/a0; b1=(1-c)/a0; b2=(1-c)/2/a0; a1=-2*c/a0; a2=(1-al)/a0;
  clear();
}

void visToneDecoder::biquad::highpass(double f,double q)
{
  double w=2*M_PI*f/SAMPLERATE,al=sin(w)/(2*q),c=cos(w),a0=1+al;
  b0=(1+c)/2/a0; b1=-(1+c)/a0; b2=(1+c)/2/a0; a1=-2*c/a0; a2=(1-al)/a0;
  clear();
}

double visToneDecoder::biquad::run(double x)
{
  double y=b0*x+z1;
  z1=b1*x-a1*y+z2;
  z2=b2*x-a2*y;
  return y;
}

void visToneDecoder::tank::init(double f,double bw)
{
  double r=exp(-M_PI*bw/SAMPLERATE);
  b1=2*r*cos(2*M_PI*f/SAMPLERATE);
  b2=-r*r;
  a0=sin(2*M_PI*f/SAMPLERATE)/((SAMPLERATE/6.0)/bw);
  lp.lowpass(50,0.70710678);
  scale=1;
  clear();
  // the strength of a limited tone (a square wave) at the middle of the resonator is 1
  double d=1;
  for(int i=0;i<SAMPLERATE/2;i++) d=run(sin(2*M_PI*f*i/SAMPLERATE)>=0?1.:-1.);
  scale=1./d;
  clear();
  t50=0;
  for(int i=0;i<SAMPLERATE/10;i++)
    {
      if(run(sin(2*M_PI*f*i/SAMPLERATE)>=0?1.:-1.)>=0.5)
        {
          t50=i;
          break;
        }
    }
  clear();
}

double visToneDecoder::tank::run(double x)
{
  double y=a0*x+b1*z1+b2*z2;
  z2=z1;
  z1=y;
  return lp.run(fabs(y))*scale;
}

void visToneDecoder::tank::clear()
{
  z1=z2=0;
  lp.clear();
}

visToneDecoder::visToneDecoder()
{
  pre[0].highpass(700,0.70710678);
  pre[1].highpass(700,0.70710678);
  pre[2].lowpass(2300,0.70710678);
  pre[3].lowpass(2300,0.70710678);
  static const double offsets[3]={-VTBANKSPACING,0,VTBANKSPACING};
  for(int k=0;k<3;k++) banks[k].init(offsets[k]);
  reset();
}

void visToneDecoder::bank::init(double offset)
{
  t1080.init(1080+offset,80);
  t1200.init(1200+offset,100);
  t1320.init(1320+offset,80);
  t1900.init(1900+offset,100);
  clear();
}

void visToneDecoder::bank::clear()
{
  t1080.clear(); t1200.clear(); t1320.clear(); t1900.clear();
  for(int i=0;i<VTHIST;i++) hist[i]=0;
  histPos=0;
  for(int i=0;i<VTLEAD;i++) lead[i]=0;
  leadPos=0;
  leadCount=0;
  startOver();
}

void visToneDecoder::bank::startOver()
{
  state=IDLE;
  runStart=0;
  onset=0;
  onsetEmit=0;
  trimMs=VTDELAYTRIMLEVEL;
  plateau=0;
  symbol=0;
  bitIndex=0;
  bitsWanted=8;
  sumCount=0;
  sum11=sum13=sum19=0;
  stopSum12=stopSum19=0;
  stopCount=0;
}

void visToneDecoder::reset()
{
  for(int i=0;i<4;i++) pre[i].clear();
  for(int k=0;k<3;k++) banks[k].clear();
  localPos=0;
  quiet=0;
  candidate.valid=false;
}

/**
 * @brief visToneDecoder::process one stripe of audio (what the receiver hears, before any sync or video filter)
 * @param syncSampleCtr the sample counter of the sync processor at the start of the stripe; a code is reported in its time
 */

void visToneDecoder::process(const float *audio,unsigned int syncSampleCtr)
{
  for(int i=0;i<RXSTRIPE;i++,localPos++)
    {
      double x=audio[i];
      for(int k=0;k<4;k++) x=pre[k].run(x);
      // hard limiter: noise, a signal and a crash all count the same, so what the tones say is their share of the energy
      double y=(x>1.)?1.:((x<-1.)?-1.:x);
      for(int k=0;k<3;k++) step(banks[k],y,localPos,syncSampleCtr,i);
      if(quiet) quiet--;
      if(candidate.valid && (int)(localPos-candidate.due)>=0)
        {
          emit visCodeWideDetected(candidate.mode,(uint)candidate.counter);
          candidate.valid=false;
          quiet=VTQUIET;
          for(int k=0;k<3;k++) banks[k].startOver();
        }
    }
}

/**
 * @brief visToneDecoder::step one sample through one bank
 * @return true if the bank has read a code (and reported it)
 */

bool visToneDecoder::step(bank &b,double y,uint localPos,uint syncSampleCtr,int i)
{
  double d11=b.t1080.run(y),d12=b.t1200.run(y),d13=b.t1320.run(y),d19=b.t1900.run(y);
  b.hist[b.histPos]=(float)d12;
  b.histPos=(b.histPos+1)%VTHIST;
  unsigned char isLead=((d19-d12)>=VTLEADLEVEL)?1:0;
  b.leadCount+=isLead-b.lead[b.leadPos];
  b.lead[b.leadPos]=isLead;
  b.leadPos=(b.leadPos+1)%VTLEAD;
  if(quiet) return false;
  // (once a code has been read the other banks only finish theirs; none starts a new one)
  bool tone12=!candidate.valid && (d12>d19) && (d12>VTSTRONG) && ((d12-d19)>=VTSTRONG);
  switch(b.state)
    {
    case IDLE:
      if(tone12)
        {
          b.runStart=localPos;
          b.state=STARTBIT;
        }
      break;
    case STARTBIT:
      if(!tone12)
        {
          b.state=IDLE;
        }
      else if((localPos-b.runStart)>=VTSTARTNEED)
        {
          if(b.leadCount<VTMS(VTLEADNEED))
            {
              b.state=IDLE;                         // no leader before it: a burst of tone, not a start bit
              break;
            }
          // The start bit begins where the strength of 1200 Hz has risen to half way between what it was before and what it reaches: that
          // does not depend on how well the resonator is tuned to the signal or on its level, as the moment it passes a fixed level
          // does. The rise has to last (noise makes dips); if no clear rise is found the level crossing is used.
          double plateau=0,base=0;
          for(uint k=0;k<VTMS(4);k++) plateau+=b.hist[(b.histPos+VTHIST-1-k)%VTHIST];
          plateau/=VTMS(4);
          for(uint k=VTMS(40);k<VTMS(60);k++) base+=b.hist[(b.histPos+VTHIST-1-k)%VTHIST];
          base/=(VTMS(60)-VTMS(40));
          double level=base+0.5*(plateau-base);
          int edge=-1;                                      // samples back from now to where the strength crossed the level
          if(plateau-base>0.1)
            {
              // looking back, the first place where the strength (smoothed over 3 ms, noise makes dips) is under the level
              const int w=(int)VTMS(3);
              for(int k=(int)VTMS(4);k<(int)VTMS(40);k++)
                {
                  double sm=0;
                  for(int j=0;j<w;j++) sm+=b.hist[(b.histPos+VTHIST-1-(k+j))%VTHIST];
                  if(sm/w<level)
                    {
                      edge=k+w/2;
                      break;
                    }
                }
            }
          // The bits are read from where the strength passed the level (it is the same moment in every bank and good enough to
          // find the middle of a 30 ms bit); the first line is timed from the edge when the signal was strong enough to find it
          // within a millisecond, else from the level as well.
          b.onset=b.runStart-VTRISE;
          if(edge>=0 && (plateau-base)>=VTCLEAREDGE)
            {
              b.onsetEmit=localPos-edge-b.t1200.t50;
              b.trimMs=VTDELAYTRIM;
            }
          else
            {
              b.onsetEmit=b.onset;
              b.trimMs=VTDELAYTRIMLEVEL;
            }
          b.plateau=plateau;
          b.bitIndex=0;
          b.bitsWanted=8;
          b.symbol=0;
          b.sumCount=0;
          b.sum11=b.sum13=b.sum19=0;
          b.state=BITS;
        }
      break;
    case BITS:
      {
        // data bit k is in the 30 ms that start 30 ms*(k+1) after the start bit begins; it is judged on the 10 ms around its middle
        uint centre=b.onset+VTMS(VTBITMS)*(b.bitIndex+1)+VTMS(VTBITMS/2);
        if(localPos<centre-VTHALFWINDOW) break;
        if(localPos<=centre+VTHALFWINDOW)
          {
            b.sum11+=d11; b.sum13+=d13; b.sum19+=d19; b.sumCount++;
            break;
          }
        double a11=b.sum11/b.sumCount,a13=b.sum13/b.sumCount,a19=b.sum19/b.sumCount;
        if((a11<a19 && a13<a19) || fabs(a11-a13)<VTMARGIN)
          {
            b.startOver();
            break;
          }
        if(a11>a13) b.symbol|=(1u<<b.bitIndex);
        b.bitIndex++;
        b.sum11=b.sum13=b.sum19=0;
        b.sumCount=0;
        if(b.bitIndex==8 && b.bitsWanted==8 && (b.symbol&0xFF)==VTEXTENDED) b.bitsWanted=16;
        if(b.bitIndex==b.bitsWanted)
          {
            b.stopSum12=b.stopSum19=0;
            b.stopCount=0;
            b.state=STOP;
          }
        break;
      }
    case STOP:
      {
        uint centre=b.onset+VTMS(VTBITMS)*(b.bitsWanted+1)+VTMS(VTBITMS/2);
        if(localPos<centre-VTHALFWINDOW) break;
        if(localPos<=centre+VTHALFWINDOW)
          {
            b.stopSum12+=d12; b.stopSum19+=d19; b.stopCount++;
            break;
          }
        double a12=b.stopSum12/b.stopCount,a19=b.stopSum19/b.stopCount;
        esstvMode mode=NOTVALID;
        if(a12>a19 && a12>VTSTRONG) mode=lookupVIS(b.symbol);
        if(mode!=NOTVALID)
          {
            // the stop bit ends where the first line starts; QSSTV counts the end of a code VTENDTOLINE0 ms after that
            int endOfStop=(int)(b.onsetEmit+VTMS(VTBITMS)*(b.bitsWanted+2));
            int counter=(int)(syncSampleCtr+i)+(endOfStop-(int)localPos)+(int)VTMS(VTENDTOLINE0)+(int)VTMS(b.trimMs);
            if(!candidate.valid)
              {
                candidate.valid=true;
                candidate.mode=(int)mode;
                candidate.counter=counter;
                candidate.plateau=b.plateau;
                candidate.due=localPos+VTMS(VTPICKMS);
              }
            else if(candidate.mode==(int)mode ? b.plateau>candidate.plateau : b.plateau>candidate.plateau*2)
              {
                candidate.mode=(int)mode;
                candidate.counter=counter;
                candidate.plateau=b.plateau;
              }
            b.startOver();
            return true;
          }
        b.startOver();
        break;
      }
    }
  return false;
}

streamDecoder::streamDecoder(bool narrow)
{
  fskCoder.setDataPtr(avgBuffer);
  visCoder.setDataPtr(avgBuffer);
//  retracer.setDataPtr(avgBuffer);
  isNarrow=narrow;
}


void streamDecoder::reset()
{
  avgFreq=0;
  fskCoder.reset();
  visCoder.reset();
  toneVisCoder.reset();
//  retracer.reset();
}

void streamDecoder::process(quint16 *freqPtr , const float *audioPtr, unsigned int syncSampleCtr)
{

  int i;
  for(i=0;i<RXSTRIPE;i++)
    {
      avgFreq=avgFreq*(1-FREQAVG)+FREQAVG*(DSPFLOAT)freqPtr[i];
      avgBuffer[i]=avgFreq;
    }

    fskCoder.extract(syncSampleCtr,isNarrow);
    if(isNarrow) visCoder.extract(syncSampleCtr,true);      // the narrow code is still read from the frequency
    else toneVisCoder.process(audioPtr,syncSampleCtr);
  //  retracer.extract();
}








