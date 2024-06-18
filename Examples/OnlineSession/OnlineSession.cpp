#include <iostream>
#include "SDL2/SDL.h"
#include <gekko.h>
#include <chrono>
#include <string>

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

void process_events(Gekko::Session& sess) {
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

            if (event.key.keysym.sym == SDLK_q) {
                break;
            }

            if (event.key.keysym.sym == SDLK_e) {
                break;
            }
        }
    }
}

struct GState {
    int px[2]{ 0, 0};
    int py[2]{ 0, 0};
    char desync = 0;
};

void update_state(GState& gs, GInput inputs[2], int num_players) {
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

void save_state(GState* gs, Gekko::GameEvent* ev) {
    *ev->data.save.state_len = sizeof(GState);
    *ev->data.save.checksum = fletcher32((uint16_t*)gs, sizeof(GState));
    std::memcpy(ev->data.save.state, gs, sizeof(GState));
}

void load_state(GState* gs, Gekko::GameEvent* ev) {
    std::memcpy(gs, ev->data.load.state, sizeof(GState));
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

    auto sess = Gekko::Session();
    auto conf = Gekko::Config();

    conf.num_players = num_players;
    conf.input_size = sizeof(char);
    conf.state_size = sizeof(GState);
    conf.max_spectators = 0;
    conf.input_prediction_window = 10;
    conf.desync_detection = true;
    // conf.limited_saving = true;

    sess.Init(conf);

    auto sock = Gekko::NonBlockingSocket(localport);
    sess.SetNetAdapter(&sock);

    // this is order dependant so we have to keep that in mind. 
    if (localplayer == 1) {
        // add local player
        localplayer = sess.AddActor(Gekko::PlayerType::LocalPlayer);
        // add remote player
        auto remote = Gekko::NetAddress((void*)remote_address.c_str(), (Gekko::u32)remote_address.size());
        sess.AddActor(Gekko::PlayerType::RemotePlayer, &remote);
    } else {
        // add remote player
        auto remote = Gekko::NetAddress((void*)remote_address.c_str(), (Gekko::u32)remote_address.size());
        sess.AddActor(Gekko::PlayerType::RemotePlayer, &remote);
        // add local player
        localplayer = sess.AddActor(Gekko::PlayerType::LocalPlayer);
    }

    sess.SetLocalDelay(localplayer, 5);
    // timing

    gtime_point start_time(gclock::now());
    gtime_point end_time(gclock::now());

    int current = 0;

    while (running) {
        start_time = gclock::now();

        auto frame_time = GetFrameTime(sess.FramesAhead());

        // std::cout << "ft: " << frame_time.count() << std::endl;

        process_events(sess);

        //add local inputs to the session
        auto input = get_key_inputs();
        sess.AddLocalInput(localplayer, &input);

        for (auto event : sess.Events()) {
            printf("EV: %d\n", event->type);
            if (event->type == 6) {
                auto desync = event->data.desynced;
                printf("desync detected, f:%d, rh:%d, lc:%u, rc:%u\n", desync.frame, desync.remote_handle, desync.local_checksum, desync.remote_checksum);
            }
        }

        for (auto ev : sess.UpdateSession()) {
            switch (ev->type) {
            case Gekko::SaveEvent:
                printf("Save frame:%d\n", ev->data.save.frame);
                save_state(&state, ev);
                break;
            case Gekko::LoadEvent:
                printf("Load frame:%d\n", ev->data.load.frame);
                load_state(&state, ev);
                break;
            case Gekko::AdvanceEvent:
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

    return 0;
}
