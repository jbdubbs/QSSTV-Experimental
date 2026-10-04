#include "linefit.h"
#include "ntppacket.h"
#include <cstdio>
#include <cmath>
#include <random>

static int failures=0;
#define CHECK(c,...) do{ if(!(c)){printf("FAIL: " __VA_ARGS__);printf("\n");failures++;} else {printf("ok:   " __VA_ARGS__);printf("\n");} }while(0)

int main()
{
  // packet round trip: a server 1.234567 s ahead, 20 ms each way, 1 ms processing
  {
    unsigned char req[NTPPACKETSIZE];
    uint64_t origin=0x123456789abcdef0ULL;
    ntpBuildRequest(req,origin);
    unsigned char rep[NTPPACKETSIZE]={};
    rep[0]=(0<<6)|(4<<3)|4;rep[1]=1;
    memcpy(rep+24,req+40,8);
    double T0=1000.0;                                  // local
    double serverNow=1.8e9;                            // unix seconds at T0
    double t1=serverNow+1.234567+0.020;
    double t2=t1+0.001;
    auto put=[&](unsigned char *p,double unixs){double s=unixs+NTPUNIXOFFSET;uint32_t sec=uint32_t(s);uint32_t fr=uint32_t((s-std::floor(s))*4294967296.0);ntpPut32(p,sec);ntpPut32(p+4,fr);};
    put(rep+32,t1);put(rep+40,t2);
    double T3=T0+0.041;
    ntpReply r=ntpParseReply(rep,NTPPACKETSIZE);
    CHECK(r.ok && r.origin==origin,"reply parsed, origin echoed");
    double theta,delay;
    ntpOffsetDelay(T0,T3,r,theta,delay);
    CHECK(std::fabs(theta-(serverNow-T0+1.234567))<2e-6,"offset %.7f",theta-(serverNow-T0));
    CHECK(std::fabs(delay-0.040)<2e-6,"delay %.6f",delay);
    rep[0]|=(3<<6);
    CHECK(!ntpParseReply(rep,NTPPACKETSIZE).ok,"leap indicator 3 rejected");
    rep[0]&=~(3<<6);rep[1]=0;
    CHECK(!ntpParseReply(rep,NTPPACKETSIZE).ok,"stratum 0 (kiss-o'-death) rejected");
  }
  // 2036 rollover: raw seconds 0x00000001 are in era 1
  CHECK(ntpToUnix(uint64_t(1)<<32)>2.08e9,"era 1 timestamp");

  // the full chain: soundcard +37.5 ppm, local clock -12 ppm from UTC, 3 servers with offsets and jitter
  {
    std::mt19937 rng(7);
    std::normal_distribution<double> jitter(0.0,0.002),audioNoise(0.0,0.001);
    const double nominal=48000.0,cardPpm=37.5,localPpm=-12.0;   // local clock runs 12 ppm slow: UTC gains on it
    // true card rate (frames per UTC second); frames per local second = card rate / (1+local) ... see below
    double trueRate=nominal*(1.0+cardPpm*1e-6);
    double utcPerLocal=1.0-localPpm*1e-6*(-1.0);              // local slow by 12 ppm: UTC advances 1+12e-6 per local second
    utcPerLocal=1.0+12e-6;
    double bias[3]={0.003,-0.011,0.0007};
    for(double minutes:{5.0,30.0})
      {
        std::vector<linePoint> ap,np;
        for(double w=5;w<minutes*60;w+=10)
          {
            double x=w;                                        // local time
            double frames=trueRate*x*utcPerLocal+audioNoise(rng)*nominal;
            frames+=std::fabs(audioNoise(rng))*nominal;        // late arrival only
            ap.push_back({x,frames,0});
          }
        int k=0;
        for(double x=3;x<minutes*60;x+=21,k++)
          {
            int g=k%3;
            double theta=(utcPerLocal-1.0)*x+bias[g]+jitter(rng);
            if(k%17==5) theta+=0.080;                          // a congested sample
            np.push_back({x,theta,g});
          }
        lineFitResult fa=fitLine(ap,3.0,0.002*nominal),fn=fitLine(np,3.0,0.001);
        double rateLocal=fa.slope;
        double ppm=(rateLocal/(1.0+fn.slope)/nominal-1.0)*1e6;
        double ea=fa.slopeError/rateLocal*1e6,en=fn.slopeError*1e6;
        double err=std::sqrt(ea*ea+en*en);
        printf("      %.0f min: %.2f ppm (true %.1f), error estimate %.2f ppm, %d/%zu ntp inliers\n",minutes,ppm,cardPpm,err,fn.inliers,np.size());
        CHECK(fa.valid && fn.valid,"fits valid, %.0f min",minutes);
        CHECK(std::fabs(ppm-cardPpm)<std::max(3.0*err,0.5),"within 3 sigma, %.0f min",minutes);
        if(minutes==5.0) CHECK(err<10.0,"5 minutes gets under 10 ppm error (%.2f)",err);
        if(minutes==30.0) CHECK(std::fabs(ppm-cardPpm)<2.0,"30 minutes within 2 ppm (%.2f)",ppm-cardPpm);
      }
  }
  printf(failures? "%d FAILURES\n":"all passed\n",failures);
  return failures!=0;
}
