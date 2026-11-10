// The lock-free sample ring of the Android sound backend (src/sound/samplering.h), on the desktop: its single-thread rules,
// and a producer and a consumer thread hammering a small ring with random block sizes, so it wraps and fills up all the time.
// Every sample must arrive once, in order.
#include "samplering.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <thread>

static int fails=0;
#define CHECK(cond,what) do{ if(cond) printf("  ok   %s\n",what); else {printf("  FAIL %s\n",what);fails++;} }while(0)

int main()
{
  {
    sampleRing r(4);   // 16 samples
    int16_t in[40],out[40];
    for(int i=0;i<40;i++) in[i]=int16_t(i+1);
    CHECK(r.capacity()==16 && r.readable()==0 && r.writable()==16,"a new ring is empty");
    CHECK(r.write(in,10)==10 && r.readable()==10 && r.writable()==6,"ten samples go in");
    CHECK(r.write(in+10,10)==6 && r.writable()==0,"only what fits goes in");
    CHECK(r.read(out,4)==4 && out[0]==1 && out[3]==4,"four come out, in order");
    CHECK(r.write(in+16,8)==4 && r.readable()==16,"the freed room is used, across the end of the buffer");
    CHECK(r.read(out,40)==16,"everything comes out");
    r.write(in,12);
    r.discardAll();
    CHECK(r.readable()==0 && r.writable()==16,"discardAll empties it");
    r.write(in,5);
    r.reset();
    CHECK(r.readable()==0,"reset empties it");
  }
  {
    // a read wants more than there is, a write more than fits: both return what happened, never block
    sampleRing r(3);
    int16_t buf[100]={0};
    CHECK(r.read(buf,100)==0,"reading an empty ring returns 0");
    CHECK(r.write(buf,100)==8,"writing into an empty ring takes the whole capacity");
    CHECK(r.write(buf,1)==0,"a full ring takes nothing");
  }
  {
    const size_t total=40000000;
    sampleRing r(12);   // 4096 samples, so it is full or empty most of the time
    std::thread producer([&]
      {
        std::mt19937 g(1);
        std::uniform_int_distribution<int> sz(1,700);
        int16_t block[700];
        size_t sent=0;
        while(sent<total)
          {
            size_t n=std::min<size_t>(sz(g),total-sent);
            for(size_t i=0;i<n;i++) block[i]=int16_t((sent+i)&0x7FFF);
            size_t done=0;
            while(done<n)
              {
                size_t w=r.write(block+done,n-done);
                if(w==0) std::this_thread::yield();
                done+=w;
              }
            sent+=n;
          }
      });
    size_t got=0,bad=0;
    std::mt19937 g(2);
    std::uniform_int_distribution<int> sz(1,900);
    int16_t block[900];
    while(got<total)
      {
        size_t n=r.read(block,sz(g));
        if(n==0) {std::this_thread::yield();continue;}
        for(size_t i=0;i<n;i++) if(block[i]!=int16_t((got+i)&0x7FFF)) bad++;
        got+=n;
      }
    producer.join();
    printf("  %zu samples through a 4096 sample ring, %zu out of order\n",got,bad);
    CHECK(bad==0 && r.readable()==0,"two threads, 40 million samples, every one once and in order");
  }
  printf("\n%s\n",fails ? "FAILED" : "all checks passed");
  return fails ? 1 : 0;
}
