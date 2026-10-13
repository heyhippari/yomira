#include "base/plugins.h"
#include "engines/advancedDetector.h"
#include "complet/detection_tables.h"

static const PlainGameDescriptor completGames[] = {
	{"mamatuma", "Mama! Tsuma?"},
	{nullptr, nullptr}
};

class CompletMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	CompletMetaEngineDetection() : AdvancedMetaEngineDetection(Complet::gameDescriptions, completGames) {
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
};

REGISTER_PLUGIN_STATIC(COMPLET_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, CompletMetaEngineDetection);
