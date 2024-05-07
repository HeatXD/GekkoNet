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
		"GekkoNet Example: Fake Online Session 1",
		0,
		50,
		400,
		400,
		0
	);

	window2 = SDL_CreateWindow(
		"GekkoNet Example: Fake Online Session 2",
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

struct GInput {
	union inp {
		struct dir {
			char up : 1;
			char left : 1;
			char down : 1;
			char right : 1;
		}dir;
		unsigned short int value;
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
};

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

		if (recv_idx % 2 == 1) {
			inbox_session2.clear();
		} else {
			inbox_session1.clear();
		}

		recv_idx++;
		return result;
	}

	virtual void SendData(Gekko::NetAddress& addr, Gekko::NetPacket& pkt) {
		auto data = std::unique_ptr<Gekko::NetData>(new Gekko::NetData);
		data->pkt = pkt;

		Gekko::u8 addr_from = 0;
		if ((Gekko::u8)*addr.GetAddress() == 1) addr_from = 2; else addr_from = 1;

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
	GState state2 = {};

	GInput inputs[2] = {};

	Gekko::Session::Test();

	auto adapter = FakeNetAdapter();

	int num_players = 2;

	auto sess1 = Gekko::Session();
	auto sess2 = Gekko::Session();

	sess1.Init(num_players, 0, sizeof(short int));
	sess2.Init(num_players, 0, sizeof(short int));

	sess1.SetNetAdapter(&adapter);
	sess2.SetNetAdapter(&adapter);

	char addrs1 = 1;
	auto addr1 = Gekko::NetAddress((Gekko::u8*)&addrs1, sizeof(char));

	char addrs2 = 2;
	auto addr2 = Gekko::NetAddress((Gekko::u8*)&addrs2, sizeof(char));

	auto s1p1 = sess1.AddActor(Gekko::PlayerType::LocalPlayer);
	auto s1p2 = sess1.AddActor(Gekko::PlayerType::RemotePlayer, &addr2);
	// sess1.SetLocalDelay(s1p1, 3);

	auto s2p1 = sess2.AddActor(Gekko::PlayerType::RemotePlayer, &addr1);
	auto s2p2 = sess2.AddActor(Gekko::PlayerType::LocalPlayer);
	// sess2.SetLocalDelay(s2p2, 3);

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

			//add local inputs to the session
			sess1.AddLocalInput(s1p1, &inputs[0].input.value);
			sess2.AddLocalInput(s2p2, &inputs[1].input.value);

			int frame = 0;

			auto ev1 = sess1.UpdateSession();
			for (int i = 0; i < ev1.size(); i++)
			{
				switch (ev1[i].type)
				{
				case Gekko::AdvanceEvent:
					// on advance event, advance the gamestate using the given inputs
					inputs[0].input.value = ev1[i].data.ev.adv.inputs[0];
					inputs[1].input.value = ev1[i].data.ev.adv.inputs[2];
					frame = ev1[i].data.ev.adv.frame;
					printf("S1, F:%d, P1:%d P2:%d\n", frame, inputs[0].input.value, inputs[1].input.value);
					// be sure to free the inputs when u have used or collected them.
					std::free(ev1[i].data.ev.adv.inputs);
					// now we can use them to update state.
					update_state(state1, inputs, num_players);
					break;
				default:
					break;
				}
			}

			auto ev2 = sess2.UpdateSession();
			for (int i = 0; i < ev2.size(); i++)
			{
				switch (ev2[i].type)
				{
				case Gekko::AdvanceEvent:
					// on advance event, advance the gamestate using the given inputs
					inputs[0].input.value = ev2[i].data.ev.adv.inputs[0];
					inputs[1].input.value = ev2[i].data.ev.adv.inputs[2];
					frame = ev2[i].data.ev.adv.frame;
					printf("S2, F:%d, P1:%d P2:%d\n", frame, inputs[0].input.value, inputs[1].input.value);
					// be sure to free the inputs when u have used or collected them.
					std::free(ev2[i].data.ev.adv.inputs);
					// now we can use them to update state.
					update_state(state2, inputs, num_players);
					break;
				default:
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