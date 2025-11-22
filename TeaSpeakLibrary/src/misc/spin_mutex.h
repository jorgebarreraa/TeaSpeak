#pragma once

// Legacy compatibility header
// spin_mutex was renamed to spin_lock
#include "spin_lock.h"

using spin_mutex = spin_lock;
