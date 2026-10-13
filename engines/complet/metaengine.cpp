#include "base/plugins.h"
#include "complet/complet.h"
#include "engines/advancedDetector.h"

class CompletMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	Common::Error createInstance(OSystem *system, Engine **engine, const ADGameDescription *) const override {
		*engine = new Complet::CompletEngine(system);
		return Common::Error(Common::kNoError);
	}

	const char *getName() const override {
		return "complet";
	}
};

#if PLUGIN_ENABLED_DYNAMIC(COMPLET)
REGISTER_PLUGIN_DYNAMIC(COMPLET, PLUGIN_TYPE_ENGINE, CompletMetaEngine);
#else
REGISTER_PLUGIN_STATIC(COMPLET, PLUGIN_TYPE_ENGINE, CompletMetaEngine);
#endif
