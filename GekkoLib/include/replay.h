#pragma once

#include "gekko_types.h"

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <filesystem>

#include <zpp/serializer.h>

namespace Gekko {
    struct ReplayFile {
        u32 input_size = 0;
        u32 num_players = 0;
        u32 input_count = 0;

        std::vector<u8> start_state;
        std::vector<u8> inputs;

        template <typename Archive, typename Self>
        static void serialize(Archive& a, Self& s) {
            a(s.input_size, s.num_players, s.input_count, s.start_state, s.inputs);
        }
    };

    struct ReplaySystem {

        ~ReplaySystem();

        void Init(bool replay_mode, u8 num_players, u32 input_size);

        void AddStartState(u8* state, u32 size);

        void LoadFile(std::string filepath);

        void SetOutputDir(std::string dirpath);

        void SetSessionMagic(u32 magic);

        std::vector<u8> GetInputsForFrame(Frame frame);

        void AddInputForHandle(Handle handle, Frame frame, u8* input, u32 length);

    private:
        Frame GetMinInputCount();

        void WriteToFile();

        bool IsReading() const;

        bool IsWriting() const;

        bool EndsWith(std::string_view str, std::string_view suffix);

        std::string Today();

    private:
        inline static const std::string BASE_DIR = "GekkoNet\\Replays";
        inline static const std::string FILE_EXT = ".gsrf"; // Gekko Session Replay Format

        bool _mode;

        u32 _magic;

        ReplayFile _file;

        std::filesystem::path _path;

        struct Chunk {
            static const i32 MAX_SIZE = 255;

            Frame last_added = -1;

            std::vector<u8> raw;
        };

        std::map<Handle, Chunk> _inputs;
    };
}
