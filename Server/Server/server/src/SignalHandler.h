#pragma once

#include <string>
#include "Configuration.h"

namespace ts {
    namespace syssignal {
        extern bool setup();
        extern bool setup_threads();
        extern void handleStopSignal(int);
        extern void handleAbortSignal(int);
        extern void handleTerminate();
    }
}