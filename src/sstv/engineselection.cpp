/***************************************************************************
 *   mmsstv-linux-port: Step 6 -- per-mode engine selection                *
 *   See engineselection.h for what this is and why.                      *
 ***************************************************************************/
#include "engineselection.h"

#include <QSettings>
#include <QString>

bool mmsstvCoreSupports(esstvMode mode)
{
	switch (mode) {
	case M1:
		return true;
	default:
		return false;
	}
}

eEngine selectedEngine(esstvMode mode)
{
	if (!mmsstvCoreSupports(mode)) {
		return ENGINE_QSSTV; // no alternative implementation exists
	}
	QSettings qSettings;
	qSettings.beginGroup("Engine");
	int stored = qSettings.value(getSSTVModeNameShort(mode), int(ENGINE_MMSSTV_CORE)).toInt();
	qSettings.endGroup();
	return (stored == int(ENGINE_QSSTV)) ? ENGINE_QSSTV : ENGINE_MMSSTV_CORE;
}

void setSelectedEngine(esstvMode mode, eEngine engine)
{
	QSettings qSettings;
	qSettings.beginGroup("Engine");
	qSettings.setValue(getSSTVModeNameShort(mode), int(engine));
	qSettings.endGroup();
}
