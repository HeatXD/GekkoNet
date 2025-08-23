#include <SDL3/SDL.h>
#include <stdio.h>

const int TARGET_FPS = 60;
const int MAX_PLAYERS = 4;

struct Gamestate {
    int px[MAX_PLAYERS], py[MAX_PLAYERS];
    int vx[MAX_PLAYERS], vy[MAX_PLAYERS];
    Uint8 hp[MAX_PLAYERS];
    struct Flags {
        bool started : 1;
        bool finished : 1;
    } flags = {};
};

int main() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("SDL3 GekkoNet Example", 400, 400, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    const Uint64 frame_delay_ns = (1000000000 / TARGET_FPS); // ns/frame
    Uint64 performance_frequency = SDL_GetPerformanceFrequency();

    Uint64 frame_start, frame_time_ns = 0, frame_counter = 0;

    bool running = true;
    while (running) {
        frame_start = SDL_GetPerformanceCounter();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_FRect rect = { frame_counter % 400, 100, 100, 100 };
        SDL_RenderFillRect(renderer, &rect);

        // Present renderer
        SDL_RenderPresent(renderer);

        Uint64 frame_end = SDL_GetPerformanceCounter();
        frame_time_ns = ((frame_end - frame_start) * 1000000000) / performance_frequency;

        if (frame_counter % 60 == 0) {
            printf("FT: %llu ns (%.2f ms)\n",
                frame_time_ns,
                frame_time_ns / 1000000.0);
        }

        if (frame_delay_ns > frame_time_ns) {
            Uint64 delay_ns = frame_delay_ns - frame_time_ns;
            SDL_DelayNS(delay_ns);
        }

        frame_counter++;
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
