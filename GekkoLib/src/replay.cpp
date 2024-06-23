#include "replay.h"
#include "compression.h"

#include <ctime>
#include <cassert>
#include <iostream>
#include <fstream>  

void Gekko::ReplaySystem::Init(bool replay_mode, u8 num_players, u32 input_size)
{
    _mode = replay_mode;

    if (IsWriting()) {
        _file.input_size = input_size;
        _file.num_players = num_players;
    }


    for (u8 i = 1; i <= num_players; i++) {
        _inputs[i] = Chunk();
    }
}

void Gekko::ReplaySystem::AddStartState(u8* state, u32 size)
{
    if (!IsWriting()) {
        return;
    }

    _file.start_state.insert(_file.start_state.end(), state, state + size);
}

void Gekko::ReplaySystem::LoadFile(std::string filepath)
{
    if (!IsReading()) {
        return;
    }

    if (!EndsWith(filepath, FILE_EXT)) {
        assert(false);
    }

    _path = filepath;
}

void Gekko::ReplaySystem::SetOutputDir(std::string dirpath)
{
    if (!IsWriting()) {
        return;
    }

    if (dirpath.empty()) {
        dirpath = BASE_DIR;
    }

    std::filesystem::create_directories(dirpath);

    _path = dirpath;
    _path /= std::to_string(_magic);
    _path += Today();
    _path += FILE_EXT;

    auto stream = std::ofstream(_path, std::ios::binary);
    stream.close();
}

void Gekko::ReplaySystem::SetSessionMagic(u32 magic)
{
    _magic = magic;
}

std::vector<Gekko::u8> Gekko::ReplaySystem::GetInputsForFrame(Frame frame)
{
    return std::vector<u8>();
}

void Gekko::ReplaySystem::AddInputForHandle(Handle handle, Frame frame, u8* input, u32 length)
{
    if (!IsWriting() || _inputs.count(handle) == 0) {
        return;
    }

    auto& chnk = _inputs[handle];

    if(chnk.last_added + 1 != frame) {
        return;
    }

    _file.input_count = frame;

    chnk.last_added = frame;
    chnk.raw.insert(chnk.raw.end(), input, input + length);

    if (chnk.raw.size() < Chunk::MAX_SIZE * _file.input_size) {
        return;
    }

    // endcode raw data end add it to the encoded list.
    auto tmp = Compression::RLEEncodeSized(chnk.raw.data(), (u32)chnk.raw.size(), _file.input_size);
    _file.inputs.insert(_file.inputs.end(),tmp.begin(), tmp.end());

    // clear raw list
    chnk.raw.clear();
}

bool Gekko::ReplaySystem::IsReading() const
{
    return _mode == false;
}

bool Gekko::ReplaySystem::IsWriting() const
{
    return _mode == true;
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


#ifdef _WIN32
#define localtime_safe(tm_ptr, time_ptr) localtime_s((tm_ptr), (time_ptr))
#else
#define localtime_safe(tm_ptr, time_ptr) localtime_r((time_ptr), (tm_ptr))
#endif

std::string Gekko::ReplaySystem::Today()
{
    // Get current time
    std::time_t t = std::time(nullptr);
    std::tm now;
    // Convert to local time safely
    localtime_safe(&now, &t);

    // Buffer to hold the formatted date
    char buffer[15]; // [YYYY-MM-DD] is 12 characters long + 1 for null terminator
    std::snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02d]", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

    // Convert buffer to a string and return
    return std::string(buffer);
}
