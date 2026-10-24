#include "replay.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "compression.h"
#include "zpp/zpp_bits.h"

bool Gekko::ReplaySystem::StartRecording(GekkoConfig config, Frame frame, bool save_state, bool disable_compression)
{
    Reset();

    _replay.config = config;

    _no_compression = disable_compression;

    if (InputSize() == 0) {
        Reset();
        return false;
    }

    _start_frame = frame;
    _last_recorded_frame = frame - 1;
    _last_checksum_frame = frame - 1;

    _needs_state = save_state && config.state_size > 0;

    if (_needs_state) {
        _state.state = std::make_unique<u8[]>(config.state_size);
    }

    _mode = Recording;

    return true;
}

const u8* Gekko::ReplaySystem::StopRecording(u32& length)
{
    length = 0;

    if (_mode != Recording) return nullptr;

    const u8* data = Serialize(length);

    _mode = None;

    return data;
}

const u8* Gekko::ReplaySystem::PeekRecording(u32& length)
{
    length = 0;

    if (_mode != Recording) return nullptr;

    return Serialize(length);
}

const u8* Gekko::ReplaySystem::Serialize(u32& length)
{
    length = 0;

    RecordPendingState();

    const u32 block = InputSize();

    // a checksum can only be verified for a frame the replay holds the inputs of,
    // keep later ones aside while serializing.
    const u64 recorded_frames = _replay.inputs.size() / block;
    size_t kept = _replay.checksums.size();
    while (kept > 0 && (u64)_replay.checksums[kept - 1].frame >= recorded_frames) {
        kept--;
    }
    std::vector<ReplayChecksum> later(_replay.checksums.begin() + kept, _replay.checksums.end());
    _replay.checksums.resize(kept);

    std::vector<u8> packed;
    if (!_no_compression && block > 0 && !_replay.inputs.empty()) {
        auto delta = Compression::DeltaEncode(_replay.inputs.data(), (u32)_replay.inputs.size(), block);
        packed = Compression::RLEEncode(delta.data(), (u32)delta.size());
    }

    const bool use_packed = !packed.empty() && packed.size() < _replay.inputs.size();
    if (use_packed) {
        _replay.inputs.swap(packed);
        _replay.compressed = true;
    }

    _bin_buffer.clear();
    zpp::bits::out out(_bin_buffer);
    const bool bad = failure(out(
        _replay.header,
        _replay.version,
        _replay.compressed,
        _replay.config,
        _replay.inputs,
        _replay.initial_state,
        _replay.checksum_interval,
        _replay.checksums,
        _replay.user_data
    ));

    if (use_packed) {
        _replay.inputs.swap(packed);
        _replay.compressed = false;
    }

    _replay.checksums.insert(_replay.checksums.end(), later.begin(), later.end());

    if (bad) {
        printf("failed to serialize replay data\n");
        return nullptr;
    }

    length = (u32)_bin_buffer.size();

    return _bin_buffer.data();
}

void Gekko::ReplaySystem::RecordInputs(SyncSystem& sync)
{
    if (_mode != Recording || _needs_state) return;

    const Frame incorrect = sync.GetMinIncorrectFrame();

    if (incorrect != GameInput::NULL_FRAME) {
        _last_recorded_frame = std::min(_last_recorded_frame, incorrect - 1);
    }

    const Frame received = sync.GetMinReceivedFrame();

    std::unique_ptr<u8[]> inputs;
    for (Frame frame = _last_recorded_frame + 1; frame <= received; frame++) {
        if (!sync.GetSpectatorInputs(inputs, frame)) {
            break;
        }

        RecordInput(frame, inputs.get());

        _last_recorded_frame = frame;
    }
}

void Gekko::ReplaySystem::RecordInput(Frame frame, const u8* input)
{
    if (_mode != Recording || !input) return;

    const u32 size = InputSize();
    if (size == 0) return;

    if (frame < _start_frame) return;

    const u64 index = (u64)(frame - _start_frame);
    const u64 needed = (index + 1) * size;
    if (_replay.inputs.size() < needed) {
        _replay.inputs.resize(needed);
    }

    std::memcpy(_replay.inputs.data() + index * size, input, size);
}

void Gekko::ReplaySystem::RecordState(const u8* state, u32 length, Frame frame)
{
    if (_mode != Recording || !state || length == 0) return;

    if (length > _replay.config.state_size) return;

    _replay.initial_state.assign(state, state + length);

    _replay.initial_state.resize(_replay.config.state_size);

    _start_frame = frame + 1;
    _last_recorded_frame = frame;
    _last_checksum_frame = frame;

    _needs_state = false;
}

bool Gekko::ReplaySystem::SetChecksumInterval(u32 interval)
{
    if (_mode != Recording) return false;

    _replay.checksum_interval = interval;

    return true;
}

bool Gekko::ReplaySystem::SetUserData(const u8* data, u32 length)
{
    if (_mode != Recording || (!data && length > 0)) return false;

    if (length == 0) {
        _replay.user_data.clear();
    }
    else {
        _replay.user_data.assign(data, data + length);
    }

    return true;
}

bool Gekko::ReplaySystem::WantsChecksum(Frame frame) const
{
    if (_mode != Recording || _needs_state || _replay.checksum_interval == 0) return false;

    if (frame < 0 || frame < _start_frame || frame <= _last_checksum_frame) return false;

    return frame % (Frame)_replay.checksum_interval == 0;
}

void Gekko::ReplaySystem::RecordChecksums(StateStorage& storage, Frame up_to)
{
    if (_mode != Recording || _needs_state || _replay.checksum_interval == 0) return;

    for (Frame frame = std::max(_last_checksum_frame + 1, _start_frame); frame <= up_to; frame++) {
        // frames the session did not save, or whose save got overwritten, stay without one.
        auto state = storage.GetState(frame);
        if (state->frame == frame) {
            RecordChecksum(frame, state->checksum);
        }
    }

    _last_checksum_frame = std::max(_last_checksum_frame, up_to);
}

void Gekko::ReplaySystem::RecordChecksum(Frame frame, u32 checksum)
{
    if (!WantsChecksum(frame)) return;

    _replay.checksums.push_back({ frame - _start_frame, checksum });

    _last_checksum_frame = frame;
}

bool Gekko::ReplaySystem::NeedsState()
{
    RecordPendingState();

    return _mode == Recording && _needs_state;
}

Gekko::StateEntry* Gekko::ReplaySystem::PendingState()
{
    _pending_state = true;

    return &_state;
}

void Gekko::ReplaySystem::RecordPendingState()
{
    if (!_pending_state) return;

    _pending_state = false;

    RecordState(_state.state.get(), _state.state_len, _state.frame);
}

bool Gekko::ReplaySystem::LoadReplay(const u8* replay_data, u32 length)
{
    Reset();

    if (!replay_data || length == 0) return false;

    _bin_buffer.assign(replay_data, replay_data + length);

    bool bad = true;
    try {
        zpp::bits::in in(_bin_buffer);
        bad = failure(in(_replay.header, _replay.version));

        if (!bad && _replay.header == ReplayBlob::MAGIC) {
            if (_replay.version == ReplayBlob::FORMAT_VERSION_1) {
                bad = failure(in(
                    _replay.compressed,
                    _replay.config,
                    _replay.inputs,
                    _replay.initial_state
                ));
            }
            else if (_replay.version == ReplayBlob::FORMAT_VERSION) {
                bad = failure(in(
                    _replay.compressed,
                    _replay.config,
                    _replay.inputs,
                    _replay.initial_state,
                    _replay.checksum_interval,
                    _replay.checksums,
                    _replay.user_data
                ));
            }
        }
    }
    catch (...) {
        bad = true;
    }

    if (bad) {
        printf("failed to deserialize replay data\n");
        Reset();
        return false;
    }

    if (_replay.header != ReplayBlob::MAGIC) {
        printf("invalid replay header\n");
        Reset();
        return false;
    }

    if (_replay.version != ReplayBlob::FORMAT_VERSION &&
        _replay.version != ReplayBlob::FORMAT_VERSION_1) {
        printf("unsupported replay version %u\n", _replay.version);
        Reset();
        return false;
    }

    if (InputSize() == 0) {
        printf("invalid replay config\n");
        Reset();
        return false;
    }

    if (!_replay.initial_state.empty() &&
        _replay.initial_state.size() != _replay.config.state_size) {
        printf("invalid replay state size\n");
        Reset();
        return false;
    }

    if (_replay.compressed) {
        if (_replay.inputs.size() % 2 != 0) {
            printf("invalid replay input data\n");
            Reset();
            return false;
        }

        auto delta = Compression::RLEDecode(_replay.inputs.data(), (u32)_replay.inputs.size());
        _replay.inputs = Compression::DeltaDecode(delta.data(), (u32)delta.size(), InputSize());
        _replay.compressed = false;
    }

    if (!ValidChecksums()) {
        printf("invalid replay checksums\n");
        Reset();
        return false;
    }

    _current_frame = 0;
    _next_checksum = 0;
    _mode = Replaying;

    return true;
}

const u8* Gekko::ReplaySystem::ReplayState()
{
    if (_replay.initial_state.empty()) return nullptr;

    return _replay.initial_state.data();
}

bool Gekko::ReplaySystem::NextReplayInput(u8* input)
{
    if (_mode != Replaying || !input) return false;

    const u32 size = InputSize();
    if (size == 0) return false;

    const u64 offset = (u64)_current_frame * size;
    if (offset + size > _replay.inputs.size()) return false;

    std::memcpy(input, _replay.inputs.data() + offset, size);
    _current_frame++;

    return true;
}

bool Gekko::ReplaySystem::RecordedChecksum(Frame frame, u32& checksum)
{
    if (_mode != Replaying) return false;

    const auto& checksums = _replay.checksums;
    while (_next_checksum < checksums.size() && checksums[_next_checksum].frame < frame) {
        _next_checksum++;
    }

    if (_next_checksum >= checksums.size() || checksums[_next_checksum].frame != frame) {
        return false;
    }

    checksum = checksums[_next_checksum].checksum;

    return true;
}

const u8* Gekko::ReplaySystem::UserData(u32& length) const
{
    length = 0;

    if (_mode != Replaying || _replay.user_data.empty()) return nullptr;

    length = (u32)_replay.user_data.size();

    return _replay.user_data.data();
}

GekkoConfig Gekko::ReplaySystem::Config()
{
    return _replay.config;
}

bool Gekko::ReplaySystem::IsRecording() const
{
    return _mode == Recording;
}

bool Gekko::ReplaySystem::IsReplaying() const
{
    return _mode == Replaying;
}

void Gekko::ReplaySystem::Reset()
{
    _mode = None;
    _start_frame = 0;
    _current_frame = 0;
    _last_recorded_frame = GameInput::NULL_FRAME;
    _last_checksum_frame = GameInput::NULL_FRAME;
    _next_checksum = 0;

    _needs_state = false;
    _pending_state = false;
    _no_compression = false;

    _bin_buffer.clear();

    _state = StateEntry();

    _replay = {};
}

u32 Gekko::ReplaySystem::InputSize() const
{
    return _replay.config.input_size * _replay.config.num_players;
}

bool Gekko::ReplaySystem::ValidChecksums() const
{
    if (_replay.checksums.empty()) return true;

    if (_replay.checksum_interval == 0) return false;

    // checksums belong to recorded frames and are stored in frame order.
    const u64 frames = _replay.inputs.size() / InputSize();

    Frame previous = GameInput::NULL_FRAME;
    for (auto& entry : _replay.checksums) {
        if (entry.frame <= previous || (u64)entry.frame >= frames) {
            return false;
        }
        previous = entry.frame;
    }

    return true;
}
