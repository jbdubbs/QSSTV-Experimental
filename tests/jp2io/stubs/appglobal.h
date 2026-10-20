#pragma once
// Minimal stand-in for the application globals, so utils/jp2io.cpp builds without the GUI.
#include <QString>
#include "loggingparams.h"
inline void addToLog(const QString &, int) {}
