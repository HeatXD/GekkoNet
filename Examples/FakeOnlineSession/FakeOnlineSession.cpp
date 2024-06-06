#include <iostream>
#include <windows.h>
#include "SDL2/SDL.h"
#include "../../GekkoLib/gekko.h"
#include <chrono>

SDL_Window* window1 = nullptr;
SDL_Window* window2 = nullptr;

SDL_Renderer* renderer1 = nullptr;
SDL_Renderer* renderer2 = nullptr;

bool running = false;

bool init_windows(void) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "Error initializing SDL.\n");
		return false;
	}
	window1 = SDL_CreateWindow(
		"GekkoNet Example: Fake Online Session P1",
		0,
		50,
		400,
		400,
		0
	);

	window2 = SDL_CreateWindow(
		"GekkoNet Example: Fake Online Session P2",
		400,
		50,
		400,
		400,
		0
	);

	if (!window1 || !window2) {
		fprintf(stderr, "Error creating SDL Window.\n");
		return false;
	}

	renderer1 = SDL_CreateRenderer(window1, -1, 0);
	renderer2 = SDL_CreateRenderer(window2, -1, 0);

	if (!renderer1 || !renderer2) {
		fprintf(stderr, "Error creating SDL Renderer.\n");
		return false;
	}
	return true;
}

void del_windows(void) {
	SDL_DestroyRenderer(renderer1);
	SDL_DestroyRenderer(renderer2);
	SDL_DestroyWindow(window1);
	SDL_DestroyWindow(window2);
	SDL_Quit();
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

struct GInput {
	union inp {
		struct dir {
			char up : 1;
			char left : 1;
			char down : 1;
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
			break;
		}
	}
}

struct GState {
	int px[2] = {0, 0};
	int py[2] = {0, 0};
    int desync_test = 0; // changing this value for each state can trigger the desync system
};

void save_state(GState* gs, Gekko::GameEvent* ev) {
    *ev->data.save.state_len = sizeof(GState);
	*ev->data.save.checksum = fletcher32((uint16_t*)gs, sizeof(GState));
	std::memcpy(ev->data.save.state, gs, sizeof(GState));
}

void load_state(GState* gs, Gekko::GameEvent* ev) {
	std::memcpy(gs, ev->data.load.state, sizeof(GState));
}

void update_state(GState& gs, GInput inputs[2], int num_players) {
	for (int player = 0; player < num_players; player++) {
		if (inputs[player].input.dir.up) gs.py[player] -= 2;
		if (inputs[player].input.dir.down) gs.py[player] += 2;
		if (inputs[player].input.dir.left) gs.px[player] -= 2;
		if (inputs[player].input.dir.right) gs.px[player] += 2;
	}
}

void render_state(GState& gs, SDL_Renderer* renderer) {
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

void get_key_inputs(GInput inputs[2]) {
	// reset inputs
	inputs[0].input.value = 0;
	inputs[0].input.value = 0;
	// fetch inputs
	auto keys = SDL_GetKeyboardState(NULL);
	// P1
	inputs[0].input.dir.up = keys[SDL_SCANCODE_W];
	inputs[0].input.dir.left = keys[SDL_SCANCODE_A];
	inputs[0].input.dir.down = keys[SDL_SCANCODE_S];
	inputs[0].input.dir.right = keys[SDL_SCANCODE_D];
	// P2
	inputs[1].input.dir.up = keys[SDL_SCANCODE_UP];
	inputs[1].input.dir.left = keys[SDL_SCANCODE_LEFT];
	inputs[1].input.dir.down = keys[SDL_SCANCODE_DOWN];
	inputs[1].input.dir.right = keys[SDL_SCANCODE_RIGHT];
}

class FakeNetAdapter : public Gekko::NetAdapter {
	int recv_idx = 0;
	std::vector<std::unique_ptr<Gekko::NetData>> inbox_session1;
	std::vector<std::unique_ptr<Gekko::NetData>> inbox_session2;

	virtual std::vector<Gekko::NetData*> ReceiveData() {
		auto& curr_inbox = recv_idx % 2 == 1 ? inbox_session2 : inbox_session1;
		auto result = std::vector<Gekko::NetData*>();
		
		for (auto& ptr : curr_inbox) {
			result.push_back(ptr.release());
		}

		curr_inbox.clear();

		recv_idx++;
		return result;
	}

	virtual void SendData(Gekko::NetAddress& addr, Gekko::NetPacket& pkt) {
		auto data = std::unique_ptr<Gekko::NetData>(new Gekko::NetData);
		data->pkt = pkt;

		Gekko::u8 addr_from = (Gekko::u8)*addr.GetAddress() == 1 ? 2 : 1;

		auto tmp = Gekko::NetAddress(&addr_from, (Gekko::u32)sizeof(char));
		data->addr.Copy(&tmp);

		// be sure to copy input since it gets cleaned up later if not send.
		// normally you wouldnt need to do this. just sent it over the network instead.

		if (pkt.type == Gekko::SpectatorInputs || pkt.type == Gekko::Inputs) {
			data->pkt.type = pkt.type;
			data->pkt.magic = pkt.magic;

			data->pkt.x.input.input_count = pkt.x.input.input_count;
			data->pkt.x.input.start_frame = pkt.x.input.start_frame;
			data->pkt.x.input.total_size = pkt.x.input.total_size;
			data->pkt.x.input.inputs = (Gekko::u8*)std::malloc(pkt.x.input.total_size);
			
			if (data->pkt.x.input.inputs)
				std::memcpy(data->pkt.x.input.inputs, pkt.x.input.inputs, pkt.x.input.total_size);
		}
		else {
			data->pkt = pkt;
		}

		if (addr_from == 1){
			inbox_session2.push_back(std::move(data));
		} else {
			inbox_session1.push_back(std::move(data));
		}

		// printf("netaddr:%d sent pkt type:%d to netaddr:%d\n", addr_from, pkt.type, *addr.GetAddress());
	}
};

int main(int argc, char* args[])
{
	running = init_windows();

	GState state1 = {};
    // state1.desync_test = 22;
	GState state2 = {};
    // state2.desync_test = 11;
	GInput inputs[2] = {};

	auto adapter = FakeNetAdapter();

	int num_players = 2;

	auto sess1 = Gekko::Session();
	auto sess2 = Gekko::Session();

	auto conf = Gekko::Config();

	conf.num_players = num_players;
	conf.input_size = sizeof(char);
	conf.max_spectators = 0;
	conf.input_prediction_window = 8;
	conf.state_size = sizeof(GState);
	conf.limited_saving = false;
    // conf.desync_detection = true;

	sess1.Init(conf);
	sess2.Init(conf);

	sess1.SetNetAdapter(&adapter);
	sess2.SetNetAdapter(&adapter);

	char addrs1 = 1;
	auto addr1 = Gekko::NetAddress(&addrs1, sizeof(char));

	char addrs2 = 2;
	auto addr2 = Gekko::NetAddress(&addrs2, sizeof(char));

	auto s1p1 = sess1.AddActor(Gekko::PlayerType::LocalPlayer);
	auto s1p2 = sess1.AddActor(Gekko::PlayerType::RemotePlayer, &addr2);
	sess1.SetLocalDelay(s1p1, 1);

	auto s2p1 = sess2.AddActor(Gekko::PlayerType::RemotePlayer, &addr1);
	auto s2p2 = sess2.AddActor(Gekko::PlayerType::LocalPlayer);
	sess2.SetLocalDelay(s2p2, 1);

	// timing 
	using time_point = std::chrono::time_point<std::chrono::steady_clock>;
	using frame = std::chrono::duration<unsigned int, std::ratio<1, 60>>;
	using clock = std::chrono::steady_clock;

	time_point timer(clock::now());
	frame fps = {};

	while (running) {
		fps = std::chrono::duration_cast<frame>(clock::now() - timer);

		if (fps.count() > 0) {
			timer = clock::now();

			process_events();

			get_key_inputs(inputs);

			// add local inputs to the session
			sess1.AddLocalInput(s1p1, &inputs[0].input.value);
			sess2.AddLocalInput(s2p2, &inputs[1].input.value);

			int frame = 0;

            for (auto event : sess1.Events()) {
                printf("S1 EV: %d\n", event->type);
                if (event->type == 6) {
                    auto desync = event->data.desynced;
                    printf("desync detected, f:%d, rh:%d, lc:%u, rc:%u\n", desync.frame, desync.remote_handle, desync.local_checksum, desync.remote_checksum);
                }
            }

            for (auto ev : sess1.UpdateSession())
            {
                switch (ev->type)
                {
                case Gekko::SaveEvent:
                    printf("S1 Save frame:%d\n", ev->data.save.frame);
                    save_state(&state1, ev);
                    break;
                case Gekko::LoadEvent:
                    printf("S1 Load frame:%d\n", ev->data.load.frame);
                    load_state(&state1, ev);
                    break;
                case Gekko::AdvanceEvent:
                    // on advance event, advance the gamestate using the given inputs
                    inputs[0].input.value = ev->data.adv.inputs[0];
                    inputs[1].input.value = ev->data.adv.inputs[1];
                    frame = ev->data.adv.frame;
                    printf("S1, F:%d, P1:%d P2:%d\n", frame, inputs[0].input.value, inputs[1].input.value);
                    // now we can use them to update state.
                    update_state(state1, inputs, num_players);
                    break;
                default:
                    printf("S1 Unkown Event: %d\n", ev->type);
                    break;
                }
            }

            for (auto event : sess2.Events()) {
                printf("S2 EV: %d\n", event->type);
                if (event->type == 6) {
                    auto desync = event->data.desynced;
                    printf("desync detected, f:%d, rh:%d, lc:%u, rc:%u\n", desync.frame, desync.remote_handle, desync.local_checksum, desync.remote_checksum);
                }
            }

            for (auto ev : sess2.UpdateSession())
            {
                switch (ev->type)
                {
                case Gekko::SaveEvent:
                    printf("S2 Save frame:%d\n", ev->data.save.frame);
                    save_state(&state2, ev);
                    break;
                case Gekko::LoadEvent:
                    printf("S2 Load frame:%d\n", ev->data.load.frame);
                    load_state(&state2, ev);
                    break;
                case Gekko::AdvanceEvent:
                    // on advance event, advance the gamestate using the given inputs
                    inputs[0].input.value = ev->data.adv.inputs[0];
                    inputs[1].input.value = ev->data.adv.inputs[1];
                    frame = ev->data.adv.frame;
                    printf("S2, F:%d, P1:%d P2:%d\n", frame, inputs[0].input.value, inputs[1].input.value);
                    // now we can use them to update state.
                    update_state(state2, inputs, num_players);
                    break;
                default:
                    printf("S2 Unkown Event: %d\n", ev->type);
                    break;
                }
            }
		}

		// draw the state every iteration
		SDL_SetRenderDrawColor(renderer1, 0, 0, 0, 255);
		SDL_SetRenderDrawColor(renderer2, 0, 0, 0, 255);

		SDL_RenderClear(renderer1);
		SDL_RenderClear(renderer2);

		render_state(state1, renderer1);
		render_state(state2, renderer2);

		SDL_RenderPresent(renderer1);
		SDL_RenderPresent(renderer2);
	}
	del_windows();

	return 0;
}
