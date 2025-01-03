#include "SDL2/SDL.h"

#define GEKKONET_STATIC

#include "gekkonet.h"

#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <thread>

// Define grid dimensions
#define GRID_COLUMNS 10
#define GRID_ROWS 10
#define WINDOW_WIDTH 750
#define WINDOW_HEIGHT 750

// Calculate cell dimensions
#define CELL_WIDTH (WINDOW_WIDTH / GRID_COLUMNS)
#define CELL_HEIGHT (WINDOW_HEIGHT / GRID_ROWS)

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
bool running = false;

using micro = std::chrono::microseconds;
using gduration = std::chrono::duration<unsigned int, std::ratio<1, 60>>;
using gtime_point = std::chrono::time_point<std::chrono::steady_clock>;
using slow_frame = std::chrono::duration<unsigned int, std::ratio<1, 58>>;
using normal_frame = std::chrono::duration<unsigned int, std::ratio<1, 60>>;
using fast_frame = std::chrono::duration<unsigned int, std::ratio<1, 62>>;
using gclock = std::chrono::steady_clock;

bool init_window(void) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "Error initializing SDL.\n");
        return false;
    }
    window = SDL_CreateWindow(
        "GekkoNet Example: Online Session",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0
    );
    if (!window) {
        fprintf(stderr, "Error creating SDL Window.\n");
        return false;
    }
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "Error creating SDL Renderer.\n");
        return false;
    }
    return true;
}

void del_window(void) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

struct GInput {
    union i {
        struct dir {
            char up : 1;
            char down : 1;
            char left : 1;
            char right : 1;
        }dir;
        uint8_t value;
    }input;
    struct m {
        union a {
            struct c {
                char left : 1;
                char right : 1;
            } clicked;
            uint8_t value;
        } action;
        union g {
            struct pos {
                uint16_t x;
                uint16_t y;
            } pos;
            uint32_t value;
        } grid;
    } mouse;
};

void process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            break;
        }
    }
}

struct GState {
    int px[2]{ 0, 0 };
    int py[2]{ 0, 0 };
    uint16_t mouse_px[2]{ 0, 0 };
    uint16_t mouse_py[2]{ 0, 0 };
    char desync = 0;
};

void update_state(GState& gs, GInput inputs[2], int num_players) {
    for (int player = 0; player < num_players; player++) {
        if (inputs[player].input.dir.up) gs.py[player] -= 2;
        if (inputs[player].input.dir.down) gs.py[player] += 2;
        if (inputs[player].input.dir.left) gs.px[player] -= 2;
        if (inputs[player].input.dir.right)gs.px[player] += 2;

        // set mouse pos
        if (inputs[player].mouse.action.value != 0) {
            gs.mouse_px[player] = inputs[player].mouse.grid.pos.x;
            gs.mouse_py[player] = inputs[player].mouse.grid.pos.y;
        }
    }
}

// https://en.wikipedia.org/wiki/Fletcher%27s_checksum
uint32_t fletcher32(const uint16_t* data, size_t len) {
    uint32_t c0, c1;
    len = (len + 1) & ~1;      /* Round up len to words */

    /* We similarly solve for n > 0 and n * (n+1) / 2 * (2^16-1) < (2^32-1) here. */
    /* On modern computers, using a 64-bit c0/c1 could allow a group size of 23726746. */
    for (c0 = c1 = 0; len > 0; ) {
        size_t blocklen = len;
        if (blocklen > 360 * 2) {
            blocklen = 360 * 2;
        }
        len -= blocklen;
        do {
            c0 = c0 + *data++;
            c1 = c1 + c0;
        } while ((blocklen -= 2));
        c0 = c0 % 65535;
        c1 = c1 % 65535;
    }
    return (c1 << 16 | c0);
}

void save_state(GState* gs, GekkoGameEvent* ev) {
    *ev->data.save.state_len = sizeof(GState);
    *ev->data.save.checksum = fletcher32((uint16_t*)gs, sizeof(GState));
    std::memcpy(ev->data.save.state, gs, sizeof(GState));
}

void load_state(GState* gs, GekkoGameEvent* ev) {
    std::memcpy(gs, ev->data.load.state, sizeof(GState));
}

void render_state(GState& gs) {
    // render grid
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
    for (int i = 1; i < GRID_COLUMNS; ++i) {
        int x = i * CELL_WIDTH;
        SDL_RenderDrawLine(renderer, x, 0, x, WINDOW_HEIGHT);
    }

    for (int i = 1; i < GRID_ROWS; ++i) {
        int y = i * CELL_HEIGHT;
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }

    // render player
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int player = 0; player < 2; player++)
    {
        SDL_Rect player_rect = {
            gs.px[player],
            gs.py[player],
            40,
            40
        };
        SDL_RenderFillRect(renderer, &player_rect);
    }

    // render mouse position
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int player = 0; player < 2; player++)
    {
        SDL_Rect mouse_rect = {
            gs.mouse_px[player],
            gs.mouse_py[player],
            5,
            5
        };
        SDL_RenderFillRect(renderer, &mouse_rect);
    }
}

GInput get_key_inputs() {
    GInput input{};
    // reset inputs
    input.input.value = 0;
    input.mouse.action.value = 0;
    input.mouse.grid.value = 0;
    // fetch local inputs
    auto keys = SDL_GetKeyboardState(NULL);
    // local player
    input.input.dir.up = keys[SDL_SCANCODE_W];
    input.input.dir.left = keys[SDL_SCANCODE_A];
    input.input.dir.down = keys[SDL_SCANCODE_S];
    input.input.dir.right = keys[SDL_SCANCODE_D];
    // get mouse state
    int mouse_x, mouse_y;
    auto buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    // mouse action
    input.mouse.action.clicked.left = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    input.mouse.action.clicked.right = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    // local mouse position
    // only send position if there is a mouse action detected.
    // otherwise we are constantly send new values for the mouse which causes it to rollback almost every frame.
    if (input.mouse.action.value != 0) {
        input.mouse.grid.pos.x = mouse_x;
        input.mouse.grid.pos.y = mouse_y;
    }
    return input;
}

micro GetFrameTime(float frames_ahead) {
    if (frames_ahead >= 1.0f) {
        return std::chrono::duration_cast<micro>(slow_frame(1));
    }
    else if (frames_ahead <= -1.0f) {
        return std::chrono::duration_cast<micro>(fast_frame(1));
    }
    else {
        return std::chrono::duration_cast<micro>(normal_frame(1));
    }

}

int main(int argc, char* args[])
{
    std::vector<std::string> all_args;

    if (argc > 1) {
        all_args.assign(args + 1, args + argc);
    }

    if (all_args.size() != 3) {
        printf("args found = %d\n", (int)all_args.size());
        return 0;
    }

    int localplayer = std::stoi(all_args[0]);
    const int localport = std::stoi(all_args[1]);
    std::string remote_address = std::move(all_args[2]);

    printf("lplayer:%d, lport:%d, rem_addr:%s\n", localplayer, localport, remote_address.c_str());

    running = init_window();

    GState state = {};
    // state.desync = localplayer;

    GInput inputs[2] = {};

    int num_players = 2;

    GekkoSession* sess = nullptr;
    GekkoConfig conf{};

    conf.num_players = num_players;
    conf.input_size = sizeof(GInput);
    conf.state_size = sizeof(GState);
    conf.max_spectators = 0;
    conf.input_prediction_window = 10;
    // conf.desync_detection = true;
    conf.limited_saving = true;

    gekko_create(&sess);
    gekko_start(sess, &conf);
    gekko_net_adapter_set(sess, gekko_default_adapter(localport));

    // this is order dependant so we have to keep that in mind. 
    if (localplayer == 0) {
        // add local player
        localplayer = gekko_add_actor(sess, LocalPlayer, nullptr);
        // add remote player
        auto remote = GekkoNetAddress{ (void*)remote_address.c_str(), (unsigned int)remote_address.size() };
        gekko_add_actor(sess, RemotePlayer, &remote);
    }
    else {
        // add remote player
        auto remote = GekkoNetAddress{ (void*)remote_address.c_str(), (unsigned int)remote_address.size() };
        gekko_add_actor(sess, RemotePlayer, &remote);
        // add local player
        localplayer = gekko_add_actor(sess, LocalPlayer, nullptr);
    }

    gekko_set_local_delay(sess, localplayer, 1);
    // timing

    gtime_point start_time(gclock::now());
    gtime_point end_time(gclock::now());

    int current = 0;

    while (running) {
        start_time = gclock::now();

        auto frame_time = GetFrameTime(gekko_frames_ahead(sess));

        // std::cout << "ft: " << frame_time.count() << std::endl;

        process_events();

        //add local inputs to the session
        auto input = get_key_inputs();
        gekko_add_local_input(sess, localplayer, &input);

        int count = 0;
        auto events = gekko_session_events(sess, &count);
        for (int i = 0; i < count; i++) {
            auto event = events[i];

            printf("EV: %d\n", event->type);

            if (event->type == DesyncDetected) {
                auto desync = event->data.desynced;
                printf("desync detected, f:%d, rh:%d, lc:%u, rc:%u\n", desync.frame, desync.remote_handle, desync.local_checksum, desync.remote_checksum);
            }

            if (event->type == PlayerDisconnected) {
                auto disco = event->data.disconnected;
                printf("disconnect detected, player: %d\n", disco.handle);
            }
        }

        count = 0;
        auto updates = gekko_update_session(sess, &count);
        for (int i = 0; i < count; i++) {
            auto ev = updates[i];

            switch (ev->type) {
            case SaveEvent:
                printf("Save frame:%d\n", ev->data.save.frame);
                save_state(&state, ev);
                break;
            case LoadEvent:
                printf("Load frame:%d\n", ev->data.load.frame);
                load_state(&state, ev);
                break;
            case AdvanceEvent:
                // on advance event, advance the gamestate using the given inputs
                memcpy(&inputs[0], &ev->data.adv.inputs[0], conf.input_size);
                memcpy(&inputs[1], &ev->data.adv.inputs[conf.input_size], conf.input_size);
                current = ev->data.adv.frame;
                printf("F:%d, P1:%d P2:%d\n", current, inputs[0].input.value, inputs[1].input.value);
                // now we can use them to update state.
                update_state(state, inputs, num_players);
                break;
            default:
                printf("Unknown Event: %d\n", ev->type);
                break;
            }
        }

        // draw the state every iteration
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        render_state(state);
        SDL_RenderPresent(renderer);

        end_time = gclock::now();

        auto work_time = std::chrono::duration_cast<micro>(end_time - start_time);
        // std::cout << "wt: " << work_time.count() << std::endl;

        auto adjusted_sleep = frame_time - work_time;
        // std::cout << "st: " << adjusted_sleep.count() << std::endl;

        if (adjusted_sleep.count() > 0) {
            std::this_thread::sleep_for(adjusted_sleep);
        }
    }

    del_window();

    gekko_destroy(sess);

    return 0;
}
