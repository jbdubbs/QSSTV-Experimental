#ifndef APPDEFS_H
#define APPDEFS_H
#include <stdint.h>
#include <complex>
#include <string>
#include <vector>
#include <set>
#include <ostream>
#include <limits>
#include <sstream>
// Only pull in the specific names actually used unqualified throughout this codebase
// (bare complex<...>/string/vector, mostly in the vendored DRM/DABMOT code) rather than
// the whole std namespace: `using namespace std;` here used to make std::byte visible
// unqualified too, which is an enum class since C++17 and collides with MinGW's Windows
// headers' own unscoped `typedef unsigned char byte;` (rpcndr.h, pulled in by Qt's
// Windows-only COM headers) -- "reference to 'byte' is ambiguous", Windows-only since
// those headers only exist on that platform. Nothing in this codebase wants std::byte
// unqualified, so it's simply never added to this list.
using std::complex;
using std::string;
using std::vector;
using std::set;
using std::ostream;
using std::numeric_limits;
using std::stringstream;

#define SOUNDFRAME  quint32

#define BASESAMPLERATE 48000
#define SUBSAMPLINGFACTOR 4
#define MONOCHANNEL 1
#define STEREOCHANNEL 2
#define RXSTRIPE 1024
#define TXSTRIPE 1024
#define FILTERPARAMTYPE double
#define DOWNSAMPLESIZE (SUBSAMPLINGFACTOR*RXSTRIPE)
#define SAMPLERATE (BASESAMPLERATE/SUBSAMPLINGFACTOR)


#undef DISABLENARROW

typedef double DSPFLOAT;


/* Define the application specific data-types ------------------------------- */
typedef	double							_REAL;
typedef	complex<_REAL>			_COMPLEX;
typedef short						  	_SAMPLE;
typedef unsigned char				_BYTE;
typedef bool							  _BOOLEAN;
typedef unsigned char 			_BINARY;

#endif // APPDEFS_H
