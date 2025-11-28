# TeaSpeakLibrary Version Comparison: master vs 1.4.10

## Overview
This document compares the `Definitions.h` file between TeaSpeakLibrary-master and TeaSpeakLibrary-1.4.10, highlighting key differences that may cause compilation issues when migrating or building against different versions.

## Summary of Differences

| Feature | TeaSpeakLibrary-master | TeaSpeakLibrary-1.4.10 | Impact |
|---------|------------------------|------------------------|--------|
| `VirtualServerId` typedef | ✓ Present (line 14) | ✗ Removed | Low - May affect server ID handling |
| `ClientType::UNKNOWN` | ✓ Present (0xFF) | ✗ Removed | Medium - Client type validation may break |
| `ClientState` enum | ✓ Present | → Renamed to `ConnectionState` | High - Direct compilation errors |
| `ChannelConversationMode` enum | ✗ Missing | ✓ Added (lines 110-114) | **Critical** - VirtualServer.cpp fails |
| `ChannelSidebarMode` enum | ✗ Missing | ✓ Added (lines 116-120) | Medium - UI feature support |

## Detailed Differences

### 1. VirtualServerId Typedef (REMOVED in 1.4.10)

**TeaSpeakLibrary-master (line 14):**
```cpp
typedef uint16_t VirtualServerId;
```

**TeaSpeakLibrary-1.4.10:**
Not present - typedef removed

**Migration Impact:** Code using `VirtualServerId` will need to use `ServerId` or `uint16_t` directly.

---

### 2. ClientType Enum Changes

**TeaSpeakLibrary-master (lines 90-101):**
```cpp
enum ClientType {
    CLIENT_TEAMSPEAK,
    CLIENT_QUERY,
    CLIENT_INTERNAL,
    CLIENT_WEB,
    CLIENT_MUSIC,
    CLIENT_TEASPEAK,
    MAX,

    UNKNOWN = 0xFF  // <-- PRESENT
};
```

**TeaSpeakLibrary-1.4.10 (lines 90-98):**
```cpp
enum ClientType {
    CLIENT_TEAMSPEAK,
    CLIENT_QUERY,
    CLIENT_INTERNAL,
    CLIENT_WEB,
    CLIENT_MUSIC,
    CLIENT_TEASPEAK,
    MAX  // <-- NO UNKNOWN VALUE
};
```

**Migration Impact:** Code checking for `ClientType::UNKNOWN` will fail to compile in 1.4.10.

---

### 3. ClientState → ConnectionState (RENAMED in 1.4.10)

**TeaSpeakLibrary-master (lines 103-109):**
```cpp
enum ClientState {
    UNKNWON,

    INITIALIZING,
    CONNECTED,
    DISCONNECTED
};
```

**TeaSpeakLibrary-1.4.10 (lines 100-107):**
```cpp
enum ConnectionState {
    UNKNWON,
    INIT_LOW,    //Web -> WS Handschake
    INIT_HIGH,   //Web -> Auth
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED
};
```

**Key Changes:**
- Enum renamed: `ClientState` → `ConnectionState`
- Removed: `INITIALIZING`
- Added: `INIT_LOW`, `INIT_HIGH`, `DISCONNECTING`
- Transform macro updated: `DEFINE_TRANSFORMS(ts::server::ClientState, uint8_t)` → `DEFINE_TRANSFORMS(ts::server::ConnectionState, uint8_t)`

**Migration Impact:** All references to `ClientState` must be updated to `ConnectionState`. State machine logic may need adjustment.

---

### 4. ChannelConversationMode Enum (ADDED in 1.4.10) ⚠️ CRITICAL

**TeaSpeakLibrary-master:**
Not present - MISSING

**TeaSpeakLibrary-1.4.10 (lines 110-114):**
```cpp
enum ChannelConversationMode : uint8_t {
    CHANNELCONVERSATIONMODE_PUBLIC = 0,
    CHANNELCONVERSATIONMODE_PRIVATE = 1,
    CHANNELCONVERSATIONMODE_NONE = 2
};
```

**Transform Macro Added (line 181):**
```cpp
DEFINE_TRANSFORMS(ts::ChannelConversationMode, uint8_t);
```

**Usage in VirtualServer.cpp (lines 1113-1118):**
```cpp
auto conversation_mode = channel->properties()[property::CHANNEL_CONVERSATION_MODE]
    .as_or<ChannelConversationMode>(ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE);
if(conversation_mode == ChannelConversationMode::CHANNELCONVERSATIONMODE_NONE) {
    /* nothing to do */
    return;
} else {
    conversation_private = conversation_mode == ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE;
```

**Migration Impact:**
- **CRITICAL:** Server code using `ChannelConversationMode` will fail to compile against master
- This enum controls channel conversation privacy modes
- Missing this enum breaks channel conversation features

---

### 5. ChannelSidebarMode Enum (ADDED in 1.4.10)

**TeaSpeakLibrary-master:**
Not present - MISSING

**TeaSpeakLibrary-1.4.10 (lines 116-120):**
```cpp
enum ChannelSidebarMode : uint8_t {
    CHANNELSIDEBARMODE_CONVERSATION = 0,
    CHANNELSIDEBARMODE_DESCRIPTION = 1,
    CHANNELSIDEBARMODE_FILE_TRANSFER = 2
};
```

**Transform Macro Added (line 182):**
```cpp
DEFINE_TRANSFORMS(ts::ChannelSidebarMode, uint8_t);
```

**Migration Impact:** UI code referencing channel sidebar modes will fail to compile against master.

---

## Migration Recommendations

### If building against TeaSpeakLibrary-master but need 1.4.10 features:

1. **Add ChannelConversationMode enum** (see CHANNELCONVERSATIONMODE_FIX.md)
2. **Add ChannelSidebarMode enum** if needed
3. **Rename ConnectionState to ClientState** if code uses the old name
4. **Add VirtualServerId typedef** if code uses it
5. **Add ClientType::UNKNOWN** if code checks for unknown clients

### If migrating from master to 1.4.10:

1. Replace all `ClientState` with `ConnectionState`
2. Update state machine logic for new states (`INIT_LOW`, `INIT_HIGH`, `DISCONNECTING`)
3. Remove references to `VirtualServerId` (use `ServerId` or `uint16_t`)
4. Remove checks for `ClientType::UNKNOWN`
5. Add handling for new channel conversation and sidebar modes

---

## Complete Diff

```diff
--- TeaSpeakLibrary-master/src/Definitions.h
+++ TeaSpeakLibrary-1.4.10/src/Definitions.h
@@ -11,7 +11,6 @@
     typedef int32_t OptionalServerId;
     constexpr auto EmptyServerId = (OptionalServerId) -1;

-    typedef uint16_t VirtualServerId;
     typedef uint64_t ClientDbId;
     typedef uint16_t ClientId;
     typedef std::string ClientUid;
@@ -95,20 +94,31 @@
             CLIENT_WEB,
             CLIENT_MUSIC,
             CLIENT_TEASPEAK,
-            MAX,
-
-            UNKNOWN = 0xFF
+            MAX
         };

-        enum ClientState {
+        enum ConnectionState {
             UNKNWON,
-
-            INITIALIZING,
+            INIT_LOW,    //Web -> WS Handschake
+            INIT_HIGH,   //Web -> Auth
             CONNECTED,
+            DISCONNECTING,
             DISCONNECTED
         };
     }

+    enum ChannelConversationMode : uint8_t {
+        CHANNELCONVERSATIONMODE_PUBLIC = 0,
+        CHANNELCONVERSATIONMODE_PRIVATE = 1,
+        CHANNELCONVERSATIONMODE_NONE = 2
+    };
+
+    enum ChannelSidebarMode : uint8_t {
+        CHANNELSIDEBARMODE_CONVERSATION = 0,
+        CHANNELSIDEBARMODE_DESCRIPTION = 1,
+        CHANNELSIDEBARMODE_FILE_TRANSFER = 2
+    };
+
     enum QueryEventGroup : int {
         QEVENTGROUP_MIN                 = 0,
         QEVENTGROUP_SERVER              = 0,
@@ -162,12 +172,14 @@
     DEFINE_CONVERTER_ENUM(a, b);
     DEFINE_VARIABLE_TRANSFORM_ENUM(a, b);

-DEFINE_TRANSFORMS(ts::server::ClientState, uint8_t);
+DEFINE_TRANSFORMS(ts::server::ConnectionState, uint8_t);
 DEFINE_TRANSFORMS(ts::server::ClientType, uint8_t);
 DEFINE_TRANSFORMS(ts::LicenseType, uint8_t);
 DEFINE_TRANSFORMS(ts::PluginTargetMode, uint8_t);
 DEFINE_TRANSFORMS(ts::ViewReasonId, uint8_t);
 DEFINE_TRANSFORMS(ts::ChatMessageMode, uint8_t);
+DEFINE_TRANSFORMS(ts::ChannelConversationMode, uint8_t);
+DEFINE_TRANSFORMS(ts::ChannelSidebarMode, uint8_t);

 #ifdef WIN32
 #define ts_always_inline __forceinline
```

---

## Related Files

- **Fix Guide:** See `CHANNELCONVERSATIONMODE_FIX.md` for step-by-step instructions to add the missing enum
- **Source Files:**
  - `/home/user/TeaSpeak/TeaSpeakLibrary-master/src/Definitions.h`
  - `/home/user/TeaSpeak/TeaSpeakLibrary-1.4.10/src/Definitions.h`
- **Affected Code:**
  - `VirtualServer.cpp` (lines 1113-1118) - uses ChannelConversationMode
