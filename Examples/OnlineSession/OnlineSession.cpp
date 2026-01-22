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

    if (argc < 4) {
        printf("Usage: %s <local_player1,local_player2,...> <port1> <port2> <port3> <port4>\n", argv[0]);
        return 1;
    }

    // parse local player numbers
    int num_players = argc - 2;
    printf("num_players: %d\n", num_players);

    std::string local_players_str = argv[1];
    std::vector<int> local_players;
    std::istringstream ss(local_players_str);
    std::string token;

    while (std::getline(ss, token, ',')) {
        try {
            int player_num = std::stoi(token);
            if (player_num < 0 || player_num >= MAX_PLAYERS || player_num > num_players - 1) {
                printf("Invalid local player index: %d. Must be 0-%d\n", player_num, num_players - 1);
                return 1;
            }
            local_players.push_back(player_num);
        }
        catch (const std::exception&) {
            printf("Invalid number format: %s\n", token.c_str());
            return 1;
        }
    }

    int num_local_players = local_players.size();

    printf("Local players: ");
    for (int player : local_players) {
        printf("%d ", player);
    }
    printf("\n");

    // parse ports
    int ports[MAX_PLAYERS] = {};
    for (int i = 0; i < argc - 2; i++) {
        ports[i] = atoi(argv[i + 2]);
        if (ports[i] <= 0 || ports[i] > 65535) {
            printf("Invalid port: %d\n", ports[i]);
            return 1;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        printf("Player %d port: %d\n", i, ports[i]);
    }

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
    config.max_spectators = 0;
    config.input_prediction_window = 10;
    config.num_players = num_players;

    gekko_start_session(session, &config);
    gekko_net_adapter_set(session, gekko_default_adapter(ports[local_players[0]]));

    for (int i = 0; i < num_players; i++) {
        bool is_local = false;
        for (int j = 0; j < num_local_players; j++) {
            if (local_players[j] == i) {
                is_local = true;
                break;
            }
        }

        if (is_local) {
            gekko_add_actor(session, LocalPlayer, nullptr);
            gekko_set_local_delay(session, i, 1);
        }
        else {
            GekkoNetAddress addr = {};
            std::string address_str = "127.0.0.1:" + std::to_string(ports[i]);
            addr.data = (void*)address_str.c_str();
            addr.size = address_str.size();
            gekko_add_actor(session, RemotePlayer, &addr);
        }
    }

    // setup game
    Gamestate gs = {};
    gs.Init(num_players);

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
        for (int i = 0; i < num_local_players; i++) {
            gekko_add_local_input(session, local_players[i], &local_input);
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
                for (int j = 0; j < num_players; j++) {
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
