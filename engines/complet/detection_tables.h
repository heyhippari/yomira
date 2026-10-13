#ifndef ENGINES_COMPLET_DETECTION_TABLES_H
#define ENGINES_COMPLET_DETECTION_TABLES_H

#include "engines/advancedDetector.h"

#include "complet/detection.h"

namespace Complet {

// MP3 music and voices only, and the text is always shown
#define COMPLET_GUIO GUIO7(GUIO_NOMIDI, GUIO_NOSFX, GUIO_NOSUBTITLES, GUIO_NOASPECT, GUIO_NOLANG, GAMEOPTION_AUTO_MODE, GAMEOPTION_TEXT_ANIMATION)

static const ADGameDescription gameDescriptions[] = {
	{
		"mamatuma",
		nullptr,
		AD_ENTRY1s("mpx/startup.mpx", "9de255cdc728753d982abb61ad6cc7e2", 2390),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		COMPLET_GUIO
	},
	{
		"yagai",
		nullptr,
		AD_ENTRY1s("mpx/startup.mpx", "eae53720ffed59edbd447a6d16452cd4", 2691),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		COMPLET_GUIO
	},
	AD_TABLE_END_MARKER
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_DETECTION_TABLES_H
