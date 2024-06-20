#pragma once

#include "gekko_types.h"

#include <string>
#include <memory>
#include <vector>
#include <filesystem>

namespace Gekko {
    struct ReplayFile {
        std::unique_ptr<u8[]> start_state;
        std::vector<u8> inputs;
    };

    struct ReplaySystem {

        void Init(bool read_mode);

        void LoadFile(std::string filepath);

        void SetOutputDir(std::string dirpath);

        void SetSessionMagic(u16 magic);

        std::vector<u8> GetInputsForFrame(Frame frame);

    private:
        bool IsReading() const;

        bool IsWriting() const;

        bool EndsWith(std::string_view str, std::string_view suffix);

        std::string Today();

    private:
        bool _mode;

        u16 _magic;

        Frame _current;

        ReplayFile _file;

        std::filesystem::path _path;
    };
}
