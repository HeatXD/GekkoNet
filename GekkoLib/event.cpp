#include "event.h"

Gekko::Advance::~Advance()
{
	if (inputs)
		std::free(inputs);
}

Gekko::EventData::EventData()
{
	ev.xxx = 0;
}

Gekko::Event::Event()
{
	type = None;
}

Gekko::EventData::x::~x()
{	
}
