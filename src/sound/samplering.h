#ifndef SAMPLERING_H
#define SAMPLERING_H

// A lock-free ring of 16-bit samples for exactly one producer thread and one consumer thread. The Android sound backend
// (soundandroid.cpp) puts one between the microphone callback and the sound thread, and one between the sound thread and
// the speaker callback, so the audio threads never wait for a lock held by the application. No Qt: tests/samplering
// builds it on the desktop.

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

class sampleRing
{
public:
  //! The ring holds 2^powerOfTwo samples
  explicit sampleRing(unsigned powerOfTwo) : buf(size_t(1)<<powerOfTwo), mask((size_t(1)<<powerOfTwo)-1), head(0), tail(0) {}

  size_t capacity() const {return buf.size();}
  size_t readable() const {return head.load(std::memory_order_acquire)-tail.load(std::memory_order_acquire);}
  size_t writable() const {return buf.size()-readable();}

  //! Producer: store as many of the samples as fit, return how many
  size_t write(const int16_t *data,size_t count)
  {
    const size_t h=head.load(std::memory_order_relaxed);
    const size_t room=buf.size()-(h-tail.load(std::memory_order_acquire));
    const size_t n=std::min(count,room);
    for(size_t i=0;i<n;i++) buf[(h+i)&mask]=data[i];
    head.store(h+n,std::memory_order_release);
    return n;
  }

  //! Consumer: take as many as there are, up to count, return how many
  size_t read(int16_t *data,size_t count)
  {
    const size_t t=tail.load(std::memory_order_relaxed);
    const size_t have=head.load(std::memory_order_acquire)-t;
    const size_t n=std::min(count,have);
    for(size_t i=0;i<n;i++) data[i]=buf[(t+i)&mask];
    tail.store(t+n,std::memory_order_release);
    return n;
  }

  //! Consumer: drop everything that is in it
  void discardAll() {tail.store(head.load(std::memory_order_acquire),std::memory_order_release);}

  //! Empty it. Only while neither side is running.
  void reset() {head.store(0);tail.store(0);}

private:
  std::vector<int16_t> buf;
  size_t mask;
  std::atomic<size_t> head;   // written by the producer; counts up for ever, only the difference to tail matters
  std::atomic<size_t> tail;   // written by the consumer
};

#endif // SAMPLERING_H
