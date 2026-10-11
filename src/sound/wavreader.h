/***************************************************************************
 *   QSSTV-Experimental: WAV reader for "decode from file"                  *
 *                                                                         *
 *   Streams any common WAV file as mono 16 bit samples at 48 kHz, the     *
 *   format the receiver works in. Unlike wavIO (which only accepts a      *
 *   canonical 48 kHz 16 bit header and shows dialogs on errors) it walks  *
 *   the RIFF chunks (LIST, fact, ... are skipped), accepts 8/16/24/32 bit *
 *   PCM and 32/64 bit float, any channel count (the first channel is      *
 *   used) and any sample rate (resampled with a Kaiser windowed sinc),    *
 *   and reports problems as text instead of dialogs.                      *
 ***************************************************************************/
#ifndef WAVREADER_H
#define WAVREADER_H

#include <QBuffer>
#include <QFile>
#include <QString>
#include <deque>
#include <vector>

class wavReader
{
public:
  static const int outputRate=48000;

  wavReader();
  ~wavReader();

  //! open `path`; on failure returns false and sets `error` to a one line explanation
  bool open(const QString &path,QString &error);
  void close();
  bool isOpen() const { return dev && dev->isOpen(); }

  //! read up to `count` mono samples at outputRate; returns the number produced, 0 at the end of the file
  int read(qint16 *dst,unsigned int count);

  //! length of the recording in seconds
  double durationSeconds() const;
  //! how much of the file has been consumed, 0..100
  int progressPercent() const;
  //! e.g. "44100 Hz, 16 bit PCM, 2 channels (resampled to 48000 Hz)"
  QString describe() const;

  //! check a file without keeping it open
  static bool probe(const QString &path,QString &error,double *seconds=nullptr,QString *description=nullptr);

private:
  bool parseHeader(QString &error);
  bool openCompressed(const QString &path,QString &error);
  bool openVorbis(QString &error);
  void startDecoded(const QByteArray &pcm,int pcmRate,const QString &description);
  float sampleAt(const char *p) const;
  void readInput(unsigned int frames);
  int readNative(qint16 *dst,unsigned int count);
  int readResampled(qint16 *dst,unsigned int count);
  void buildKernel();

  QFile file;
  QBuffer decoded;          //!< mono 16 bit PCM at outputRate for non-WAV files (mp3, flac, ...)
  QIODevice *dev;           //!< file or decoded, whichever is being read
  bool compressed;
  QString sourceDescription;
  int formatTag;            //!< 1 = PCM, 3 = IEEE float
  int channels;
  int bits;
  int rate;
  unsigned int frameBytes;
  qint64 dataStart;
  qint64 dataFrames;        //!< frames in the data chunk
  qint64 framesRead;        //!< frames consumed from the file so far
  bool inputDone;

  // resampler (only used when rate != outputRate)
  bool resampling;
  double ratio;             //!< input frames per output frame
  double cutoff;            //!< low pass cutoff relative to the input Nyquist frequency
  int halfWidth;            //!< kernel half width in input frames
  std::vector<float> kernel;
  std::deque<float> hist;
  qint64 histBase;          //!< input frame index of hist[0]
  double nextT;             //!< input position of the next output frame
  QByteArray raw;
};

#endif // WAVREADER_H
