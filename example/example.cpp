#include <iostream>
#include "../GekkoLib/gekko.h"
#include <windows.h>
#include "example.h"

struct GState {
	int Inp1 = 0;
	int Inp2 = 0;
	int Sum = 0;
};

int main(void)
{
	std::cout << "Hello World!\n";

	Gekko::Session::Test();

	union input {
		unsigned char val;
	};

	auto sess = Gekko::Session();
	sess.Init(2, 0, 1, sizeof(char));

	auto p1 = sess.AddPlayer(Gekko::PlayerType::Local);
	auto p2 = sess.AddPlayer(Gekko::PlayerType::Local);

	int idx = 0;
	unsigned char inp = 255;
	while (true)
	{
		sess.AddLocalInput(p1, &inp);
		sess.AddLocalInput(p2, &inp);

		auto ev = sess.UpdateSession();
		for (int j = 0; j < ev.size(); j++)
		{
			std::cout << "ev type: " << ev[j].type << "\n";
			switch (ev[j].type)
			{
			case Gekko::EventType::AdvanceEvent:
				printf("P1 %d, P2 %d\n", ev[j].data.ev.adv.inputs[0], ev[j].data.ev.adv.inputs[1]);
				// THE USER HAS TO FREE THE INPUTS THEYRE USING. when theyre done
				std::free(ev[j].data.ev.adv.inputs);
				break;
			default:
				break;
			}

		}
		std::cout << "iter: " << idx << " ev size:" << ev.size() << "\n";
		Sleep(10);
		idx++;
	}

	return 0;
}