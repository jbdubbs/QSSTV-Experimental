/***************************************************************************
 *   mmsstv-linux-port: WAV reader for "decode from file"                  *
 *   See wavreader.h for what this is and why.                             *
 ***************************************************************************/
#include "wavreader.h"

#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QEventLoop>
#include <QFileInfo>
#include <QTimer>
#include <QUrl>
#include <QtEndian>
#ifdef QSSTV_HAVE_VORBISFILE
#include <vorbis/vorbisfile.h>
#endif
#include <algorithm>
#include <cmath>

namespace
{
  const unsigned int kChunkFrames=4096;
  const int kZeroCrossings=16;      // sinc zero crossings each side of the centre
  const int kTableRes=256;          // kernel table entries per input frame
  const double kKaiserBeta=8.0;
  const double kCutoffMargin=0.97;  // keep the transition band inside the output Nyquist frequency

  double besselI0(double x)
  {
    double sum=1,term=1;
    for(int k=1;k<60;k++)
      {
        double h=x/(2*k);
        term*=h*h;
        sum+=term;
        if(term<1e-14*sum) break;
      }
    return sum;
  }

  inline qint16 toInt16(double v)
  {
    long i=lround(v);
    return (qint16)(i<-32768 ? -32768 : (i>32767 ? 32767 : i));
  }

#ifdef QSSTV_HAVE_VORBISFILE
  size_t vorbisRead(void *ptr,size_t size,size_t nmemb,void *ds)
  {
    qint64 n=((QFile*)ds)->read((char*)ptr,(qint64)(size*nmemb));
    return n<0 ? 0 : (size_t)n/size;
  }
  int vorbisSeek(void *ds,ogg_int64_t offset,int whence)
  {
    QFile *f=(QFile*)ds;
    qint64 pos=offset;
    if(whence==SEEK_CUR) pos+=f->pos();
    else if(whence==SEEK_END) pos+=f->size();
    return f->seek(pos) ? 0 : -1;
  }
  long vorbisTell(void *ds) { return (long)((QFile*)ds)->pos(); }
#endif

  const char *kHint="convert it with: ffmpeg -i input -ar 48000 -ac 1 -c:a pcm_s16le output.wav";
}

wavReader::wavReader()
{
  dev=nullptr;
  compressed=false;
  formatTag=0;
  channels=0;
  bits=0;
  rate=0;
  frameBytes=0;
  dataStart=0;
  dataFrames=0;
  framesRead=0;
  inputDone=true;
  resampling=false;
  ratio=1;
  cutoff=1;
  halfWidth=0;
  histBase=0;
  nextT=0;
}

wavReader::~wavReader()
{
}

void wavReader::close()
{
  file.close();
  decoded.close();
  decoded.setData(QByteArray());
  dev=nullptr;
  compressed=false;
  sourceDescription.clear();
  hist.clear();
  histBase=0;
  nextT=0;
  framesRead=0;
  inputDone=true;
}

/*!
  Walk the RIFF chunks up to the start of the data chunk.
*/
bool wavReader::parseHeader(QString &error)
{
  char id[4];
  quint32 size;
  bool haveFmt=false;
  qint64 fileSize=file.size();

  if(file.read(id,4)!=4 || memcmp(id,"RIFF",4)!=0)
    {
      error="not a WAV file (no RIFF header)";
      return false;
    }
  file.read(4);   // RIFF size, unreliable for streamed files
  if(file.read(id,4)!=4 || memcmp(id,"WAVE",4)!=0)
    {
      error="not a WAV file (RIFF but not WAVE)";
      return false;
    }
  while(true)
    {
      if(file.read(id,4)!=4 || file.read((char*)&size,4)!=4)
        {
          error=haveFmt ? "WAV file has no data chunk" : "WAV file has no fmt chunk";
          return false;
        }
      size=qFromLittleEndian<quint32>(size);
      qint64 chunkStart=file.pos();
      if(memcmp(id,"fmt ",4)==0)
        {
          if(size<16)
            {
              error="WAV fmt chunk is too short";
              return false;
            }
          QByteArray f=file.read(std::min<quint32>(size,40));
          if(f.size()<16)
            {
              error="WAV fmt chunk is truncated";
              return false;
            }
          const uchar *p=(const uchar*)f.constData();
          formatTag=qFromLittleEndian<quint16>(p);
          channels=qFromLittleEndian<quint16>(p+2);
          rate=(int)qFromLittleEndian<quint32>(p+4);
          bits=qFromLittleEndian<quint16>(p+14);
          if(formatTag==0xFFFE && f.size()>=26)
            {
              formatTag=qFromLittleEndian<quint16>(p+24);   // first two bytes of the sub-format GUID
            }
          haveFmt=true;
        }
      else if(memcmp(id,"data",4)==0)
        {
          if(!haveFmt)
            {
              error="WAV data chunk comes before the fmt chunk";
              return false;
            }
          dataStart=chunkStart;
          qint64 avail=fileSize-dataStart;
          qint64 bytes=size;
          if(size==0 || size==0xFFFFFFFFu || bytes>avail) bytes=avail;   // streamed or truncated file: use what is there
          if(formatTag!=1 && formatTag!=3)
            {
              error=QString("unsupported WAV format tag %1 (compressed audio?); %2").arg(formatTag).arg(kHint);
              return false;
            }
          if(channels<1 || channels>8)
            {
              error=QString("unsupported channel count %1; %2").arg(channels).arg(kHint);
              return false;
            }
          if(!((formatTag==1 && (bits==8||bits==16||bits==24||bits==32)) || (formatTag==3 && (bits==32||bits==64))))
            {
              error=QString("unsupported sample format (%1 bit %2); %3").arg(bits).arg(formatTag==3 ? "float" : "PCM").arg(kHint);
              return false;
            }
          if(rate<4000 || rate>768000)
            {
              error=QString("unsupported sample rate %1 Hz; %2").arg(rate).arg(kHint);
              return false;
            }
          frameBytes=channels*(bits/8);
          dataFrames=bytes/frameBytes;
          if(dataFrames==0)
            {
              error="WAV file contains no audio";
              return false;
            }
          return true;
        }
      // skip this chunk (chunks are padded to an even size)
      if(!file.seek(chunkStart+size+(size&1)))
        {
          error="WAV file is truncated";
          return false;
        }
      if(file.pos()>=fileSize)
        {
          error=haveFmt ? "WAV file has no data chunk" : "WAV file has no fmt chunk";
          return false;
        }
    }
}

void wavReader::buildKernel()
{
  cutoff=std::min(1.0,(double)outputRate/rate)*kCutoffMargin;
  halfWidth=(int)ceil(kZeroCrossings/cutoff);
  const int n=halfWidth*kTableRes+2;
  kernel.assign(n,0.f);
  const double norm=besselI0(kKaiserBeta);
  for(int i=0;i<n;i++)
    {
      double x=(double)i/kTableRes;
      double r=x/halfWidth;
      double w=(r>=1.0) ? 0.0 : besselI0(kKaiserBeta*sqrt(1.0-r*r))/norm;
      double y=cutoff*x;
      double s=(y==0) ? 1.0 : sin(M_PI*y)/(M_PI*y);
      kernel[i]=(float)(cutoff*s*w);
    }
}

bool wavReader::open(const QString &path,QString &error)
{
  close();
  file.setFileName(path);
  if(!file.open(QIODevice::ReadOnly))
    {
      error=QString("cannot open file: %1").arg(file.errorString());
      return false;
    }
  char magic[4];
  bool riff=(file.read(magic,4)==4 && memcmp(magic,"RIFF",4)==0);
  bool ogg=(memcmp(magic,"OggS",4)==0);
  file.seek(0);
  if(!riff)
    {
#ifdef QSSTV_HAVE_VORBISFILE
      // Windows Media Foundation has no Vorbis decoder, so decode Ogg Vorbis ourselves
      if(ogg)
        {
          QString vorbisError;
          if(openVorbis(vorbisError)) return true;
          file.seek(0);   // not Vorbis (e.g. Opus in Ogg): let Qt Multimedia try
        }
#else
      Q_UNUSED(ogg);
#endif
      file.close();
      if(!openCompressed(path,error))
        {
          close();
          return false;
        }
      return true;
    }
  if(!parseHeader(error))
    {
      close();
      return false;
    }
  dev=&file;
  file.seek(dataStart);
  framesRead=0;
  inputDone=false;
  hist.clear();
  histBase=0;
  nextT=0;
  resampling=(rate!=outputRate);
  ratio=(double)rate/outputRate;
  if(resampling) buildKernel();
  return true;
}

/*!
  Decode a non-WAV file (mp3, flac, ogg, aac, ...) with Qt Multimedia into
  mono 16 bit PCM at outputRate, held in memory, then read it like a WAV.
*/
bool wavReader::openCompressed(const QString &path,QString &error)
{
  QAudioDecoder decoder;
  QAudioFormat fmt;
  fmt.setSampleRate(outputRate);
  fmt.setChannelCount(1);
  fmt.setSampleFormat(QAudioFormat::Int16);
  decoder.setAudioFormat(fmt);
  decoder.setSource(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));

  QByteArray pcm;
  QString failure;
  bool ok=true;
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&decoder,&QAudioDecoder::bufferReady,&loop,[&]()
    {
      QAudioBuffer b=decoder.read();
      if(b.isValid() && b.format().sampleFormat()==QAudioFormat::Int16 && b.format().channelCount()==1)
        pcm.append((const char*)b.constData<qint16>(),b.byteCount());
      timeout.start(10000);   // restart the stall guard on every buffer
    });
  QObject::connect(&decoder,&QAudioDecoder::finished,&loop,&QEventLoop::quit);
  QObject::connect(&decoder,QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),&loop,[&](QAudioDecoder::Error)
    {
      ok=false;
      failure=decoder.errorString();
      loop.quit();
    });
  QObject::connect(&timeout,&QTimer::timeout,&loop,[&]()
    {
      ok=false;
      failure="timed out decoding";
      loop.quit();
    });
  timeout.start(10000);
  decoder.start();
  loop.exec();
  decoder.stop();
  if(!ok || pcm.size()<2)
    {
      error=QString("unsupported or undecodable audio file%1; %2")
              .arg(failure.isEmpty() ? QString() : " ("+failure+")").arg(kHint);
      return false;
    }
  startDecoded(pcm,outputRate,QString("%1, decoded to %2 Hz mono").arg(QFileInfo(path).suffix().toLower()).arg(outputRate));
  return true;
}

//! hold mono 16 bit PCM at pcmRate in memory and read it like a WAV file (resampled when pcmRate != outputRate)
void wavReader::startDecoded(const QByteArray &pcm,int pcmRate,const QString &description)
{
  decoded.setData(pcm);
  decoded.open(QIODevice::ReadOnly);
  dev=&decoded;
  compressed=true;
  formatTag=1;
  channels=1;
  bits=16;
  rate=pcmRate;
  frameBytes=2;
  dataStart=0;
  dataFrames=pcm.size()/2;
  framesRead=0;
  inputDone=false;
  hist.clear();
  histBase=0;
  nextT=0;
  resampling=(rate!=outputRate);
  ratio=(double)rate/outputRate;
  if(resampling) buildKernel();
  sourceDescription=description;
  if(resampling) sourceDescription+=QString(" (resampled to %1 Hz)").arg(outputRate);
}

#ifdef QSSTV_HAVE_VORBISFILE
//! decode the (already open) file as Ogg Vorbis: first channel only, at the file's own rate
bool wavReader::openVorbis(QString &error)
{
  OggVorbis_File vf;
  ov_callbacks cb={vorbisRead,vorbisSeek,nullptr,vorbisTell};
  if(ov_open_callbacks(&file,&vf,nullptr,0,cb)!=0)
    {
      error="not an Ogg Vorbis file";
      return false;
    }
  vorbis_info *vi=ov_info(&vf,-1);
  if(!vi || vi->channels<1 || vi->rate<4000 || vi->rate>768000)
    {
      ov_clear(&vf);
      error="unsupported Ogg Vorbis stream";
      return false;
    }
  const int ch=vi->channels;
  const int pcmRate=(int)vi->rate;
  QByteArray pcm;
  char buf[8192];
  int section=0;
  while(true)
    {
      long n=ov_read(&vf,buf,sizeof(buf),0,2,1,&section);
      if(n==OV_HOLE) continue;
      if(n<=0) break;
      const qint16 *s=(const qint16*)buf;
      for(long i=0;i+ch<=n/2;i+=ch) pcm.append((const char*)&s[i],2);
    }
  ov_clear(&vf);
  if(pcm.size()<2)
    {
      error="Ogg Vorbis file contains no audio";
      return false;
    }
  startDecoded(pcm,pcmRate,QString("Ogg Vorbis, %1 Hz, %2 channel%3").arg(pcmRate).arg(ch).arg(ch==1 ? "" : "s"));
  return true;
}
#else
bool wavReader::openVorbis(QString &error)
{
  error="Ogg Vorbis decoder not built in";
  return false;
}
#endif

double wavReader::durationSeconds() const
{
  return rate ? (double)dataFrames/rate : 0.0;
}

int wavReader::progressPercent() const
{
  if(dataFrames<=0) return 0;
  return (int)std::min<qint64>(100,(framesRead*100)/dataFrames);
}

QString wavReader::describe() const
{
  if(compressed) return sourceDescription;
  QString s=QString("%1 Hz, %2 bit %3, %4 channel%5").arg(rate).arg(bits).arg(formatTag==3 ? "float" : "PCM").arg(channels).arg(channels==1 ? "" : "s");
  if(resampling) s+=QString(" (resampled to %1 Hz)").arg(outputRate);
  return s;
}

bool wavReader::probe(const QString &path,QString &error,double *seconds,QString *description)
{
  wavReader r;
  if(!r.open(path,error)) return false;
  if(seconds) *seconds=r.durationSeconds();
  if(description) *description=r.describe();
  return true;
}

//! first channel of the frame starting at p, in 16 bit sample units
float wavReader::sampleAt(const char *p) const
{
  const uchar *u=(const uchar*)p;
  switch(formatTag)
    {
    case 1:
      switch(bits)
        {
        case 8:  return ((int)u[0]-128)*256.f;
        case 16: return (float)(qint16)qFromLittleEndian<quint16>(u);
        case 24:
          {
            qint32 v=(qint32)(((quint32)u[0]<<8)|((quint32)u[1]<<16)|((quint32)u[2]<<24));   // sign extends through the top byte
            return v/65536.f;
          }
        default: return (float)((qint32)qFromLittleEndian<quint32>(u)/65536.0);
        }
    default:   // IEEE float
      if(bits==32)
        {
          quint32 v=qFromLittleEndian<quint32>(u);
          float f;
          memcpy(&f,&v,4);
          return f*32767.f;
        }
      else
        {
          quint64 v=qFromLittleEndian<quint64>(u);
          double d;
          memcpy(&d,&v,8);
          return (float)(d*32767.0);
        }
    }
}

//! append the first channel of up to `frames` more input frames to hist
void wavReader::readInput(unsigned int frames)
{
  qint64 want=std::min<qint64>(frames,dataFrames-framesRead);
  if(want<=0)
    {
      inputDone=true;
      return;
    }
  raw.resize((int)(want*frameBytes));
  qint64 got=dev->read(raw.data(),raw.size())/frameBytes;
  const char *p=raw.constData();
  for(qint64 i=0;i<got;i++,p+=frameBytes) hist.push_back(sampleAt(p));
  framesRead+=got;
  if(got<want || framesRead>=dataFrames) inputDone=true;   // short read: the file is shorter than its header said
}

int wavReader::readNative(qint16 *dst,unsigned int count)
{
  qint64 want=std::min<qint64>(count,dataFrames-framesRead);
  if(want<=0) return 0;
  raw.resize((int)(want*frameBytes));
  qint64 got=dev->read(raw.data(),raw.size())/frameBytes;
  const char *p=raw.constData();
  for(qint64 i=0;i<got;i++,p+=frameBytes) dst[i]=toInt16(sampleAt(p));
  framesRead+=got;
  if(got<want) framesRead=dataFrames;
  return (int)got;
}

int wavReader::readResampled(qint16 *dst,unsigned int count)
{
  unsigned int produced=0;
  while(produced<count)
    {
      qint64 center=(qint64)floor(nextT);
      if(center>=dataFrames) break;
      while(!inputDone && histBase+(qint64)hist.size()<=center+halfWidth) readInput(kChunkFrames);
      double acc=0;
      for(qint64 idx=center-halfWidth+1;idx<=center+halfWidth;idx++)
        {
          if(idx<0 || idx>=histBase+(qint64)hist.size()) continue;
          double pos=fabs(idx-nextT)*kTableRes;
          int i=(int)pos;
          double f=pos-i;
          acc+=hist[idx-histBase]*(kernel[i]*(1-f)+kernel[i+1]*f);
        }
      dst[produced++]=toInt16(acc);
      nextT+=ratio;
      qint64 keepFrom=(qint64)floor(nextT)-halfWidth;
      while(histBase<keepFrom && !hist.empty())
        {
          hist.pop_front();
          histBase++;
        }
      if(hist.empty() && histBase<keepFrom) histBase=keepFrom;
    }
  return (int)produced;
}

int wavReader::read(qint16 *dst,unsigned int count)
{
  if(!dev || !dev->isOpen()) return 0;
  return resampling ? readResampled(dst,count) : readNative(dst,count);
}
