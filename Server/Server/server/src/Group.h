#pragma once

#include <list>
#include <chrono>
#include <BasicChannel.h>
#include <ThreadPool/Mutex.h>
#include "PermissionManager.h"
#include "Properties.h"
#include "channel/ServerChannel.h"
#include "Definitions.h"
#include "Properties.h"
#include <sql/SqlQuery.h>

namespace ts {
    enum GroupTarget {
        GROUPTARGET_SERVER,
        GROUPTARGET_CHANNEL
    };
}
DEFINE_TRANSFORMS(ts::GroupTarget, uint8_t);