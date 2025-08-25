#include <SDL3/SDL.h>
#include <stdio.h>

constexpr int TARGET_FPS = 60;
constexpr int MAX_PLAYERS = 4;
constexpr int MAX_BALLS = 4;
constexpr int MAX_ENTITIES = MAX_PLAYERS + MAX_BALLS;

struct Gamestate {
    uint32_t frame = 0;
    uint8_t hp[MAX_PLAYERS];
    int16_t px[MAX_ENTITIES], py[MAX_ENTITIES];
    int16_t vx[MAX_ENTITIES], vy[MAX_ENTITIES];
    struct Flags {
        // ive got 32 bits for flags
        int started: 1;
        int finished: 1;
        int players: 2; // 2 bits are enough to repesent 4 players
        int balls : 2; // 2 bits are enough to repesent 4 balls
    } flags = {};
};

static void draw_frame(SDL_Renderer* renderer, Gamestate& state) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_FRect rect = { 100, 100, 100, 100 };
    SDL_RenderFillRect(renderer, &rect);
    SDL_RenderPresent(renderer);
}

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
    // window init
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL3 GekkoNet Example", 400, 400, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    // timing
    const uint64_t frame_delay_ns = (1000000000 / TARGET_FPS); // ns/frame
    const uint64_t performance_frequency = SDL_GetPerformanceFrequency();
    uint64_t frame_start = 0, frame_time_ns = 0;
    //
    Gamestate gs = {};

    bool running = true;
    while (running) {
        frame_start = SDL_GetPerformanceCounter();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        draw_frame(renderer, gs);
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
