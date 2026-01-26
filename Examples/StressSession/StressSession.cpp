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

    // window init
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL3 GekkoNet Pong Stress Example", FIELD_SIZE, FIELD_SIZE, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // timing
    const uint64_t frame_delay_ns = (1000000000 / TARGET_FPS); // ns/frame
    const uint64_t performance_frequency = SDL_GetPerformanceFrequency();
    uint64_t frame_start = 0, frame_time_ns = 0;

    // gekkonet setup
    GekkoSession* session = nullptr;

    gekko_create(&session, GekkoSessionType::Stress);

    GekkoConfig config{};

    config.desync_detection = true;
    config.input_size = sizeof(Input);
    config.state_size = sizeof(Gamestate::State);
    config.num_players = 2;
    config.check_distance = 10;

    gekko_start(session, &config);

    for (int i = 0; i < 2; i++) {
        gekko_add_actor(session, LocalPlayer, nullptr);
        gekko_set_local_delay(session, i, 1);
    }

    // setup game
    Gamestate gs = {};
    gs.Init(2);

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
        for (int i = 0; i < 2; i++) {
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
                printf("sf%d \n", event->data.save.frame);
                break;

            case LoadEvent:
                memcpy(&gs.state, event->data.load.state, sizeof(Gamestate::State));
                printf("lf%d \n", event->data.load.frame);
                break;

            case AdvanceEvent:
                Input inputs[MAX_PLAYERS] = {};
                printf("f%d,", event->data.adv.frame);
                for (int j = 0; j < 2; j++) {
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
    gekko_destroy(&session);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
