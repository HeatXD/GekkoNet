#include "gekko.h"
#include <iostream>
#include "backend.h"

void Gekko::Session::Test()
{
	std::cout << "Hello Gekko!\n";
}

Gekko::Session::Session()
{
	_host = nullptr;
	_input_size = 0;
	_max_spectators = 0;
	_num_players = 0;
	_started = false;
	_num_players = 0;
}

void Gekko::Session::SetNetAdapter(NetAdapter* adapter)
{
	_host = adapter;
}

void Gekko::Session::Init(u8 num_players, u8 max_spectators, u32 input_size)
{
	_host = nullptr;
	_started = false;

	_num_players = num_players;
	_max_spectators = max_spectators;
	_input_size = input_size;

	// setup input queues for the players
	_sync.Init(_num_players, _input_size, 0);
}

void Gekko::Session::SetLocalDelay(Handle player, u8 delay)
{
	if (player - 1 >= 0 && _players[player].GetType() == Local) {
		_sync.SetLocalDelay(player, delay);
	}
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
	Poll();
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

void Gekko::Session::Poll()
{
	// return if no host is defined.
	if (!_host)
		return;

	auto msg = _host->ReceiveMessages();

	UpdatePlayerStatus();
}

void Gekko::Session::UpdatePlayerStatus()
{
	// TODO
}

bool Gekko::Session::AllPlayersValid()
{
	if (!_started) {
		for (int i = 0; i < _num_players; i++) {
			if (_players[i].GetStatus() == Initiating) {
				return false;
			}
		}

		// on session start we care that all spectators start with the players.
		// when the session started and someone wants to join midway 
		// we should send an initiation packet with state.
		for (int i = 0; i < _spectators.size(); i++) {
			if (_spectators[i].GetStatus() == Initiating) {
				return false;
			}
		}

		// if none returned that the session is ready!
		_started = true;
	}
	return true;
}
