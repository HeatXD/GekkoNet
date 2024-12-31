#include "SDL2/SDL.h"

#define GEKKONET_STATIC

#include "gekkonet.h"

#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <unordered_map>

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

const int MAX_ROLLBACK_WINDOW = 8;

bool init_window(void) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "Error initializing SDL.\n");
        return false;
    }
    window = SDL_CreateWindow(
        "GekkoNet Example: Online Session",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        300,
        300,
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
    union inp {
        struct dir {
            char up : 1;
            char down : 1;
            char left : 1;
            char right : 1;
        }dir;
        unsigned char value;
    }input;
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
                break;
            }
        }
    }
}

struct GState {
    int px[2]{ 0, 0};
    int py[2]{ 0, 0};
    char desync = 0;
    int framenumber = 0;
};

// gamestate handling
int most_recent_savestate_frame = 0;
std::unordered_map<int, std::unique_ptr<GState>> gamestates;

void update_state(GState& gs, GInput inputs[2], int num_players) {
    // advance frame number
    gs.framenumber++;
    // update game
    for (int player = 0; player < num_players; player++) {
        if (inputs[player].input.dir.up) gs.py[player] -= 2;
        if (inputs[player].input.dir.down) gs.py[player] += 2;
        if (inputs[player].input.dir.left) gs.px[player] -= 2;
        if (inputs[player].input.dir.right)gs.px[player] += 2;
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

void SaveGameState(GState* gs) {
    // save the gamestate how you do this depends on your implementation.
    
    // state doesnt exist? well then create it.
    if (gamestates.count(gs->framenumber) == 0) {
        gamestates[gs->framenumber] = std::make_unique<GState>();
    }

    // copy to save it
    std::memcpy(gamestates[gs->framenumber].get(), gs, sizeof(GState));

    // savestate cleanup
    if (gs->framenumber > most_recent_savestate_frame) {
        most_recent_savestate_frame = gs->framenumber;

        for (auto it = gamestates.begin(); it != gamestates.end();) {
            // we add 2 because gekkonet relies on that buffer to handle rollbacks internally.
            // good to keep that standard.
            if (it->first < most_recent_savestate_frame - (MAX_ROLLBACK_WINDOW + 2)) {
                it = gamestates.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

void save_state(GState* gs, GekkoGameEvent* ev) {
    // pass framenumber to gekkonet
    *ev->data.save.state_len = sizeof(int);
    *ev->data.save.checksum = fletcher32((uint16_t*)&gs->framenumber, sizeof(int));
    std::memcpy(ev->data.save.state, &gs->framenumber, sizeof(int));
    // handle saving ourselves
    SaveGameState(gs);
}

void LoadGameState(int frame, GState* gs) {
    if (gamestates.count(frame) == 0) {
        // cant load a state that doesnt exist..
        return;
    }
    // load the gamestate
    // how to handle this depends on your implementation
    std::memcpy(gs, gamestates[frame].get(), sizeof(GState));
}

void load_state(GState* gs, GekkoGameEvent* ev) {
    // get the framenumber
    int frame_to_load = 0;
    std::memcpy(&frame_to_load, ev->data.load.state, sizeof(int));
    // load the frame ourselves
    LoadGameState(frame_to_load, gs);
}

void render_state(GState& gs) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int player = 0; player < 2; player++)
    {
        SDL_Rect ball_rect = {
            gs.px[player],
            gs.py[player],
            40,
            40
        };
        SDL_RenderFillRect(renderer, &ball_rect);
    }
}

GInput get_key_inputs() {
    GInput input{};
    // reset inputs
    input.input.value = 0;
    // fetch local inputs
    auto keys = SDL_GetKeyboardState(NULL);
    // local player
    input.input.dir.up = keys[SDL_SCANCODE_W];
    input.input.dir.left = keys[SDL_SCANCODE_A];
    input.input.dir.down = keys[SDL_SCANCODE_S];
    input.input.dir.right = keys[SDL_SCANCODE_D];
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
    GekkoConfig conf {};

    conf.num_players = num_players;
    conf.input_size = sizeof(char);

    // instead of passing state we are passing the frame number and handling saving/loading ourselves.
    conf.state_size = sizeof(int);

    conf.max_spectators = 0;
    conf.input_prediction_window = MAX_ROLLBACK_WINDOW;
    conf.desync_detection = true;
    // conf.limited_saving = true;

    gekko_create(&sess);
    gekko_start(sess, &conf);
    gekko_net_adapter_set(sess, gekko_default_adapter(localport));

    // this is order dependant so we have to keep that in mind. 
    if (localplayer == 1) {
        // add local player
        localplayer = gekko_add_actor(sess, LocalPlayer, nullptr); 
        // add remote player
        auto remote = GekkoNetAddress{ (void*)remote_address.c_str(), (unsigned int)remote_address.size() };
        gekko_add_actor(sess, RemotePlayer, &remote);
    } else {
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

        std::cout << "ft: " << frame_time.count() << std::endl;

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
                inputs[0].input.value = ev->data.adv.inputs[0];
                inputs[1].input.value = ev->data.adv.inputs[1];
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
