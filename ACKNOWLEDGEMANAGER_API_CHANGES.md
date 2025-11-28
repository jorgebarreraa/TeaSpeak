# AcknowledgeManager API Changes: master vs 1.4.10

## Overview
The `AcknowledgeManager` class underwent significant API changes between TeaSpeakLibrary-master and TeaSpeakLibrary-1.4.10. The most notable change is the extraction of RTO (Retransmission Timeout) calculation logic into a separate `RtoCalculator` class.

## Critical Breaking Changes

### 1. RTO Calculation Methods - Access Pattern Changed

**TeaSpeakLibrary-master (DIRECT ACCESS):**
```cpp
class AcknowledgeManager {
    [[nodiscard]] inline auto current_rto() const { return this->rto; }
    [[nodiscard]] inline auto current_srtt() const { return this->srtt; }
    [[nodiscard]] inline auto current_rttvar() const { return this->rttvar; }
private:
    float rto{1000};
    float srtt{-1};
    float rttvar{};

    constexpr static auto alpha{.125f};
    constexpr static auto beta{.25f};

    void update_rto(size_t /* response time */);
};
```

**TeaSpeakLibrary-1.4.10 (DELEGATED TO RtoCalculator):**
```cpp
class AcknowledgeManager {
    [[nodiscard]] inline const auto& rto_calculator() const { return this->rto_calculator_; }
private:
    protocol::RtoCalculator rto_calculator_{};
};
```

**Migration Required:**
```cpp
// OLD (master):
auto rttvar = acknowledge_manager.current_rttvar();
auto rto = acknowledge_manager.current_rto();
auto srtt = acknowledge_manager.current_srtt();

// NEW (1.4.10):
auto rttvar = acknowledge_manager.rto_calculator().current_rttvar();
auto rto = acknowledge_manager.rto_calculator().current_rto();
auto srtt = acknowledge_manager.rto_calculator().current_srtt();
```

**Example Fix for VoiceClient.cpp:**

The compilation error from the summary was in VoiceClient.cpp:444:
```cpp
// BROKEN (compiles with master, fails with 1.4.10):
return this->connection->packet_encoder().acknowledge_manager().current_rttvar();

// FIXED (works with 1.4.10):
return this->connection->packet_encoder().acknowledge_manager().rto_calculator().current_rttvar();
```

---

### 2. Entry Structure Changes

**TeaSpeakLibrary-master:**
```cpp
struct Entry {
    uint16_t packet_id{0};
    uint16_t generation_id{0};

    uint8_t packet_type{0xFF};
    uint8_t resend_count{0};
    bool acknowledged : 1;
    uint8_t send_count : 7;

    pipes::buffer buffer;
    std::chrono::system_clock::time_point first_send;
    std::chrono::system_clock::time_point next_resend;
    std::unique_ptr<threads::Future<bool>> acknowledge_listener;
};
```

**TeaSpeakLibrary-1.4.10:**
```cpp
struct Entry {
    uint32_t packet_full_id{0};  // CHANGED: combined ID instead of separate packet_id + generation_id
    uint8_t packet_type{0xFF};

    uint8_t resend_count{0};
    bool acknowledged : 1;
    uint8_t send_count : 7;

    std::chrono::system_clock::time_point first_send;
    std::chrono::system_clock::time_point next_resend;
    std::unique_ptr<std::function<void(bool)>> acknowledge_listener;  // CHANGED: from Future to function

    void* packet_ptr;  // ADDED: opaque packet pointer
};
```

**Key Changes:**
- **Packet ID:** Split `packet_id` + `generation_id` merged into single `packet_full_id`
- **Buffer:** Removed `pipes::buffer`, replaced with opaque `void* packet_ptr`
- **Acknowledge Listener:** Changed from `threads::Future<bool>` to `std::function<void(bool)>`

---

### 3. process_packet() Signature Changed

**TeaSpeakLibrary-master:**
```cpp
void process_packet(ts::protocol::BasicPacket& /* packet */);
```

**TeaSpeakLibrary-1.4.10:**
```cpp
void process_packet(
    uint8_t /* packet type */,
    uint32_t /* full packet id */,
    void* /* packet ptr */,
    std::unique_ptr<std::function<void(bool)>> /* ack listener */
);
```

**Migration Impact:** All callers must be updated to pass individual parameters instead of a packet object.

---

### 4. execute_resend() Return Type and Signature Changed

**TeaSpeakLibrary-master:**
```cpp
ssize_t execute_resend(
    const std::chrono::system_clock::time_point& /* now */,
    std::chrono::system_clock::time_point& /* next resend */,
    std::deque<std::shared_ptr<Entry>>& /* buffers to resend */,
    std::string& /* error */
);
```

**TeaSpeakLibrary-1.4.10:**
```cpp
void execute_resend(
    const std::chrono::system_clock::time_point& /* now */,
    std::chrono::system_clock::time_point& /* next resend */,
    std::deque<std::shared_ptr<Entry>>& /* buffers to resend */
);
```

**Changes:**
- Return type: `ssize_t` → `void`
- Removed `std::string& error` parameter

---

### 5. New Callbacks in 1.4.10

**TeaSpeakLibrary-1.4.10 ONLY:**
```cpp
typedef void(*callback_resend_failed_t)(void* /* user data */, const std::shared_ptr<Entry>& /* entry */);

void(*destroy_packet)(void* /* packet */){nullptr};

void* callback_data{nullptr};
callback_resend_failed_t callback_resend_failed{[](auto, auto){}}; /* must be valid all the time */
```

**Purpose:**
- `destroy_packet`: Callback to properly destroy the opaque packet pointer
- `callback_resend_failed`: Callback when a packet resend fails
- `callback_data`: User data pointer for callbacks

---

### 6. Include Changes

**TeaSpeakLibrary-master:**
```cpp
#include <memory>
#include <protocol/Packet.h>

#define DEBUG_ACKNOWLEDGE
namespace ts::connection {
    class VoiceClientConnection;  // Forward declaration
```

**TeaSpeakLibrary-1.4.10:**
```cpp
#include <memory>
#include <chrono>
#include <functional>
#include <mutex>
#include "./Packet.h"
#include "./RtoCalculator.h"
```

**Changes:**
- Removed `DEBUG_ACKNOWLEDGE` define
- Removed `VoiceClientConnection` forward declaration
- Added `<functional>` for `std::function`
- Added `<chrono>` for time points
- Added `<mutex>` for synchronization
- Added `"./RtoCalculator.h"` for the new RTO calculator
- Include path changed: `<protocol/Packet.h>` → `"./Packet.h"`

---

## New File: RtoCalculator.h (1.4.10 only)

The RTO calculation logic was extracted into a separate class. Here's the interface:

**Location:** `TeaSpeakLibrary-1.4.10/src/protocol/RtoCalculator.h`

```cpp
namespace ts::protocol {
    class RtoCalculator {
    public:
        [[nodiscard]] inline auto current_rto() const { return this->rto; }
        [[nodiscard]] inline auto current_srtt() const { return this->srtt; }
        [[nodiscard]] inline auto current_rttvar() const { return this->rttvar; }

        void update_rto(size_t /* response time */);

    private:
        float rto{1000};
        float srtt{-1};
        float rttvar{};

        constexpr static auto alpha{.125f};
        constexpr static auto beta{.25f};
    };
}
```

---

## Migration Checklist

When migrating code from master to 1.4.10:

### High Priority (Breaks Compilation)
- [ ] Update all `acknowledge_manager.current_rttvar()` calls to `acknowledge_manager.rto_calculator().current_rttvar()`
- [ ] Update all `acknowledge_manager.current_rto()` calls to `acknowledge_manager.rto_calculator().current_rto()`
- [ ] Update all `acknowledge_manager.current_srtt()` calls to `acknowledge_manager.rto_calculator().current_srtt()`
- [ ] Update `process_packet()` calls to new signature
- [ ] Update `execute_resend()` calls (remove error parameter, handle void return)

### Medium Priority (Runtime Behavior)
- [ ] Implement `destroy_packet` callback if using custom packet types
- [ ] Implement `callback_resend_failed` if custom handling needed
- [ ] Update Entry structure access (packet_full_id instead of packet_id/generation_id)
- [ ] Migrate from `threads::Future<bool>` to `std::function<void(bool)>` for acknowledgment listeners

### Low Priority (Code Cleanup)
- [ ] Remove `DEBUG_ACKNOWLEDGE` related code if it exists
- [ ] Update include paths from `<protocol/Packet.h>` to `"./Packet.h"`
- [ ] Add `#include "./RtoCalculator.h"` if directly accessing RTO calculator

---

## Common Compilation Errors and Fixes

### Error 1: 'class ts::connection::AcknowledgeManager' has no member named 'current_rttvar'

```cpp
// Find and fix pattern:
grep -r "acknowledge_manager()\.current_rtt" .

// Replace:
s/acknowledge_manager()\.current_rtt/acknowledge_manager().rto_calculator().current_rtt/g
```

### Error 2: no matching function for call to 'process_packet'

Update from:
```cpp
manager.process_packet(packet);
```

To:
```cpp
manager.process_packet(
    packet.type(),
    packet.full_id(),
    &packet,
    std::make_unique<std::function<void(bool)>>([](bool acked) { /* ... */ })
);
```

### Error 3: void value not ignored as it ought to be (execute_resend)

Update from:
```cpp
ssize_t result = manager.execute_resend(now, next_resend, buffers, error);
```

To:
```cpp
manager.execute_resend(now, next_resend, buffers);
// Handle error differently - check buffer contents or use callback_resend_failed
```

---

## Summary

The refactoring in 1.4.10 improves separation of concerns by:
1. **Extracting RTO calculation** into a dedicated `RtoCalculator` class
2. **Simplifying the Entry structure** with opaque packet pointers
3. **Using callbacks** instead of futures for better performance
4. **Removing error strings** in favor of callback-based error handling

This makes the code more modular but requires updating all client code that accesses RTO metrics.

---

## Related Files

- **Fix Guide:** See `CHANNELCONVERSATIONMODE_FIX.md` for Definitions.h fixes
- **Version Comparison:** See `LIBRARY_VERSION_COMPARISON.md` for other API changes
- **Source Files:**
  - `/home/user/TeaSpeak/TeaSpeakLibrary-master/src/protocol/AcknowledgeManager.h`
  - `/home/user/TeaSpeak/TeaSpeakLibrary-1.4.10/src/protocol/AcknowledgeManager.h`
  - `/home/user/TeaSpeak/TeaSpeakLibrary-1.4.10/src/protocol/RtoCalculator.h`
