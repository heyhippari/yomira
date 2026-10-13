#ifndef ENGINES_COMPLET_DETECTION_H
#define ENGINES_COMPLET_DETECTION_H

namespace Complet {

enum CompletDebugChannels {
	kDebugScript = 1,
	kDebugGraphics,
	kDebugAudio
};

#define GAMEOPTION_AUTO_MODE      GUIO_GAMEOPTIONS1
#define GAMEOPTION_TEXT_ANIMATION GUIO_GAMEOPTIONS2

} // End of namespace Complet

#endif // ENGINES_COMPLET_DETECTION_H
