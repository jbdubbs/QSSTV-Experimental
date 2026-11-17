#include "applog.h"
#include "sstvtx.h"
#include <algorithm>
#include "appglobal.h"
#include "configparams.h"
#include "soundbase.h"
#include "cw.h"
#include "dispatcher.h"
#include "synthes.h"
#include "modes/modes.h"
#include "txfunctions.h"



sstvTx::sstvTx()
{
  currentMode=0;
  oldMode=NOTVALID;
}
sstvTx::~sstvTx()
{
  if(currentMode) delete currentMode;
}

void sstvTx::init()
{
  sampleCounter=0;
}




void sstvTx::sendPreamble()
{
  addToLog("txFunc:sendPreamble",LOGTXFUNC);

  synthesPtr->sendTone(0.1,1900.,0,true);
  synthesPtr->sendTone(0.1,1500.,0,true);
  synthesPtr->sendTone(0.1,1900.,0,true);
  synthesPtr->sendTone(0.1,1500.,0,true);
  synthesPtr->sendTone(0.1,2300.,0,true);
  synthesPtr->sendTone(0.1,1500.,0,true);
  synthesPtr->sendTone(0.1,2300.,0,true);
  synthesPtr->sendTone(0.1,1500.,0,true);
  synthesPtr->sendTone(0.3,1900.,0,true);
  synthesPtr->sendTone(0.01,1200.,0,true);
  synthesPtr->sendTone(0.3,1900.,0,true);
}



void sstvTx::sendVIS()
{
  int i,l;
  int t=txSSTVParam.VISCode;
  addToLog("txFunc:sendVis",LOGTXFUNC);
  if(currentMode->isNarrow())
  {
    l=24;
    synthesPtr->sendTone(0.300,1900,0,true);
    synthesPtr->sendTone(0.100,2100,0,true);
    synthesPtr->sendTone(0.022,1900,0,true); // startbit
    for (i=0;i<l;i++)
    {
      if((t&1)==1) synthesPtr->sendTone(0.022,1900,0,true);
      else synthesPtr->sendTone(0.022,2100,0,true);
      t>>=1;
    }
  }
  else
  {
    if ((t&0xFF)==0x23) l=16;
    else l=8;
    synthesPtr->sendTone(0.030,1200,0,true); // startbit
    for (i=0;i<l;i++)
    {
      if((t&1)==1) synthesPtr->sendTone(0.030,1100,0,true);
      else synthesPtr->sendTone(0.030,1300,0,true);
      t>>=1;
    }
    synthesPtr->sendTone(0.030,1200,0,true); // stopbit
  }
}



/*!
  The digital header of the AVT modes (the SSTV Handbook, 4.2.5, and MMSSTV's SendHeader agree): 32 words, each a 1900 Hz pulse and 16 bits of
  9.7646 ms (2048/20 Bd), 1 is 2200 Hz and 0 is 1600 Hz. A word is 8 bits, 3 of the mode and a 5 bit count that runs from 0 to 31, and the
  same bits inverted. The first line follows 0.305 ms after the last word.
*/
void sstvTx::sendAvtHeader()
{
  const double bit=0.0097646;
  int modeBits=(txSSTVParam.mode==AVT24)?2:((txSSTVParam.mode==AVT94)?3:5);  // 010 AVT 24, 011 AVT 94, 101 AVT 90
  for(int count=0;count<32;count++)
  {
    unsigned int word=(((modeBits<<5)|count)<<8)|(((modeBits<<5)|count)^0xFF);
    synthesPtr->sendTone(bit,1900,0,true);
    for(int i=0;i<16;i++)
    {
      synthesPtr->sendTone(bit,(word&0x8000)?2200:1600,0,true);
      word=(word<<1)&0xFFFF;
    }
  }
  synthesPtr->sendTone(0.00030514375,1500,0,true);
}

bool sstvTx::create(esstvMode m,DSPFLOAT clock)
{
  if((oldMode==m) && (currentMode!=NULL))
  {
    currentMode->init(clock);
    return true;
  }
  oldMode=m;
  if(currentMode) delete currentMode;
  currentMode=0;
  switch (m)
  {
  case M1:
  case M2:
    currentMode=new modeGBR(m,TXSTRIPE,true,false);
    break;
  case S1:
  case S2:
  case SDX:
    currentMode=new modeGBR2(m,TXSTRIPE,true,false);
    break;
  case R36:
    currentMode=new modeRobot1(m,TXSTRIPE,true,false);
    break;
  case R24:
  case R72:
  case MR73:
  case MR90:
  case MR115:
  case MR140:
  case MR175:
  case ML180:
  case ML240:
  case ML280:
  case ML320:
    currentMode=new modeRobot2(m,TXSTRIPE,true,false);
    break;
  case SC2_60:
  case SC2_120:
  case SC2_180:
  case P3:
  case P5:
  case P7:
    currentMode=new modeRGB(m,TXSTRIPE,true,false);
    break;
  case MC110N:
  case MC140N:
  case MC180N:
    currentMode=new modeRGB(m,TXSTRIPE,true,true);   // narrow: 1900 Hz sync, 2044-2300 Hz video, 24 bit VIS code (#145)
    break;
  case FAX480:
  case BW8:
  case BW12:
    currentMode=new modeBW(m,TXSTRIPE,true,false);
    break;
  case AVT24:
  case AVT90:
  case AVT94:
    currentMode=new modeAVT(m,TXSTRIPE,true,false);
    break;
  case PD50:
  case PD90:
  case PD120:
  case PD120W:
  case PD160:
  case PD180:
  case PD240:
  case PD290:
  case MP73:
  case MP115:
  case MP140:
  case MP175:
    currentMode=new modePD(m,TXSTRIPE,true,false);
    break;

  case JB60:
    currentMode=new modeJB60(m,TXSTRIPE,true,false);
    break;
  case MP73N:
  case MP110N:
  case MP140N:
    currentMode=new modePD(m,TXSTRIPE,true,true);
    break;
  default:
    m=NOTVALID;
    break;
  }
  if (m!=NOTVALID)
  {
    initializeSSTVParametersIndex(m,true);
    QString s=getSSTVModeNameLong(m);
    addToLog("create: create TX mode",LOGTXFUNC);
    currentMode->init(clock);
    return true;
  }
  return false;
}

double sstvTx::FSKIDTime()
{
  double tim;
  double charTime=0.022*6;
  tim=(myCallsign.size()+3)*charTime;
  tim+=0.422;
  return tim;
}

double sstvTx::calcTxTime(int overheadTime)
{
  double tim=0;
  //  tim= soundIOPtr->getPlaybackStartupTime();
  tim+=SILENCEDELAY;
  initializeSSTVParametersIndex(sstvModeIndexTx,true);
  int t=txSSTVParam.VISCode;
  tim+=1.41; //preamble;
  if((txSSTVParam.mode>=STARTNARROW) && (txSSTVParam.mode<=ENDNARROW)) tim+=0.3+0.1+25*0.022; // leader, break, start bit and 24 bits (sendVIS)
  else if ((t&0xFF)==0x23) tim+=18.*0.03;
  else tim+=10.*0.03;
  if(txSSTVParam.mode==FAX480) tim+=1220*2*0.002048;     // the start tone
  if(txSSTVParam.mode>=AVT24 && txSSTVParam.mode<=AVT94) tim+=32*17*0.0097646+0.0003; // the digital header
  tim+=txSSTVParam.imageTime;

//  if(enableCW)
  if(useCW)
  {
    tim+=0.5 ;//CW silence gap
    initCW(cwText);
    tim+=getCWDuration();
    tim+=0.3; // trailer;
  }
  else
  {
    tim+=FSKIDTime();
  }
  if(useCW || myCallsign.isEmpty())
  {
    double lt=(txSSTVParam.numberOfDataLines>0)?txSSTVParam.imageTime/txSSTVParam.numberOfDataLines:0.1;
    tim+=useVOX?std::min(lt,0.5):std::min(lt,0.5)+0.4; // postamble
  }
  tim+=overheadTime;
  return tim;
}

bool sstvTx::sendImage(imageViewer *ivPtr)
{
  modeBase::eModeBase mb;
  if(useVOX) synthesPtr->sendTone(1.,1700.,0,false);
  if(txSSTVParam.mode==FAX480)
  {
    // The VIS code (85, added to the mode later) comes first: a program that does not know it hears a burst of tones and waits for the start tone.
    sendPreamble();
    sendVIS();
    // the start tone (the SSTV Handbook, 4.3.1): 244 Hz, 1500 and 2300 Hz for 4 units of 4 MHz/2048 (2.048 ms) each, 1220 times
    // (sendTone keeps the fraction of a sample from tone to tone)
    for (int i=0;i<1220;i++)
    {
      synthesPtr->sendTone(0.002048,1500,0,true);
      synthesPtr->sendTone(0.002048,2300,0,true);
    }
  }
  else
  {
    sendPreamble();
    sendVIS();
    if(txSSTVParam.mode>=AVT24 && txSSTVParam.mode<=AVT94) sendAvtHeader();
  }
  addToLog("txFunc: sendImage",LOGTXFUNC);
  APPLOG("qsstv-tx","preamble and VIS sent, the picture follows");
  mb=currentMode->transmitImage(ivPtr);
  APPLOG("qsstv-tx","transmitImage returned %d",int(mb));
  if(mb==modeBase::MBABORTED) return false;
  else  return true;
}

void sstvTx::applyTemplate(QString templateFilename,bool useTemplate,imageViewer *ivPtr)
{
  create(sstvModeIndexTx,txClock);

  ivPtr->setParam(templateFilename,useTemplate, txSSTVParam.numberOfPixels,txSSTVParam.numberOfDisplayLines);
}

void sstvTx::abort()
{
  if(currentMode)
    {
      currentMode->abort();
    }
}


bool sstvTx::aborted()
{
  if(currentMode) return currentMode->aborted();
  else return true;

}

void sstvTx::createTestPattern(imageViewer *ivPtr,etpSelect sel)
{
  int nb=txSSTVParam.numberOfPixels;
  int nl=txSSTVParam.numberOfDisplayLines;
  QString fn;
  switch (sel)
  {
  case TPPM5544: fn=":/icons/testpatterns/pm5544.png"; break;
  case TPINDIANHEAD: fn=":/icons/testpatterns/indianhead.png"; break;
  default: fn=":/icons/testpatterns/smptebars.png"; break;
  }
  QImage src(fn);
  if(src.isNull()) return;
  if(transmissionModeIndex==TRXDRM)
    {
      // DRM has no fixed image size: load the pattern as an ordinary source image so the
      // compression / size slider path (applyTemplate) has a source to work from
      ivPtr->openImage(src.convertToFormat(QImage::Format_ARGB32_Premultiplied));
      emit ivPtr->imageChanged();
      return;
    }
  if(nb<=0 || nl<=0) return;
  // load it as an ordinary source image, like a gallery image: the caller then runs
  // applyTemplate(), which scales it to the mode (a no-op at native size), and later
  // mode/size changes re-render it from the original pixels
  ivPtr->clear();
  ivPtr->openImage(src.convertToFormat(QImage::Format_ARGB32_Premultiplied));
}
