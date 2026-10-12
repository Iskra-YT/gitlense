#pragma once

#include "commands/command.h"

namespace gitlense {

// Definicje poszczegolnych komend mieszkaja w plikach src/commands/cmd_*.cpp
// (kazdy plik definiuje jedna). Obiekty sa `extern const`, bo samo `const`
// w namespace daje linkage wewnetrzne i rejestr nie widzialby komend.
extern const Command logCommand;
extern const Command showCommand;
extern const Command fileCommand;
extern const Command branchesCommand;
extern const Command diffCommand;
extern const Command contributorsCommand;
extern const Command languagesCommand;
extern const Command hotspotsCommand;
extern const Command activityCommand;
extern const Command shortlogCommand;
extern const Command searchCommand;
extern const Command graphCommand;

} // namespace gitlense
