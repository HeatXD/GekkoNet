#include "session/replay_session.h"

#include <cstdio>
#include <cstring>

Gekko::ReplaySession::ReplaySession()
{
    _started = false;
    _finished = false;
    _desynced = false;
    _checksum_pending = false;
    _recorded_checksum = 0;
    _inputs = nullptr;
    _config = GekkoConfig();
}

void Gekko::ReplaySession::Init(GekkoConfig* config)
{
    // no-op: a replay session is configured by the replay it loads.
}

GekkoGameEvent** Gekko::ReplaySession::UpdateSession(i32* count)
{
    _session_events.Reset();

    _game_events.Clear();

    if (_replay.IsReplaying()) {
        _game_events.Reset();

        if (!_started) {
            _started = true;
            _session_events.AddSessionStartedEvent();
            AddInitialStateLoad();
        }

        // the game handled the save of the previous frame by now.
        VerifyChecksum();

        if (AddNextReplayInputs()) {
            if (_game_events.AddAdvanceEvent(_sync, false)) {
                AddChecksumSave(_sync.GetCurrentFrame());
                _sync.IncrementFrame();
            }
        }
        else if (!_finished) {
            _finished = true;
            _session_events.AddReplayFinishedEvent();
        }
    }

    *count = _game_events.Count();
    return _game_events.Data();
}

GekkoSessionEvent** Gekko::ReplaySession::Events(i32* count)
{
    *count = (i32)_session_events.GetRecentEvents().size();
    return _session_events.GetRecentEvents().data();
}

bool Gekko::ReplaySession::LoadReplay(const u8* replay_data, u32 length)
{
    if (!_replay.LoadReplay(replay_data, length)) {
        return false;
    }

    _config = _replay.Config();

    _started = false;
    _finished = false;
    _desynced = false;
    _checksum_pending = false;
    _recorded_checksum = 0;

    try {
        _sync.Init(_config.num_players, _config.input_size);

        _game_events.Init(_config.input_size * _config.num_players);

        _inputs = std::make_unique<u8[]>(_config.input_size * _config.num_players);

        _checksum_state = StateEntry();
        _checksum_state.state = std::make_unique<u8[]>(_config.state_size);
        _checksum_state.state_len = _config.state_size;
    }
    catch (...) {
        printf("replay does not fit in memory\n");
        _replay.Reset();
        return false;
    }

    _session_events.Reset();

    return true;
}

void Gekko::ReplaySession::AddInitialStateLoad()
{
    const u8* state = _replay.ReplayState();

    if (!state) {
        return;
    }

    _game_events.AddStateLoadEvent(_sync.GetCurrentFrame() - 1, (u8*)state, _config.state_size);
}

bool Gekko::ReplaySession::AddNextReplayInputs()
{
    if (!_inputs || !_replay.NextReplayInput(_inputs.get())) {
        return false;
    }

    const Frame frame = _sync.GetCurrentFrame();

    for (u8 player = 0; player < _config.num_players; player++) {
        _sync.AddRemoteInput(player, _inputs.get() + (player * _config.input_size), frame);
    }

    return true;
}

const u8* Gekko::ReplaySession::ReplayUserData(u32& length)
{
    return _replay.UserData(length);
}

void Gekko::ReplaySession::AddChecksumSave(Frame frame)
{
    if (_desynced || !_replay.RecordedChecksum(frame, _recorded_checksum)) {
        return;
    }

    _game_events.AddStateSaveEvent(frame, &_checksum_state);
    _checksum_pending = true;
}

void Gekko::ReplaySession::VerifyChecksum()
{
    if (!_checksum_pending) {
        return;
    }

    _checksum_pending = false;

    if (_checksum_state.checksum != _recorded_checksum) {
        // only the first mismatch is reported, every later frame follows from it.
        _desynced = true;
        _session_events.AddReplayDesyncEvent(_checksum_state.frame, _checksum_state.checksum, _recorded_checksum);
    }
}
