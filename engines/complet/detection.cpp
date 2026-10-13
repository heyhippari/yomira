#include "base/plugins.h"
#include "common/debug-channels.h"
#include "engines/advancedDetector.h"
#include "complet/detection_tables.h"

static const DebugChannelDef debugFlagList[] = {
	{Complet::kDebugScript, "script", "Script commands"},
	{Complet::kDebugGraphics, "graphics", "Graphics loading"},
	{Complet::kDebugAudio, "audio", "Music and voices"},
	DEBUG_CHANNEL_END
};

static const PlainGameDescriptor completGames[] = {
	{"mamatuma", "Mama! Tsuma?"},
	{"yagai", "Yagai Gakushuu"},
	{nullptr, nullptr}
};

class CompletMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	CompletMetaEngineDetection() : AdvancedMetaEngineDetection(Complet::gameDescriptions, completGames) {
		_flags = kADFlagMatchFullPaths;
	}

	const char *getName() const override {
		return "complet";
	}

	const char *getEngineName() const override {
		return "C4 Engine";
	}

	const char *getOriginalCopyright() const override {
		return "Complet's";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

REGISTER_PLUGIN_STATIC(COMPLET_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, CompletMetaEngineDetection);
