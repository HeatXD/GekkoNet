# GekkoNet Developer Integration Guide

## Table of Contents

1. [Core Concepts](#core-concepts)
2. [Building & Linking](#building--linking)
3. [Quick Start](#quick-start)
4. [API Reference](#api-reference)
5. [Session Types](#session-types)
6. [Custom Networking](#custom-networking)
7. [Configuration Guide](#configuration-guide)
8. [Network Stats & Frame Pacing](#network-stats--frame-pacing)
9. [Tips](#tips)

---

## Core Concepts

### Rollback Networking

In traditional online games inputs are delayed until confirmed by all peers, this adds latency. Rollback networking removes that delay by executing local inputs immediately and using **predictions** for remote players. When the real remote input arrives and it differs from the prediction, the game **rolls back** to the mispredicted frame, applies the correct input and **re-simulates** forward to the present frame. This gives you offline-feeling responsiveness at the cost of occasional visual corrections.

Your responsibilities as the integrator:
1. **Save** your gamestate when asked (so GekkoNet can roll back to it later).
2. **Load** a previous gamestate when asked (this is the rollback).
3. **Advance** your simulation one frame with provided inputs (deterministically).

GekkoNet is event-driven, you poll for events each frame and respond to them in a switch statement.

### Actors

Each participant is an **actor** added via `gekko_add_actor`:

- `GekkoLocalPlayer` input provided by you each frame.
- `GekkoRemotePlayer` input arrives over the network.
- `GekkoSpectator` remote observer receiving game data.

Players must be added **in the same order on all peers**. Spectators are not order restricted but should be added after `gekko_start` and before the game loop. Spectators only need to add their host as a remote player.

### The Three Events You Must Handle

| Event | What to do |
|---|---|
| `GekkoSaveEvent` | Copy your gamestate into the provided buffer. Set `*state_len` and optionally `*checksum`. |
| `GekkoLoadEvent` | Restore your gamestate from the provided buffer. This is a rollback. |
| `GekkoAdvanceEvent` | Advance your simulation one frame using the provided inputs. Check `rolling_back` to skip rendering/audio during re-simulation. |

### Determinism

Your simulation **must be deterministic**, same state + same inputs = same result, byte-for-byte. This means:

- Zero-initialize all state. No uninitialized memory.
- Avoid `float`/`double` for cross-platform play (use fixed-point).
- If using RNG, include the seed in your saved state.
- No external state (system time, file I/O) during advance.

---

## Building & Linking

See the [README](README.md) for full build instructions. Key points for integration:

### Preprocessor Defines

Define these **before** including `gekkonet.h`:

```c
// Static library:
#define GEKKONET_STATIC
#include <gekkonet.h>

// Shared library (DLL): no define needed

// If built with NO_ASIO_BUILD=ON:
#define GEKKONET_NO_ASIO
#include <gekkonet.h>
```

### Output Naming

| Configuration | Output name |
|---|---|
| Shared + ASIO | `GekkoNet` |
| Static + ASIO | `GekkoNet_STATIC` |
| Shared + no ASIO | `GekkoNet_NO_ASIO` |
| Static + no ASIO | `GekkoNet_STATIC_NO_ASIO` |

### Platform Notes

- **Windows (MSVC)**: CMake sets `/MT` (static) or `/MD` (DLL) automatically. ASIO links `ws2_32`.
- **Linux/macOS**: `-fPIC` is added automatically.

---

## Quick Start

Minimal 2 player online session:

```c
#define GEKKONET_STATIC
#include <gekkonet.h>
#include <string.h>

typedef struct { unsigned char left, right, up, down; } MyInput;
typedef struct { int px[2]; int py[2]; } MyState;

MyState state = {0};

int main() {
    // Create and configure
    GekkoSession* session = NULL;
    gekko_create(&session, GekkoGameSession);

    GekkoConfig config = {0};
    config.num_players = 2;
    config.input_size = sizeof(MyInput);
    config.state_size = sizeof(MyState);
    config.input_prediction_window = 8;
    config.desync_detection = true;

    gekko_start(session, &config);
    gekko_net_adapter_set(session, gekko_default_adapter(7000));

    // Player 0 = local, Player 1 = remote
    gekko_add_actor(session, GekkoLocalPlayer, NULL);

    GekkoNetAddress addr = {0};
    char addr_str[] = "127.0.0.1:7001";
    addr.data = addr_str;
    addr.size = sizeof(addr_str) - 1;
    gekko_add_actor(session, GekkoRemotePlayer, &addr);

    // Game loop
    while (1) {
        gekko_network_poll(session);

        MyInput input = {0}; // poll your input system
        gekko_add_local_input(session, 0, &input);

        // Session events (connections, desyncs)
        int count = 0;
        GekkoSessionEvent** sevts = gekko_session_events(session, &count);
        for (int i = 0; i < count; i++) {
            switch (sevts[i]->type) {
            case GekkoPlayerSyncing:
                // show sync progress: sevts[i]->data.syncing.current / .max
                break;
            case GekkoPlayerConnected:
                break;
            case GekkoPlayerDisconnected:
                break;
            case GekkoDesyncDetected:
                // sevts[i]->data.desynced has frame, checksums, remote_handle
                break;
            }
        }

        // Game events (save/load/advance)
        count = 0;
        GekkoGameEvent** gevts = gekko_update_session(session, &count);
        for (int i = 0; i < count; i++) {
            switch (gevts[i]->type) {
            case GekkoSaveEvent:
                *gevts[i]->data.save.state_len = sizeof(MyState);
                *gevts[i]->data.save.checksum = my_crc(&state, sizeof(MyState));
                memcpy(gevts[i]->data.save.state, &state, sizeof(MyState));
                break;

            case GekkoLoadEvent:
                memcpy(&state, gevts[i]->data.load.state, sizeof(MyState));
                break;

            case GekkoAdvanceEvent: {
                MyInput* inputs = (MyInput*)gevts[i]->data.adv.inputs;
                // inputs[0] = player 0, inputs[1] = player 1
                state.px[0] += inputs[0].right - inputs[0].left;
                state.px[1] += inputs[1].right - inputs[1].left;
                break;
            }
            }
        }

        // render, frame timing (see Network Stats section for pacing)
    }

    gekko_default_adapter_destroy();
    gekko_destroy(&session);
    return 0;
}
```

**Rollback event order**: On misprediction GekkoNet emits: `GekkoSaveEvent` (current) > `GekkoLoadEvent` (rollback) > `GekkoAdvanceEvent` x N (re-simulate with `rolling_back = true`) > `GekkoSaveEvent` (new current). Skip rendering/audio when `rolling_back` is true.

---

## API Reference

All public types and functions are declared in [`gekkonet.h`](GekkoLib/include/gekkonet.h).

For the full API reference see the **[GekkoNet API Docs](https://heatxd.github.io/GekkoNet/)**.

---

## Session Types

### GameSession (`GekkoGameSession`)

Standard online session. Supports local players, remote players and spectators with rollback.

### StressSession (`GekkoStressSession`)

Local only session that continuously rolls back over `check_distance` frames to verify your save/load is deterministic. No networking required. Use this before testing online to help find desyncs in your local state.

```c
gekko_create(&session, GekkoStressSession);

GekkoConfig config = {0};
config.num_players = 2;
config.input_size = sizeof(MyInput);
config.state_size = sizeof(MyState);
config.check_distance = 10;

gekko_start(session, &config);

// all players are local, no adapter needed
for (int i = 0; i < 2; i++) {
    gekko_add_actor(session, GekkoLocalPlayer, NULL);
    gekko_set_local_delay(session, i, 1);
}
// then run the same game loop. a GekkoDesyncDetected event means your state isn't deterministic.
```

### SpectateSession (`GekkoSpectateSession`)

Spectator receives inputs from a host and replays the game deterministically.

**Host side**, runs a normal `GekkoGameSession`:

```c
config.max_spectators = 1;
// ... after adding players:
gekko_add_actor(session, GekkoSpectator, &spectator_addr);
```

**Spectator side**:

```c
gekko_create(&session, GekkoSpectateSession);

config.spectator_delay = 300;
gekko_start(session, &config);
gekko_net_adapter_set(session, gekko_default_adapter(spectator_port));

// the host is added as a remote player
gekko_add_actor(session, GekkoRemotePlayer, &host_addr);
```

The spectator does **not** call `gekko_add_local_input`. Handle `GekkoSpectatorPaused` / `GekkoSpectatorUnpaused` session events to know when the buffer runs dry.

---

## Custom Networking

If you want to use your own transport instead of the built in ASIO adapter you can implement `GekkoNetAdapter`:

```c
void my_send(GekkoNetAddress* addr, const char* data, int length) {
    // addr->data is the address you passed to gekko_add_actor
    // send 'length' bytes of 'data' to that address
}

GekkoNetResult** my_receive(int* length) {
    // return array of received packets since last poll
    // set *length to count. return NULL with *length = 0 if nothing available.
}

void my_free(void* data_ptr) {
    // free packet data allocated by my_receive
    free(data_ptr);
}

GekkoNetAdapter adapter = {0};
adapter.send_data = my_send;
adapter.receive_data = my_receive;
adapter.free_data = my_free;

gekko_net_adapter_set(session, &adapter);
```

GekkoNet frees `data` pointers inside each `GekkoNetResult` via `free_data`. The result array itself is yours to manage.

Build with `NO_ASIO_BUILD=ON` to remove the ASIO dependency entirely.

---

## Configuration Guide

| Field | Description | Recommended |
|---|---|---|
| `num_players` | Total active players (local + remote). Must match on all peers. | |
| `max_spectators` | Max spectator connections. Host side only. | 0 if unused |
| `input_prediction_window` | Frames of remote input to predict ahead. Larger tolerates more latency but deeper rollbacks. | 8 to 10 |
| `spectator_delay` | Frames buffered before spectator playback starts. Higher is more jitter resistant. | 120 to 300 |
| `input_size` | Bytes of **one player's** input struct. Inputs are packed contiguously (`num_players * input_size`). | |
| `state_size` | Max bytes for your serialized gamestate. Buffers are pre-allocated to this size. | |
| `limited_saving` | Save state less often. Cheaper saves but more re-simulation on rollback. | `false` to start |
| `desync_detection` | Compare checksums between peers. Requires meaningful checksums in `GekkoSaveEvent`. Only works with `limited_saving = false`. | `true` |
| `check_distance` | `GekkoStressSession` only. Frames between rollback checks. | 7 to 15 |

---

## Network Stats & Frame Pacing

```c
// per player network stats
GekkoNetworkStats stats = {0};
gekko_network_stats(session, remote_handle, &stats);
// stats.last_ping (ms), stats.avg_ping (ms), stats.jitter (ms)
// stats.kb_sent (KB/s), stats.kb_received (KB/s)
```

### Frame Pacing with `gekko_frames_ahead`

`gekko_frames_ahead(session)` returns how far ahead the local session is. Use it to slow down when you're ahead to prevent unnecessary rollbacks:

```c
float ahead = gekko_frames_ahead(session);
uint64_t delay = frame_delay_ns;
if (ahead > 0.5f) {
    delay = frame_delay_ns * 1.016;  // slow ~1.6% to let remote catch up
}
```

---

## Tips

- **Input structs**: Keep them small (bitfields for booleans). POD types only, no pointers. Consistent size across platforms.
- **Rollback rendering**: You can check `data.adv.rolling_back` on `GekkoAdvanceEvent` to skip rendering and audio during re-simulation.
- **Test locally first**: Use `GekkoStressSession` with `check_distance` set (eg. 10). It will continuously roll back and compare checksums over that distance. If your save/load has any non-determinism you'll see `GekkoDesyncDetected` immediately.
