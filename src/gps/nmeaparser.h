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

// How the Transmit tab and the %a / %b macros show a coordinate (Configuration > GPS)
enum coordinateFormat
{
  COORDDECIMAL,   //!< signed decimal degrees, 5 decimals: 47.60620 -122.33210
  COORDDEGMIN     //!< APRS / NMEA style degrees and decimal minutes with a hemisphere letter: 47 36.37N 122 19.93W
};

// Coordinate in signed decimal degrees, 5 decimals
QString formatCoordinate(double degrees);

// Coordinate in the given format. COORDDEGMIN is DD MM.mm for a latitude and DDD MM.mm for a longitude (the degrees are
// padded with zeros), N / S / E / W instead of a sign. The total is rounded to hundredths of a minute first, so the
// minutes never show 60.00.
QString formatCoordinate(double degrees,bool latitude,int format);

// Coordinate as a person writes it, to signed decimal degrees. Understands decimal degrees (47.6062, -122.3321), degrees and
// decimal minutes (47 36.37N, 47°36.37') and degrees, minutes and seconds (47 36 22N, 47°36'22"N); the hemisphere letter
// goes before or after the numbers, the marks ° ' " and commas count as blanks. Returns false for anything else: a letter
// that does not belong to the axis, a minus sign together with a letter, minutes or seconds of 60 or more, a value
// beyond 90 (latitude) or 180 (longitude) degrees and free text.
bool parseCoordinateText(const QString &text,bool latitude,double &degrees);

#endif // NMEAPARSER_H
