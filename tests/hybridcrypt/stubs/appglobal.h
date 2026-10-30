#pragma once
// Minimal stand-in for the application globals, so utils/hybridcrypt.cpp builds without the GUI.
#include <QString>
#include "loggingparams.h"
inline void addToLog(const QString &, int) {}
