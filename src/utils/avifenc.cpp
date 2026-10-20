#include "avifenc.h"
#include <QThread>

#define AVIFMINQUALITY 1
#define AVIFMAXQUALITY 99   // 100 is lossless

// Measured with SSIMULACRA2 on 13 photos at 320x256 and 640x480: 4:4:4 beats 4:2:0 by 1-4 points of size saving at equal score,
// and speed 5 is within ~1 point of speed 4 but about 25% quicker. libaom's tune=ssim made no difference.
// The quality search below runs ~7 encodes and the GUI waits for it: speed 5 takes ~0.5 s per encode on a 640x496 test card
// (16 cores) but ~0.2 s at speed 6, so big pictures drop one step. libaom's cpu-used: 0 slowest .. 10 fastest.
#define AVIFSPEED 5
#define AVIFSPEEDBIG 6
#define AVIFBIGPIXELS 200000
#define AVIFCHROMA 444

bool avifSniff(const QByteArray &data)
{
  // ISO BMFF: 4 byte size, 'ftyp', major brand
  return data.size()>=12 && data.mid(4,4)=="ftyp" && (data.mid(8,4)=="avif" || data.mid(8,4)=="avis");
}

#ifdef HAVE_LIBAVIF
#include <avif/avif.h>

bool avifWriteSupported()
{
  return avifCodecName(AVIF_CODEC_CHOICE_AOM,AVIF_CODEC_FLAG_CAN_ENCODE)!=nullptr;
}

QByteArray avifEncodeQuality(const QImage &image,int quality,int speed,int chroma)
{
  QByteArray out;
  QImage rgba=image.convertToFormat(QImage::Format_RGBA8888);
  if(rgba.isNull()) return out;
  avifImage *im=avifImageCreate(rgba.width(),rgba.height(),8,chroma==420 ? AVIF_PIXEL_FORMAT_YUV420 : AVIF_PIXEL_FORMAT_YUV444);
  avifEncoder *enc=avifEncoderCreate();
  avifRWData raw=AVIF_DATA_EMPTY;
  if(!im || !enc) goto done;
  // tag the colour space, or other decoders have to guess
  im->colorPrimaries=AVIF_COLOR_PRIMARIES_BT709;
  im->transferCharacteristics=AVIF_TRANSFER_CHARACTERISTICS_SRGB;
  im->matrixCoefficients=AVIF_MATRIX_COEFFICIENTS_BT601;
  im->yuvRange=AVIF_RANGE_FULL;
  {
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb,im);
    rgb.format=AVIF_RGB_FORMAT_RGBA;
    rgb.depth=8;
    rgb.pixels=rgba.bits();
    rgb.rowBytes=rgba.bytesPerLine();
    if(avifImageRGBToYUV(im,&rgb)!=AVIF_RESULT_OK) goto done;
  }
  enc->codecChoice=AVIF_CODEC_CHOICE_AOM;
  enc->maxThreads=qMax(1,QThread::idealThreadCount());
  enc->speed=speed;
  enc->quality=quality;
  enc->qualityAlpha=AVIF_QUALITY_LOSSLESS;
  if(avifEncoderWrite(enc,im,&raw)==AVIF_RESULT_OK) out=QByteArray(reinterpret_cast<const char *>(raw.data),static_cast<int>(raw.size));
done:
  avifRWDataFree(&raw);
  if(enc) avifEncoderDestroy(enc);
  if(im) avifImageDestroy(im);
  return out;
}

QImage avifDecode(const QByteArray &data,bool *transformed)
{
  QImage out;
  if(transformed) *transformed=false;
  avifDecoder *dec=avifDecoderCreate();
  if(!dec) return out;
  dec->maxThreads=qMax(1,QThread::idealThreadCount());
  if(avifDecoderSetIOMemory(dec,reinterpret_cast<const uint8_t *>(data.constData()),static_cast<size_t>(data.size()))==AVIF_RESULT_OK
     && avifDecoderParse(dec)==AVIF_RESULT_OK && avifDecoderNextImage(dec)==AVIF_RESULT_OK)
    {
      if(transformed && (dec->image->transformFlags&(AVIF_TRANSFORM_IROT|AVIF_TRANSFORM_IMIR))) *transformed=true;
      QImage rgba(dec->image->width,dec->image->height,QImage::Format_RGBA8888);
      avifRGBImage rgb;
      avifRGBImageSetDefaults(&rgb,dec->image);
      rgb.format=AVIF_RGB_FORMAT_RGBA;
      rgb.depth=8;
      rgb.pixels=rgba.bits();
      rgb.rowBytes=rgba.bytesPerLine();
      if(avifImageYUVToRGB(dec->image,&rgb)==AVIF_RESULT_OK) out=rgba.convertToFormat(QImage::Format_RGB32);
    }
  avifDecoderDestroy(dec);
  return out;
}
#else
bool avifWriteSupported() { return false; }
QByteArray avifEncodeQuality(const QImage &,int,int,int) { return QByteArray(); }
QImage avifDecode(const QByteArray &,bool *transformed) { if(transformed) *transformed=false; return QImage(); }
#endif

QByteArray avifEncode(const QImage &image,int targetBytes,QImage *decoded,int *qualityUsed)
{
  QByteArray best;
  const int speed=(image.width()*image.height()>AVIFBIGPIXELS) ? AVIFSPEEDBIG : AVIFSPEED;
  int bestQuality=0;
  int lo=AVIFMINQUALITY,hi=AVIFMAXQUALITY;
  // size is not strictly monotonic in quality, so keep the best fit
  while(lo<=hi)
    {
      int mid=(lo+hi)/2;
      QByteArray d=avifEncodeQuality(image,mid,speed,AVIFCHROMA);
      if(d.isEmpty()) return QByteArray();
      if(d.size()<=targetBytes)
        {
          best=d;
          bestQuality=mid;
          lo=mid+1;
        }
      else hi=mid-1;
    }
  if(best.isEmpty())
    {
      best=avifEncodeQuality(image,AVIFMINQUALITY,speed,AVIFCHROMA);
      bestQuality=AVIFMINQUALITY;
      if(best.isEmpty()) return QByteArray();
    }
  if(decoded) *decoded=avifDecode(best);
  if(qualityUsed) *qualityUsed=bestQuality;
  return best;
}
