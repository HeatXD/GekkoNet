#include <SDL3/SDL.h>
#include <stdio.h>

#include "../GekkoGame/include/game.h"

#define GEKKONET_STATIC

#include <gekkonet.h>

#include <stdlib.h>
#include <string>
#include <cassert>
#include <vector>
#include <sstream>

static void handle_frame_time(
    const uint64_t& frame_start,
    uint64_t& frame_time_ns,
    const uint64_t& frame_delay_ns,
    const uint64_t& perf_freq,
    const float frames_ahead) {
    uint64_t frame_end = SDL_GetPerformanceCounter();
    frame_time_ns = ((frame_end - frame_start) * 1000000000) / perf_freq;
    if (frame_delay_ns > frame_time_ns) {
        uint64_t delay_ns = frames_ahead > .5f ? frame_delay_ns * 1.016 : frame_delay_ns - frame_time_ns;
        SDL_DelayNS(delay_ns);
    }
}

int main(int argc, char* argv[]) {
    using namespace GekkoGame;

    if (argc != 4) {
        printf("Usage:\n");
        printf("  Host:     %s -h <local_port> <remote_port>\n", argv[0]);
        printf("  Spectate: %s -s <local_port> <remote_port>\n", argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    bool is_spectator = (mode == "-s");

    if (mode != "-h" && mode != "-s") {
        printf("Invalid mode. Use '-h' for host or '-s' for spectate\n");
        return 1;
    }

    int local_port = atoi(argv[2]);
    int remote_port = atoi(argv[3]);

    if (local_port <= 0 || local_port > 65535 || remote_port <= 0 || remote_port > 65535) {
        printf("Invalid port numbers. Must be between 1-65535\n");
        return 1;
    }

    const int NUM_PLAYERS = 2;

    printf("%s mode - Local port: %d, Remote port: %d\n",
        is_spectator ? "Spectator" : "Host", local_port, remote_port);

    // window init
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL3 GekkoNet Pong Example", FIELD_SIZE, FIELD_SIZE, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    // timing
    const uint64_t frame_delay_ns = (1000000000 / TARGET_FPS); // ns/frame
    const uint64_t performance_frequency = SDL_GetPerformanceFrequency();
    uint64_t frame_start = 0, frame_time_ns = 0;

    // gekkonet setup
    GekkoSession* session = nullptr;

    gekko_create_game_session(&session);

    GekkoConfig config{};

    config.desync_detection = true;
    config.input_size = sizeof(Input);
    config.state_size = sizeof(Gamestate::State);
    config.max_spectators = 1;
    config.input_prediction_window = 10;
    config.num_players = NUM_PLAYERS;

    gekko_start_session(session, &config);
    gekko_net_adapter_set(session, gekko_default_adapter(local_port));

    GekkoNetAddress rem_addr = {};
    std::string address_str = "127.0.0.1:" + std::to_string(remote_port);
    rem_addr.data = (void*)address_str.c_str();
    rem_addr.size = address_str.size();

    if (!is_spectator) {
        // add local players
        for (int i = 0; i < NUM_PLAYERS; i++) {
            gekko_add_actor(session, LocalPlayer, nullptr);
            gekko_set_local_delay(session, i, 1);
        }
        // add spectator
        gekko_add_actor(session, Spectator, &rem_addr);
    } else {
        // add spectator host
        gekko_add_actor(session, RemotePlayer, &rem_addr);
    }

    // setup game
    Gamestate gs = {};
    gs.Init(NUM_PLAYERS);

    bool running = true;
    while (running) {
        frame_start = SDL_GetPerformanceCounter();
        gekko_network_poll(session);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        auto local_input = gs.PollInput();
        for (int i = 0; i < NUM_PLAYERS; i++) {
            gekko_add_local_input(session, i, &local_input);
        }

        int count = 0;
        GekkoSessionEvent** events = gekko_session_events(session, &count);
        for (int i = 0; i < count; i++) {
            GekkoSessionEvent* event = events[i];
            switch (event->type) {
            case DesyncDetected:
                auto desync = event->data.desynced;
                printf(
                    "DESYNC!!! f:%d, rh:%d, lc:%u, rc:%u\n", desync.frame, desync.remote_handle,
                    desync.local_checksum, desync.remote_checksum
                );
                assert(false);
                break;

            case PlayerConnected:
                auto connect = event->data.connected;
                printf("Player %i connected\n", connect.handle);
                break;

            case PlayerDisconnected:
                auto disconnect = event->data.disconnected;
                printf("Player %i disconnected\n", disconnect.handle);
                break;

            case PlayerSyncing:
                auto sync = event->data.syncing;
                printf("Player %i is connecting %d/%d\n", sync.handle, sync.current, sync.max);
                break;
            }
        }

        count = 0;
        GekkoGameEvent** updates = gekko_update_session(session, &count);
        for (int i = 0; i < count; i++) {
            GekkoGameEvent* event = updates[i];
            switch (event->type) {
            case SaveEvent:
                *event->data.save.state_len = sizeof(Gamestate::State);
                *event->data.save.checksum = SDL_crc32(0, &gs.state, sizeof(Gamestate::State));
                memcpy(event->data.save.state, &gs.state, sizeof(Gamestate::State));
                break;

            case LoadEvent:
                memcpy(&gs.state, event->data.load.state, sizeof(Gamestate::State));
                break;

            case AdvanceEvent:
                Input inputs[MAX_PLAYERS] = {};
                printf("f%d,", event->data.adv.frame);
                for (int j = 0; j < NUM_PLAYERS; j++) {
                    inputs[j] = ((Input*)(event->data.adv.inputs))[j];
                    printf(" p%d %d%d", j, inputs[j].left, inputs[j].right);
                }
                printf("\n");
                gs.Update(inputs);
                break;
            }
        }

        gs.Draw(renderer);

        handle_frame_time(
            frame_start,
            frame_time_ns,
            frame_delay_ns,
            performance_frequency,
            gekko_frames_ahead(session)
        );
    }

    gekko_default_adapter_destroy();
    gekko_destroy_session(&session);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
