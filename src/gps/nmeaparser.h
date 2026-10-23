#ifndef NMEAPARSER_H
#define NMEAPARSER_H

#include <QByteArray>
#include <QString>

// Position from one NMEA 0183 sentence, as signed decimal degrees (north / east positive).
// Understands the GGA, RMC and GLL sentences of any talker (GP, GN, GL, GA, BD, ...).
// Returns false for every other sentence, for a bad checksum, for a sentence that reports
// no fix (GGA quality 0, RMC/GLL status V) and for coordinates out of range.
// The sentence may or may not carry the leading '$', the *hh checksum and the line ending.
bool nmeaParseFix(const QByteArray &sentence,double &lat,double &lon);

// Coordinate as the Transmit tab and the %a / %b macros show it: signed decimal degrees, 5 decimals
QString formatCoordinate(double degrees);

#endif // NMEAPARSER_H
