/***************************************************************************
 *   The wait cursor while something takes a moment.                        *
 *                                                                          *
 *   On macOS Qt makes the wait cursor from a bitmap (QImage::toCGImage),   *
 *   and on some installs (macOS 26 on an unsupported Intel Mac, #195) that *
 *   crashes inside CoreGraphics. macOS shows its own beach ball when a     *
 *   program stalls, so there the wait cursor is simply not used.           *
 *                                                                          *
 *   Depends on QtGui only, so it can be tested on its own                  *
 *   (tests/busycursor).                                                    *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef BUSYCURSOR_H
#define BUSYCURSOR_H

#include <QGuiApplication>
#include <QCursor>

// QSSTV_NO_WAIT_CURSOR: the macOS behaviour on any platform (for the test)
#if defined(Q_OS_MACOS) || defined(QSSTV_NO_WAIT_CURSOR)
#define BUSYCURSOR_AVAILABLE 0
#else
#define BUSYCURSOR_AVAILABLE 1
#endif

//! A wait cursor for as long as the object lives (or until release()). It restores only what it set itself, so an object that
//! did not set one (wanted=false, or macOS) or has been released never takes away somebody else's override cursor.
class busyCursor
{
public:
  //! wanted=false: no cursor change, e.g. while the program is still starting up and there is nothing to wait for
  explicit busyCursor(bool wanted=true)
  {
#if BUSYCURSOR_AVAILABLE
    if(wanted && QGuiApplication::instance()!=nullptr)
      {
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);
        active=true;
        setCount++;
      }
#else
    Q_UNUSED(wanted);
#endif
  }
  ~busyCursor() {release();}
  busyCursor(const busyCursor &)=delete;
  busyCursor &operator=(const busyCursor &)=delete;

  void release()
  {
#if BUSYCURSOR_AVAILABLE
    if(active)
      {
        active=false;
        QGuiApplication::restoreOverrideCursor();
      }
#endif
  }

  //! how many wait cursors were set since the program started (for tests)
  static int timesSet() {return setCount;}

private:
  bool active=false;
  static inline int setCount=0;
};

#endif // BUSYCURSOR_H
