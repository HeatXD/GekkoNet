#include <SDL3/SDL.h>
#include <stdio.h>

#include "../GekkoGame/include/game.h"

static void handle_frame_time(
    const uint64_t& frame_start,
    uint64_t& frame_time_ns,
    const uint64_t& frame_delay_ns,
    const uint64_t& perf_freq) {
    uint64_t frame_end = SDL_GetPerformanceCounter();
    frame_time_ns = ((frame_end - frame_start) * 1000000000) / perf_freq;
    // todo update based on frames ahead
    if (frame_delay_ns > frame_time_ns) {
        uint64_t delay_ns = frame_delay_ns - frame_time_ns;
        SDL_DelayNS(delay_ns);
    }
}

int main() {
    using namespace GekkoGame;
    // window init
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL3 GekkoNet Pong Example", FIELD_SIZE, FIELD_SIZE * 1.15, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    // timing
    const uint64_t frame_delay_ns = (1000000000 / TARGET_FPS); // ns/frame
    const uint64_t performance_frequency = SDL_GetPerformanceFrequency();
    uint64_t frame_start = 0, frame_time_ns = 0;
    // 
    Gamestate gs = {};
    gs.Init(2);

    bool running = true;
    while (running) {
        frame_start = SDL_GetPerformanceCounter();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        Input inputs[MAX_PLAYERS] = {};
        inputs[0].up = true;
        inputs[1].down = true;
        inputs[2].right = true;
        inputs[3].left = true;

        gs.Update(inputs);
        gs.Draw(renderer);

        handle_frame_time(
            frame_start,
            frame_time_ns,
            frame_delay_ns,
            performance_frequency
        );
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
