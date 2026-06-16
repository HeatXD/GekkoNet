#include <SDL3/SDL.h>
#include <stdio.h>

#include "../GekkoGame/include/game.h"

#define GEKKONET_STATIC

#include <gekkonet.h>

#include <stdlib.h>
#include <cassert>

static void handle_frame_time(
    const uint64_t& frame_start,
    uint64_t& frame_time_ns,
    const uint64_t& frame_delay_ns,
    const uint64_t& perf_freq) {
    uint64_t frame_end = SDL_GetPerformanceCounter();
    frame_time_ns = ((frame_end - frame_start) * 1000000000) / perf_freq;
    if (frame_delay_ns > frame_time_ns) {
        SDL_DelayNS(frame_delay_ns - frame_time_ns);
    }
}

static GekkoGame::Input poll_player(int player) {
    GekkoGame::Input inp = {};
    const bool* keys = SDL_GetKeyboardState(NULL);
    switch (player) {
    case 0: // left vertical paddle
        inp.up = keys[SDL_SCANCODE_UP];
        inp.down = keys[SDL_SCANCODE_DOWN];
        break;
    case 1: // right vertical paddle
        inp.up = keys[SDL_SCANCODE_W];
        inp.down = keys[SDL_SCANCODE_S];
        break;
    case 2: // top horizontal paddle
        inp.left = keys[SDL_SCANCODE_J];
        inp.right = keys[SDL_SCANCODE_L];
        break;
    case 3: // bottom horizontal paddle
        inp.left = keys[SDL_SCANCODE_KP_4];
        inp.right = keys[SDL_SCANCODE_KP_6];
        break;
    }
    return inp;
}

int main(int argc, char* argv[]) {
    using namespace GekkoGame;

    int num_players = 2;
    if (argc >= 2) {
        num_players = atoi(argv[1]);
        if (num_players < 1 || num_players > MAX_PLAYERS) {
            printf("Invalid num_players: %d. Must be 1-%d\n", num_players, MAX_PLAYERS);
            return 1;
        }
    }
    printf("Local Couch session with %d player(s)\n", num_players);
    printf("Controls: P0 Up/Down | P1 W/S | P2 J/L | P3 Numpad4/Numpad6 | F1/F2 delay | F3/F4 runahead\n");

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL3 GekkoNet Local Session", FIELD_SIZE, FIELD_SIZE, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const uint64_t frame_delay_ns = (1000000000 / TARGET_FPS);
    const uint64_t performance_frequency = SDL_GetPerformanceFrequency();
    uint64_t frame_start = 0, frame_time_ns = 0;

    GekkoSession* session = nullptr;

    gekko_create(&session, GekkoGameSession);

    GekkoConfig config{};

    config.input_size = sizeof(Input);
    config.state_size = sizeof(Gamestate::State);
    config.max_spectators = 0;
    // local play is lockstep: no prediction window, no rollback.
    config.input_prediction_window = 0;
    config.limited_saving = true;
    config.desync_detection = false;
    config.num_players = num_players;

    gekko_start(session, &config);
    // no net adapter and no remote actors: this is a purely local session.

    unsigned char current_runahead = 4;
    gekko_set_runahead(session, current_runahead);

    unsigned char current_delay = 2;
    for (int i = 0; i < num_players; i++) {
        gekko_add_actor(session, GekkoLocalPlayer, nullptr);
        gekko_set_local_delay(session, i, current_delay);
    }

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
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                case SDLK_F1:
                    if (current_delay > 0) {
                        current_delay--;
                        for (int i = 0; i < num_players; i++) {
                            gekko_set_local_delay(session, i, current_delay);
                        }
                        printf("delay: %d\n", current_delay);
                    }
                    break;
                case SDLK_F2:
                    if (current_delay < 15) {
                        current_delay++;
                        for (int i = 0; i < num_players; i++) {
                            gekko_set_local_delay(session, i, current_delay);
                        }
                        printf("delay: %d\n", current_delay);
                    }
                    break;
                case SDLK_F3:
                    if (current_runahead > 0) {
                        current_runahead--;
                        gekko_set_runahead(session, current_runahead);
                        printf("runahead: %d\n", current_runahead);
                    }
                    break;
                case SDLK_F4:
                    if (current_runahead < 15) {
                        current_runahead++;
                        gekko_set_runahead(session, current_runahead);
                        printf("runahead: %d\n", current_runahead);
                    }
                    break;
                }
            }
        }

        for (int i = 0; i < num_players; i++) {
            Input local_input = poll_player(i);
            gekko_add_local_input(session, i, &local_input);
        }

        int count = 0;
        GekkoGameEvent** updates = gekko_update_session(session, &count);
        for (int i = 0; i < count; i++) {
            GekkoGameEvent* event = updates[i];
            switch (event->type) {
            case GekkoSaveEvent:
                *event->data.save.state_len = sizeof(Gamestate::State);
                *event->data.save.checksum = SDL_crc32(0, &gs.state, sizeof(Gamestate::State));
                memcpy(event->data.save.state, &gs.state, sizeof(Gamestate::State));
                break;

            case GekkoLoadEvent:
                memcpy(&gs.state, event->data.load.state, sizeof(Gamestate::State));
                break;

            case GekkoAdvanceEvent:
                Input inputs[MAX_PLAYERS] = {};
                for (int j = 0; j < num_players; j++) {
                    inputs[j] = ((Input*)(event->data.adv.inputs))[j];
                }
                gs.Update(inputs);
                break;
            }
        }

        gs.Draw(renderer);

        char title[128];
        snprintf(title, sizeof(title), "local couch session | players: %d | delay: %d | runahead: %d",
            num_players, current_delay, current_runahead);
        SDL_SetWindowTitle(window, title);

        handle_frame_time(frame_start, frame_time_ns, frame_delay_ns, performance_frequency);
    }

    gekko_destroy(&session);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
