# GekkoNet
## C++ Peer To Peer Game Networking Framework

### Why?
I built this because I wanted a framework to plug into my C++ projects, in the past I have created a wrapper around GGRS for C++ but after having to deal with Rust FFI I decided to build a native alternative instead to more easily fit my projects. 
This framework is heavily inspired by the GGPO Rust port GGRS.

#### Why not use GGPO?
I am personally not a big fan of the callback based control flow of GGPO hence why I am more of fond of how GGRS handles its control flow. And I might be addicted to reinventing the wheel :sweat_smile:

### Done
- Local/Couch Sessions
	- Per Player Input Delay Settings
- Online Sessions
	- Local Player Input Delay Settings
	- Remote Player Input Prediction Settings
- Spectator Sessions
	- The ability to spectate spectators. This might be handy if you have a seperate spectating service which propegates the inputs to more spectators.
- Limited Saving 
	- Save the gamestate less often which might help games where saving the game is expensive. This is at the cost of more iterations advancing the gamestate during rollback.

### Work in progress
- Desync Detection
- Network Statistics
- Joining a session that's already in progress as a spectator (and maybe as a player later)
- Replays
- Add xmake for building.
- Commission an artist to create a logo for GekkoNet

### Building
Todo

### Projects using GekkoNet
Todo

### License
GekkoNet is licensed under the BSD-2-Clause License
[Read about it here](https://opensource.org/license/bsd-2-clause).
