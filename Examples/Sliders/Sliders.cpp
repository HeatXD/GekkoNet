#include <SDL3/SDL.h>

#define GEKKONET_STATIC

#include <gekkonet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cassert>

#define MAX_PLAYERS 4
#define MAP_SIZE 400
#define GAME_SCALE 1000

constexpr uint8_t PLAYER_COLORS[4 * MAX_PLAYERS] = {
    255, 0, 0, 255,
    0, 255, 0, 255,
    71, 167, 199, 255,
    255, 0, 255, 255,
};

struct Input {
    uint8_t left : 1;
    uint8_t right : 1;
};

struct State {
    int entt_px[MAX_PLAYERS];

    void draw(SDL_Renderer* renderer) const {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        float compY = MAP_SIZE / MAX_PLAYERS;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            const uint8_t* col = &PLAYER_COLORS[i * 4];
            SDL_SetRenderDrawColor(renderer, col[0], col[1], col[2], col[3]);
            float px = (float)entt_px[i] / GAME_SCALE;
            float py = compY * i;
            float w = compY;
            float h = compY;
            SDL_FRect rect = { px, py, w, h };
            SDL_RenderFillRect(renderer, &rect);
        }
        SDL_RenderPresent(renderer);
    }

    void tick(Input inputs[MAX_PLAYERS]) {
        // move sliders
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (inputs[i].left) entt_px[i] -= 6000;
            else if (inputs[i].right) entt_px[i] += 6000;
        }

        // clamp to screen
        const int max = GAME_SCALE * (MAP_SIZE * 2 - MAP_SIZE / MAX_PLAYERS);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (entt_px[i] > max) entt_px[i] = max;
            else if (entt_px[i] < 0) entt_px[i] = 0;
        }
    }

    Input poll_input() const {
        Input inp = {};
        const bool* keys = SDL_GetKeyboardState(NULL);
        inp.left = keys[SDL_SCANCODE_LEFT];
        inp.right = keys[SDL_SCANCODE_RIGHT];
        return inp;
    }
};

static void handle_frame_time(
    const uint64_t& frame_start,
    uint64_t& frame_time_ns,
    const uint64_t& frame_delay_ns,
    const uint64_t& perf_freq,
    const float frames_ahead) {
    uint64_t frame_end = SDL_GetPerformanceCounter();
    frame_time_ns = ((frame_end - frame_start) * 1000000000) / perf_freq;
    if (frame_delay_ns > frame_time_ns) {
        uint64_t delay_ns = frames_ahead > .5f ? frame_delay_ns * 1.016 : frame_delay_ns -  frame_time_ns;
        SDL_DelayNS(delay_ns);
    }
}

int main(int argc, char* argv[]) {
    // get network info
    if (argc < 4) {
        printf("Usage: %s <local_player> <port1> <port2> <port3> <port4>\n", argv[0]);
        return 1;
    }

    // parse local player number
    int num_players = argc - 2;
    printf("num_players: %d\n", num_players);
    int local_player = atoi(argv[1]);
    if (local_player < 0 || local_player >= MAX_PLAYERS || local_player > num_players - 1) {
        printf("Invalid local player index. Must be 0-%d\n", num_players - 1);
        return 1;
    }

    // parse ports
    int ports[MAX_PLAYERS] = {};
    for (int i = 0; i < argc - 2; i++) {
        ports[i] = atoi(argv[i + 2]);
        if (ports[i] <= 0 || ports[i] > 65535) {
            printf("Invalid port: %d\n", ports[i]);
            return 1;
        }
    }

    // print for verification
    printf("Local player: %d\n", local_player);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        printf("Player %d port: %d\n", i, ports[i]);
    }

    // window init
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL3 GekkoNet Example", MAP_SIZE * 2, MAP_SIZE, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    // timing
    const uint64_t frame_delay_ns = (1000000000 / 60); // ns/frame
    const uint64_t performance_frequency = SDL_GetPerformanceFrequency();
    uint64_t frame_start = 0, frame_time_ns = 0;
    // gekkonet
    GekkoSession* session = nullptr;

    assert(gekko_create(&session));

    GekkoConfig config {};

    config.desync_detection = true;
    config.input_size = sizeof(Input);
    config.state_size = sizeof(State);
    config.max_spectators = 0;
    config.input_prediction_window = 8;
    config.num_players = num_players;

    gekko_start(session, &config);
    GekkoNetAdapter* adapter = gekko_default_adapter(ports[local_player]);
    gekko_net_adapter_set(session, adapter);

    for (int i = 0; i < num_players; i++) {
        if (i == local_player) {
            gekko_add_actor(session, LocalPlayer, nullptr);
            gekko_set_local_delay(session, i, 2);
        }
        else {
            GekkoNetAddress addr = {};
            std::string address_str = "127.0.0.1:" + std::to_string(ports[i]);
            addr.data = (void*)address_str.c_str();
            addr.size = address_str.size();
            gekko_add_actor(session, RemotePlayer, &addr);
        }
    }

    // game
    State game = {};
    Input inputs[MAX_PLAYERS] = {};

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

        auto local_input = game.poll_input();
        gekko_add_local_input(session, local_player, &local_input);

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
            }
        }

        count = 0;
        GekkoGameEvent** updates = gekko_update_session(session, &count);
        for (int i = 0; i < count; i++) {
            GekkoGameEvent* event = updates[i];
            switch (event->type) {
            case SaveEvent:
                *event->data.save.state_len = sizeof(State);
                *event->data.save.checksum = SDL_crc32(0, &game, sizeof(State));
                memcpy(event->data.save.state, &game, sizeof(State));
                break;

            case LoadEvent:
                memcpy(&game, event->data.load.state, sizeof(State));
                printf("rb start\n");
                break;

            case AdvanceEvent:
                printf("f%d,", event->data.adv.frame);
                for (int j = 0; j < num_players; j++) {
                    inputs[j] = ((Input*)(event->data.adv.inputs))[j];
                    printf(" p%d %d%d", j, inputs[j].left, inputs[j].right);
                }
                printf("\n");
                game.tick(inputs);
                break;
            }
        }

        game.draw(renderer);

        handle_frame_time(
            frame_start,
            frame_time_ns,
            frame_delay_ns,
            performance_frequency,
            gekko_frames_ahead(session)
        );
    }

    assert(gekko_default_adapter_destroy());
    assert(gekko_destroy(&session));

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

