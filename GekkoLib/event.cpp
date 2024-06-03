#include "event.h"
#include <cassert>

void Gekko::GameEventBuffer::Init(u32 input_size)
{
    _input_size = input_size;
    _index_advance = 0;
    _index_others = 0;
}

void Gekko::GameEventBuffer::Reset()
{
    _index_advance = 0;
    _index_others = 0;

    // clear the inputs
    for (auto& ev : _buffer_advance){
        if (ev->type == Empty) {
            break;
        }
        // reuse the input buffer rather than destroying it.
        // set event to empty to be used again.
        ev->type = Empty;
    }

    for (auto& ev : _buffer_others) {
        if (ev->type == Empty) {
            break;
        }
        // set event to empty to be used again.
        ev->type = Empty;
    }
}

Gekko::GameEvent* Gekko::GameEventBuffer::GetEvent(bool advance)
{
    auto& buff = advance ? _buffer_advance : _buffer_others;
    u16& idx = advance ? _index_advance : _index_others;

    idx++;

    if (buff.size() < idx) {
        buff.push_back(std::make_unique<GameEvent>());

        if (advance) {
            buff.back()->data.adv.input_len = _input_size;
            buff.back()->data.adv.inputs = (u8*)std::malloc(_input_size);
        }
    }

    assert(idx != 0);

    return buff[idx - 1].get();
}

Gekko::GameEvent::~GameEvent()
{
    // cleanup inputs automatically
    if (type == AdvanceEvent && data.adv.inputs) {
        std::free(data.adv.inputs);
        data.adv.inputs = nullptr;
    }
}
