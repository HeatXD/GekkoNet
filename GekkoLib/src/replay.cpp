#include "replay.h"

#include <cassert>

void Gekko::ReplaySystem::Init(bool read_mode)
{
    _mode = read_mode;
}

void Gekko::ReplaySystem::LoadFile(std::string filepath)
{
    if (!EndsWith(filepath, ".GRPLY")) {
        assert(false);
    }

    _path = filepath;
}

void Gekko::ReplaySystem::SetOutputDir(std::string dirpath)
{
    _path = dirpath;
    _path /= std::to_string(_magic);
    _path += Today();
    _path += ".GRPLY";
}

void Gekko::ReplaySystem::SetSessionMagic(u16 magic)
{
    _magic = magic;
}

std::vector<Gekko::u8> Gekko::ReplaySystem::GetInputsForFrame(Frame frame)
{
    if (_current + 1 != frame) {
        assert(false);
    }

    return std::vector<u8>();
}

bool Gekko::ReplaySystem::IsReading() const
{
    return _mode == true;
}

bool Gekko::ReplaySystem::IsWriting() const
{
    return _mode == false;
}

bool Gekko::ReplaySystem::EndsWith(std::string_view str, std::string_view suffix)
{
    return
        str.size() >= suffix.size() &&
        str.compare(
            str.size() - suffix.size(),
            suffix.size(),
            suffix
        ) == 0;
}

std::string Gekko::ReplaySystem::Today()
{
    std::time_t now = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now);

    // Create a buffer to hold the date string
    char buffer[11];

    std::strftime(
        buffer,
        sizeof(buffer),
        "[%Y-%m-%d]",
        now_tm
    );

    return std::string(buffer);
}
