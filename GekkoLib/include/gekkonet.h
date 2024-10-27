#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef GEKKONET_STATIC
    // Static library - no import/export needed
#define GEKKONET_API
#else
#ifdef GEKKONET_DLL_EXPORT
    // Building the DLL
#define GEKKONET_API __declspec(dllexport)
#else
    // Using the DLL
#define GEKKONET_API __declspec(dllimport)
#endif
#endif
#else
    // Non-Windows platforms don't need special export macros
#define GEKKONET_API
#endif

// GekkoNet is mostly self contained memory wise using internal memory buffers.
// Technically the user shouldnt need to free or create any memory (look at the examples).
// The only case the user needs to create memory is when slotting in their own GekkoNetAdapter
typedef struct GekkoSession GekkoSession;

typedef struct GekkoConfig {
    unsigned char num_players;
    unsigned char max_spectators;
    unsigned char input_prediction_window;
    unsigned char spectator_delay;
    unsigned int input_size;
    unsigned int state_size;
    bool limited_saving;
    bool post_sync_joining;
    bool desync_detection;
} GekkoConfig;

typedef enum GekkoPlayerType {
    LocalPlayer,
    RemotePlayer,
    Spectator
} GekkoPlayerType;

typedef struct GekkoNetAddress {
    void* data;
    unsigned int size;
} GekkoNetAddress;

typedef struct GekkoNetResult {
    GekkoNetAddress addr;
    unsigned int data_len;
    void* data;
} GekkoNetResult;

typedef struct GekkoNetAdapter {
    // send data to another peer
    void (*send_data)(GekkoNetAddress* addr, const char* data, int length);
    // receive all packets accumulated between the last frame and now
    // the array of results will be freed after use using the free_data function
    GekkoNetResult** (*receive_data)(int* length);
    // free data function so gekkonet can cleanup data that the user created.
    void (*free_data)(void* data_ptr);
} GekkoNetAdapter;

typedef enum GekkoGameEventType {
    EmptyGameEvent = -1,
    AdvanceEvent,
    SaveEvent,
    LoadEvent
} GekkoGameEventType;

typedef struct GekkoGameEvent {
    GekkoGameEventType type;

    union EventData {
        // events 
        struct Advance {
            int frame;
            unsigned int input_len;
            unsigned char* inputs;
        } adv;
        struct Save {
            int frame;
            unsigned int* checksum;
            unsigned int* state_len;
            unsigned char* state;
        } save;
        struct Load {
            int frame;
            unsigned int state_len;
            unsigned char* state;
        } load;
    } data;
} GekkoGameEvent;

typedef enum GekkoSessionEventType {
    EmptySessionEvent = -1,
    PlayerSyncing,
    PlayerConnected,
    PlayerDisconnected,
    SessionStarted,
    SpectatorPaused,
    SpectatorUnpaused,
    DesyncDetected
} GekkoSessionEventType;

typedef struct GekkoSessionEvent {
    GekkoSessionEventType type;

    union Data {
        struct Syncing {
            int handle;
            unsigned char current;
            unsigned char max;
        } syncing;
        struct Connected {
            int handle;
        } connected;
        struct Disconnected {
            int handle;
        } disconnected;
        struct Desynced {
            int frame;
            unsigned int local_checksum;
            unsigned int remote_checksum;
            int remote_handle;
        } desynced;
    } data;
} GekkoSessionEvent;

// Public Facing API
GEKKONET_API bool gekko_create(GekkoSession** session);

GEKKONET_API bool gekko_destroy(GekkoSession* session);

GEKKONET_API void gekko_start(GekkoSession* session, GekkoConfig* config);

GEKKONET_API void gekko_net_adapter_set(GekkoSession* session, GekkoNetAdapter* adapter);

GEKKONET_API int gekko_add_actor(GekkoSession* session, GekkoPlayerType player_type, GekkoNetAddress* addr);

GEKKONET_API void gekko_set_local_delay(GekkoSession* session, int player, unsigned char delay);

GEKKONET_API void gekko_add_local_input(GekkoSession* session, int player, void* input);

GEKKONET_API GekkoGameEvent** gekko_update_session(GekkoSession* session, int* count);

GEKKONET_API GekkoSessionEvent** gekko_session_events(GekkoSession* session, int* count);

GEKKONET_API float gekko_frames_ahead(GekkoSession* session);

#ifdef __cplusplus
}
#endif
