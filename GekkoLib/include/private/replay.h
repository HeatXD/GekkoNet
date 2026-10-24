#pragma once

#include <vector>

#include "gekko_types.h"
#include "gekkonet.h"
#include "storage.h"
#include "sync.h"

namespace Gekko {
    struct ReplayChecksum {
        // frame relative to the first recorded input.
        Frame frame = 0;
        u32 checksum = 0;
    };

    struct ReplayBlob {
        static constexpr u32 MAGIC = 0x474B5250; // GKRP
        static constexpr u32 FORMAT_VERSION = 2;
        // version 1 replays end after the initial state.
        static constexpr u32 FORMAT_VERSION_1 = 1;

        u32 header = MAGIC;
        u32 version = FORMAT_VERSION;
        bool compressed = false;

        GekkoConfig config = {};

        std::vector<u8> inputs;
        std::vector<u8> initial_state;

        // added in version 2
        u32 checksum_interval = 0;
        std::vector<ReplayChecksum> checksums;
        std::vector<u8> user_data;
    };

    struct ReplaySystem {
        bool StartRecording(GekkoConfig config, Frame frame, bool save_state, bool disable_compression);
        const u8* StopRecording(u32& length);
        const u8* PeekRecording(u32& length);
        void RecordInputs(SyncSystem& sync);
        void RecordState(const u8* state, u32 length, Frame frame);

        bool SetChecksumInterval(u32 interval);
        bool SetUserData(const u8* data, u32 length);
        // records the checksums of the frames up to and including up_to the storage still holds.
        void RecordChecksums(StateStorage& storage, Frame up_to);
        // records the checksum of a single confirmed frame.
        void RecordChecksum(Frame frame, u32 checksum);
        bool WantsChecksum(Frame frame) const;

        bool NeedsState();
        StateEntry* PendingState();

        bool LoadReplay(const u8* replay_data, u32 length);
        const u8* ReplayState();
        bool NextReplayInput(u8* input);
        // the recorded checksum of a playback frame, frames have to be asked in order.
        bool RecordedChecksum(Frame frame, u32& checksum);
        const u8* UserData(u32& length) const;

        GekkoConfig Config();

        bool IsRecording() const;
        bool IsReplaying() const;

        void Reset();

    private:

        void RecordInput(Frame frame, const u8* input);

        void RecordPendingState();

        const u8* Serialize(u32& length);

        u32 InputSize() const;

        bool ValidChecksums() const;

        enum Mode {
            None,
            Recording,
            Replaying
        } _mode = None;

        bool _needs_state = false;
        bool _pending_state = false;
        bool _no_compression = false;

        Frame _start_frame = 0;
        Frame _current_frame = 0;
        Frame _last_recorded_frame = GameInput::NULL_FRAME;
        Frame _last_checksum_frame = GameInput::NULL_FRAME;

        u32 _next_checksum = 0;

        std::vector<u8> _bin_buffer;

        StateEntry _state;

        ReplayBlob _replay;
    };
}
