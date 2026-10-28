#include "kgstvimage.h"
#include "kgstvtables.h"
#include <QPainter>
#include <array>
#include <cmath>
namespace Kgstv {
namespace {
using Components=std::array<std::array<double,64>,6>;
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
struct Transform {
  double basis[8][8];
  Transform() {for(int k=0;k<8;++k)for(int n=0;n<8;++n)basis[k][n]=(k?1.0:1/std::sqrt(2.0))*std::cos((2*n+1)*k*3.14159265358979323846/16)/2;}
};
const Transform transform;
void append(Bits &bits,int value,int count){for(int i=count-1;i>=0;--i)bits.append(char((value>>i)&1));}
int category(int value){int size=0;for(int n=std::abs(value);n;n>>=1)++size;return size;}
void amplitude(Bits &bits,int value,int size){if(size)append(bits,value<0?-value-(1<<(size-1)):value,size);}
void code(Bits &bits,int symbol,bool chroma,bool dc) {
  const int *codes=dc?(chroma?Tables::dc_chroma_codes:Tables::dc_luma_codes):(chroma?Tables::ac_chroma_codes:Tables::ac_luma_codes);
  const int *lengths=dc?(chroma?Tables::dc_chroma_lengths:Tables::dc_luma_lengths):(chroma?Tables::ac_chroma_lengths:Tables::ac_luma_lengths);
  append(bits,codes[symbol],lengths[symbol]);
}
class Reader {
public:
  const Bits &bits;int position=0;bool good=true;
  explicit Reader(const Bits &data):bits(data){}
  int take(int count) {
    if(count<0 || position+count>bits.size()){good=false;return 0;}
    int value=0;while(count--)value=(value<<1)|bits[position++];return value;
  }
  int symbol(bool chroma,bool dc) {
    const int *codes=dc?(chroma?Tables::dc_chroma_codes:Tables::dc_luma_codes):(chroma?Tables::ac_chroma_codes:Tables::ac_luma_codes);
    const int *lengths=dc?(chroma?Tables::dc_chroma_lengths:Tables::dc_luma_lengths):(chroma?Tables::ac_chroma_lengths:Tables::ac_luma_lengths);
    int value=0;
    for(int length=1;length<=16 && good;++length) {
      value=(value<<1)|take(1);
      if(!good)break;
      for(int i=0;i<(dc?16:256);++i)if(lengths[i]==length && codes[i]==value)return i;
    }
    good=false;return 0;
  }
  int amplitude(int size) {
    if(size==0)return 0;
    if(size<0 || size>11){good=false;return 0;}
    const int value=take(size);return value&(1<<(size-1))?value:-(value+(1<<(size-1)));
  }
};
int pixel(double value){return qBound(0,int(std::lround(value)),255);}
}
bool encodeImageBlock(const QImage &image,int quality,Bits *payload,QString *error) {
  if(error)error->clear();
  if(!payload || image.size()!=QSize(16,16) || quality<0 || quality>15)return fail(error,"Expected a 16x16 KG-STV block and quality index 0..15.");
  Components components={};
  for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
    const QRgb rgb=image.pixel(x,y);// Original scanlines are BGR. Its matrix treats byte 0 as the first
    // component (0x408e80..0x409100), so retain that wire convention.
    const double r=qBlue(rgb),g=qGreen(rgb),b=qRed(rgb);
    const int block=(y/8)*2+x/8,index=(y%8)*8+x%8;
    components[block][index]=0.299*r+0.587*g+0.114*b-128;
    // Reference subsampling takes the top-left pixel of each 2x2 chroma cell.
    if(x%2==0 && y%2==0) {
      components[4][(y/2)*8+x/2]=-0.1684*r-0.3316*g+0.5*b;
      components[5][(y/2)*8+x/2]=0.5*r-0.4187*g-0.0813*b;
    }
  }
  Bits result;
  for(int block=0;block<6;++block) {
    const bool chroma=block>=4;int coefficients[64];
    for(int v=0;v<8;++v)for(int u=0;u<8;++u) {
      double sum=0;
      for(int y=0;y<8;++y)for(int x=0;x<8;++x)sum+=components[block][y*8+x]*transform.basis[u][x]*transform.basis[v][y];
      const int index=v*8+u;const double quant=(chroma?Tables::quant_chroma:Tables::quant_luma)[index]*Tables::quality_scales[quality];
      const int limit=index?1023:2047;
      coefficients[index]=qBound(-limit,int(std::round(sum/quant)),limit);
    }
    const int size=category(coefficients[0]);code(result,size,chroma,true);amplitude(result,coefficients[0],size);
    int run=0;
    for(int position=1;position<64;++position) {
      const int value=coefficients[Tables::zigzag[position]];
      if(value==0){++run;continue;}
      while(run>=16){code(result,0xf0,chroma,false);run-=16;}
      const int size=category(value);code(result,run*16+size,chroma,false);amplitude(result,value,size);run=0;
    }
    if(run)code(result,0,chroma,false);
  }
  if(result.size()>MaximumPayloadBits)return fail(error,"KG-STV image block exceeds the 4095-bit payload limit.");
  *payload=result;return true;
}
bool decodeImageBlock(const Bits &payload,int quality,QImage *image,QString *error) {
  if(error)error->clear();
  if(!image || payload.isEmpty() || payload.size()>MaximumPayloadBits || quality<0 || quality>15)return fail(error,"Invalid KG-STV image block or quality index.");
  for(char bit:payload)if(bit!=0 && bit!=1)return fail(error,"KG-STV image payload is not binary.");
  Reader reader(payload);Components components={};
  for(int block=0;block<6;++block) {
    const bool chroma=block>=4;double coefficients[64]={};
    const int dc=reader.symbol(chroma,true);coefficients[0]=reader.amplitude(dc);
    int position=1;
    while(position<64 && reader.good) {
      const int symbol=reader.symbol(chroma,false);
      if(!reader.good || symbol==0)break;
      if(symbol==0xf0){position+=16;if(position>64)reader.good=false;continue;}
      const int run=symbol>>4,size=symbol&15;
      if(size==0 || size>10 || position+run>=64){reader.good=false;break;}
      position+=run;coefficients[Tables::zigzag[position++]]=reader.amplitude(size);
    }
    if(!reader.good)return fail(error,"Truncated or invalid KG-STV coefficient stream.");
    for(int i=0;i<64;++i)coefficients[i]*=(chroma?Tables::quant_chroma:Tables::quant_luma)[i]*Tables::quality_scales[quality];
    for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
      double value=0;
      for(int v=0;v<8;++v)for(int u=0;u<8;++u)value+=coefficients[v*8+u]*transform.basis[u][x]*transform.basis[v][y];
      components[block][y*8+x]=value;
    }
  }
  if(reader.position!=payload.size())return fail(error,"Trailing KG-STV coefficient data.");
  QImage result(16,16,QImage::Format_RGB32);
  for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
    const double luma=components[(y/8)*2+x/8][(y%8)*8+x%8]+128;
    const double cb=components[4][(y/2)*8+x/2],cr=components[5][(y/2)*8+x/2];
    result.setPixel(x,y,qRgb(pixel(luma+1.7718*cb-0.0012*cr),pixel(luma-0.3441*cb-0.7139*cr),pixel(luma+1.402*cr)));
  }
  *image=result;return true;
}
bool prepareImage(const QImage &image,int quality,bool fec,bool fourLevel,QVector<Packet> *packets,QString *error) {
  if(error)error->clear();
  if(!packets || image.size()!=QSize(320,240) || quality<0 || quality>15)return fail(error,"KG-STV currently requires a 320x240 image and quality index 0..15.");
  QVector<Packet> result;result.reserve(300);
  for(int y=0;y<15;++y)for(int x=0;x<20;++x) {
    Packet packet;packet.header.x=x;packet.header.y=y;packet.header.fec=fec;packet.header.fourLevel=fourLevel;
    bool success=false;
    for(int adjusted=quality;adjusted<=15;++adjusted) {
      if(encodeImageBlock(image.copy(x*16,y*16,16,16),adjusted,&packet.payload)) {
        packet.header.quality=adjusted;packet.header.payloadBits=packet.payload.size();success=true;break;
      }
    }
    if(!success)return fail(error,"KG-STV image block cannot fit in a packet even at maximum quantization.");
    result.append(packet);
  }
  *packets=result;return true;
}
bool paintImageBlock(const Packet &packet,QImage *canvas,QString *error) {
  if(error)error->clear();
  if(!canvas || canvas->size()!=QSize(320,240) || packet.header.x<0 || packet.header.x>=20 || packet.header.y<0 || packet.header.y>=15 ||
     (packet.header.type!=Type::Image && packet.header.type!=Type::Repair) || packet.header.payloadBits!=packet.payload.size())
    return fail(error,"Invalid KG-STV image destination or block coordinate.");
  QImage block;if(!decodeImageBlock(packet.payload,packet.header.quality,&block,error))return false;
  QPainter painter(canvas);if(!painter.isActive())return fail(error,"KG-STV destination image is not paintable.");
  painter.drawImage(packet.header.x*16,packet.header.y*16,block);return true;
}
}
