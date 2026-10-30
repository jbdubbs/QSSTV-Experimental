// Direct check for utils/hybridcrypt.cpp (issue #155): the FTP login of a Hybrid transmission is encrypted by enCrypt() and read
// back by deCrypt(). reverseString() wrote into the indexes of an empty QString, which Qt 5 allowed and Qt 6 does not: the program
// crashed as soon as a picture was loaded with Hybrid on.
#include "hybridcrypt.h"
#include <QByteArray>
#include <cstdio>

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
