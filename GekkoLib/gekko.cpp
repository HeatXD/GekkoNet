#include "gekko.h"
#include <iostream>

void Gekko::Session::Test()
{
	std::cout << "Hello Gekko!\n";
}

void Gekko::Session::SetNetAdapter(NetAdapter* adapter)
{
	_host = adapter;
}

void Gekko::Session::Init(u8 num_players, u8 max_spectators, u8 input_delay, u32 input_size)
{
	_host = nullptr;
	_started = false;

	_num_players = num_players;
	_max_spectators = max_spectators;
	_local_delay = input_delay;
	_input_size = input_size;

	// setup input queues for the players
	_sync.Init(_num_players, _input_size, _local_delay);
}

void Gekko::Session::SetLocalDelay(u8 delay)
{
	_local_delay = delay;
	// TODO update local input queues to adjust for the new delay
}

Gekko::Handle Gekko::Session::AddPlayer(PlayerType type, NetAddress addr)
{
	if (type == PlayerType::Spectator) {
		if (_spectators.size() >= _max_spectators)
			return 0;

		_spectators.push_back(Player(type, addr));
		return _num_players + (u32)_spectators.size();
	}
	else {
		if (_started || _players.size() >= _num_players)
			return 0;

		_players.push_back(Player(type, addr));
		return (u32)_players.size();
	}
}

void Gekko::Session::AddLocalInput(Handle player, Input input)
{
	_sync.AddLocalInput(player, input);
}

std::vector<Gekko::Event> Gekko::Session::UpdateSession()
{
	auto ev = std::vector<Event>();
	// Connection Handling
	UpdatePlayerStatus();
	// Gameplay
	if (AllPlayersValid()) {
		// check whether we have all the inputs needed to proceed
		Frame frame = 0;
		std::unique_ptr<u8[]> inputs;
		if (!_sync.GetCurrentInputs(inputs, frame))
			return ev;

		// we have the inputs lets create an event for the user
		// to advance with the given inputs
		Event event;
		event.type = AdvanceEvent;
		event.data.ev.adv.frame = frame;
		event.data.ev.adv.input_len = _num_players * _input_size;
		event.data.ev.adv.inputs = (u8*) std::malloc(event.data.ev.adv.input_len);

		if (event.data.ev.adv.inputs)
			std::memcpy(event.data.ev.adv.inputs, inputs.get(), event.data.ev.adv.input_len);
		
		ev.push_back(event);

		// then advance the session
		_sync.IncrementFrame();
	}
	return ev;
}

void Gekko::Session::UpdatePlayerStatus()
{
	// TODO
}

bool Gekko::Session::AllPlayersValid()
{
	// TODO
	return true;
}
