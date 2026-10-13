#ifndef ENGINES_COMPLET_CONSOLE_H
#define ENGINES_COMPLET_CONSOLE_H

#include "gui/debugger.h"

namespace Complet {

class CompletEngine;

class Console : public GUI::Debugger {
public:
	explicit Console(CompletEngine *vm);

private:
	bool cmdScenario(int argc, const char **argv);
	bool cmdVar(int argc, const char **argv);
	bool cmdFlag(int argc, const char **argv);
	bool cmdUnlockCg(int argc, const char **argv);

	CompletEngine *_vm;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_CONSOLE_H
