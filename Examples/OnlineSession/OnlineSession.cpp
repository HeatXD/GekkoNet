#include <SDL3/SDL.h>
#include <stdio.h>

constexpr int TARGET_FPS = 60;
constexpr int MAX_PLAYERS = 4;
constexpr int MAX_BALLS = 4;
constexpr int MAX_ENTITIES = MAX_PLAYERS + MAX_BALLS;
constexpr int GAME_SCALE = 1000;
constexpr int FIELD_SIZE = 600;

constexpr uint8_t PLAYER_COLORS[4 * MAX_PLAYERS] = {
    255, 0, 0, 255,
    0, 255, 0, 255,
    0, 0, 255, 255,
    255, 0, 255, 255,
};

struct Rect {
    int32_t hw, hh;
};

const Rect BOX_TYPES[3] = {
   {15 * GAME_SCALE, 60 * GAME_SCALE}, // p1/p2 paddle
   {60 * GAME_SCALE, 15 * GAME_SCALE}, // p3/p4 paddle
   {10 * GAME_SCALE, 10 * GAME_SCALE}, // ball
};

struct Gamestate {
    uint32_t frame = 0;
    uint8_t hp[MAX_PLAYERS];
    int32_t e_px[MAX_ENTITIES], e_py[MAX_ENTITIES]; // entity (center) position
    int32_t e_vx[MAX_ENTITIES], e_vy[MAX_ENTITIES]; // entity velocity
    struct Flags {
        uint8_t started: 1;
        uint8_t finished: 1;
        uint8_t players: 2; // 2 bits are enough to up to repesent 4 players
        uint8_t balls: 2; // 2 bits are enough to up to repesent 4 balls
    } flags = {};
};

static void init_game(Gamestate& state, int num_players) {
    state = {};
    state.flags.players = num_players - 1;
    state.flags.balls = 0;
    state.flags.started = true;
    state.flags.finished = false;
    // setup pedal positions
    for (int i = 0; i <= state.flags.players; i++) {
        if (i <= 1) {
            state.e_px[i] = (FIELD_SIZE * i) * GAME_SCALE;
            state.e_py[i] = FIELD_SIZE / 2 * GAME_SCALE;
        } else {
            state.e_px[i] = FIELD_SIZE / 2 * GAME_SCALE;
            state.e_py[i] = (FIELD_SIZE * (i - 2)) * GAME_SCALE;
        }
    }
}

static void draw_frame(SDL_Renderer* renderer, Gamestate& state) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    // draw balls in play
    // draw players
    for (int i = 0; i <= state.flags.players; i++) {
        const uint8_t* col = &PLAYER_COLORS[i * 4];
        SDL_SetRenderDrawColor(renderer, col[0], col[1], col[2], col[3]);
        Rect e_box = i <= 1 ? BOX_TYPES[0] : BOX_TYPES[1];
        float px = (float)(state.e_px[i] - e_box.hw) / GAME_SCALE;
        float py = (float)(state.e_py[i] - e_box.hh) / GAME_SCALE;
        float w = (float)(e_box.hw * 2) / GAME_SCALE;
        float h = (float)(e_box.hh * 2) / GAME_SCALE;
        SDL_FRect rect = { px, py, w, h };
        SDL_RenderFillRect(renderer, &rect);
    }
    // draw hud
    // bg
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_FRect hud_rect = { 0, FIELD_SIZE, FIELD_SIZE, FIELD_SIZE * 1.15 - FIELD_SIZE};
    SDL_RenderFillRect(renderer, &hud_rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &hud_rect);
    //
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
    SDL_Window* window = SDL_CreateWindow("SDL3 GekkoNet Example", FIELD_SIZE, FIELD_SIZE * 1.15, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    // timing
    const uint64_t frame_delay_ns = (1000000000 / TARGET_FPS); // ns/frame
    const uint64_t performance_frequency = SDL_GetPerformanceFrequency();
    uint64_t frame_start = 0, frame_time_ns = 0;
    //
    Gamestate gs = {};
    init_game(gs, 4);

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
