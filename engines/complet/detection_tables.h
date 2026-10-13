#ifndef ENGINES_COMPLET_DETECTION_TABLES_H
#define ENGINES_COMPLET_DETECTION_TABLES_H

#include "engines/advancedDetector.h"

namespace Complet {

static const ADGameDescription gameDescriptions[] = {
	{
		"mamatuma",
		nullptr,
		AD_ENTRY1s("mpx/startup.mpx", "9de255cdc728753d982abb61ad6cc7e2", 2390),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		GUIO0()
	},
	{
		"yagai",
		nullptr,
		AD_ENTRY1s("mpx/startup.mpx", "eae53720ffed59edbd447a6d16452cd4", 2691),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		GUIO0()
	},
	AD_TABLE_END_MARKER
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_DETECTION_TABLES_H
