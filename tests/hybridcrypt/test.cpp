// Direct check for utils/hybridcrypt.cpp (issue #155): the FTP login of a Hybrid transmission is encrypted by enCrypt() and read
// back by deCrypt(). reverseString() wrote into the indexes of an empty QString, which Qt 5 allowed and Qt 6 does not: the program
// crashed as soon as a picture was loaded with Hybrid on.
#include "hybridcrypt.h"
#include <QByteArray>
#include <cstdio>
#include <vector>

QString hybridFtpRemoteHost, hybridFtpRemoteDirectory, hybridFtpLogin, hybridFtpPassword;

static int fails = 0;
static void check(bool ok, const char *what) { printf("  %s   %s\n", ok ? "ok " : "FAIL", what); if (!ok) fails++; }

// The ciphertexts of the original algorithm (Qt 5 reversed the string with the same loop), made once with an independent port of
// enCrypt() to Python. A round trip alone cannot catch a wrong reversal: both sides would be wrong in the same way, and the other
// QSSTV versions could no longer read the transmission.
static const char *GOLDEN_LOGIN =
  "0373016002410146023d014102670147022c0143038f016402480147024b0143023a0145023d0141023101320241014603810162023a0145024b0143024101"
  "46024f01480226012c02590145023a014503a20174038b015f024b0143038d0171022c014303940172022f013f0264014b024b0143038b015f022c01430248"
  "014702520144039601650d0a";
static const char *GOLDEN_EMPTY = "03730160038f0164038101620226012c03960165fefffeff0d0a";

int main()
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  printf("== a login\n");
  hybridFtpRemoteHost = "ftp.example.org";
  hybridFtpLogin = "user";
  hybridFtpPassword = "s3cret";
  hybridFtpRemoteDirectory = "pics";
  QByteArray ba;
  {
    hybridCrypt tx;
    check(tx.enCrypt(&ba), "enCrypt succeeds");
  }
  check(ba.size() % 8 == 2 && ba.endsWith("\r\n"), "8 bytes per 4 characters, then CR LF");
  check(ba.toHex() == GOLDEN_LOGIN, "the ciphertext is the one of the original algorithm");
  {
    hybridCrypt rx;
    check(rx.deCrypt(&ba), "deCrypt succeeds");
    check(rx.host() == "ftp.example.org" && rx.user() == "user" && rx.passwd() == "s3cret" && rx.dir() == "pics" && rx.port() == 21,
          "and gives the login back");
  }

  // Issue #156: deCrypt() read the payload in groups of 8 bytes and only checked that the size was even, so a payload of another
  // size than 8 * n + 2 made the last group read past the end. Every length goes through an exact-size heap buffer wrapped with
  // fromRawData() (no copy, no terminating zero), so AddressSanitizer reports any read at an index >= the length.
  printf("== payloads of other sizes (issue #156)\n");
  {
    const QByteArray good = ba;
    const int goodSize = good.size();
    int wrongRet = 0, wrongLogin = 0;
    for (int n = 0; n <= goodSize + 16; n++)
      {
        std::vector<char> v(n);
        for (int i = 0; i < n; i++) v[i] = i < goodSize ? good.at(i) : char(0x55);
        QByteArray in = QByteArray::fromRawData(v.data(), n);
        hybridCrypt rx;
        bool ret = rx.deCrypt(&in);
        bool expect = (n - 2) % 2 == 0 && n - 2 >= 20;     // the size rules that were already there
        if (ret != expect) wrongRet++;
        // A truncated payload may give a wrong login, but the real one (n == goodSize) and the real one followed by an even number
        // of pad bytes (0, 2, 4, 6) must give it back: ignoring the part of a group that is not complete costs those nothing.
        if (n >= goodSize && n <= goodSize + 6 && (n - goodSize) % 2 == 0)
          {
            if (!(rx.host() == "ftp.example.org" && rx.user() == "user" && rx.passwd() == "s3cret" && rx.dir() == "pics")) wrongLogin++;
          }
      }
    check(wrongRet == 0, "every length from 0 to the payload + 16 bytes: the size rules still decide true / false");
    check(wrongLogin == 0, "the payload followed by 0, 2, 4 or 6 pad bytes still gives the login back");
  }

  printf("== no Hybrid settings (issue #155)\n");
  hybridFtpRemoteHost.clear();
  hybridFtpLogin.clear();
  hybridFtpPassword.clear();
  hybridFtpRemoteDirectory.clear();
  QByteArray empty;
  {
    hybridCrypt tx;
    check(tx.enCrypt(&empty), "enCrypt succeeds with nothing to encrypt");
  }
  check(empty.toHex() == GOLDEN_EMPTY, "the ciphertext is the one of the original algorithm");
  {
    hybridCrypt rx;
    check(rx.deCrypt(&empty), "deCrypt succeeds");
    check(rx.host().isEmpty() && rx.user().isEmpty() && rx.passwd().isEmpty() && rx.dir().isEmpty(), "and the login is empty");
  }

  printf("\n%s\n", fails ? "FAILED" : "all passed");
  return fails ? 1 : 0;
}
