#ifndef ENGINES_COMPLET_DETECTION_TABLES_H
#define ENGINES_COMPLET_DETECTION_TABLES_H

#include "engines/advancedDetector.h"

namespace Complet {

static const ADGameDescription gameDescriptions[] = {
	{
		"mamatuma",
		nullptr,
		AD_ENTRY1s("c4.exe", "c83854796613f14e90797d66bf6abc8e", 229376),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		GUIO0()
	},
	AD_TABLE_END_MARKER
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_DETECTION_TABLES_H
