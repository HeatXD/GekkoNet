#pragma once

#include <SDL3/SDL.h>

namespace GekkoGame {
    constexpr int TARGET_FPS = 60;
    constexpr int MAX_PLAYERS = 4;
    constexpr int MAX_BALLS = 4;
    constexpr int MAX_ENTITIES = MAX_PLAYERS + MAX_BALLS;
    constexpr int GAME_SCALE = 1000;
    constexpr int FIELD_SIZE = 600;

    constexpr int EPOS_HISTORY = 4;

    constexpr uint8_t PLAYER_COLORS[4 * MAX_PLAYERS] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        71, 167, 199, 255,
        255, 0, 255, 255,
    };

    struct Rect {
        int32_t hw, hh;
    };

    struct Point {
        int32_t x, y;
    };

    struct AABB {
        Rect ext;
        Point pos;
    };

    const Rect BOX_TYPES[3] = {
       {15 * GAME_SCALE, 60 * GAME_SCALE}, // p1/p2 paddle
       {60 * GAME_SCALE, 15 * GAME_SCALE}, // p3/p4 paddle
       {10 * GAME_SCALE, 10 * GAME_SCALE}, // ball
    };

    struct Input {
        uint8_t up : 1;
        uint8_t down : 1;
        uint8_t left : 1;
        uint8_t right : 1;
    };

    struct Gamestate {
        struct NoState {
            Point e_prev_pos[MAX_ENTITIES * EPOS_HISTORY];
        } nostate = {};
        struct State {
            Input inputs[MAX_PLAYERS];

            uint32_t frame = 0;

            uint8_t hitstop[MAX_ENTITIES];
            uint8_t move_speed[MAX_ENTITIES];

            uint8_t scores[MAX_PLAYERS];

            Point e_pos[MAX_ENTITIES]; // current entity (center) position
            Point e_vel[MAX_ENTITIES]; // current entity velocity

            struct Flags {
                uint8_t started : 1;
                uint8_t finished : 1;
                uint8_t players : 2; // 2 bits are enough to up to repesent 4 players
                uint8_t balls : 2; // 2 bits are enough to up to repesent 4 balls
            } flags = {};
        } state = {};

        void Init(int num_players);
        void Draw(SDL_Renderer* renderer);
        void Update(Input inputs[MAX_PLAYERS]);

    private:
        void ApplyInput(Input inputs[MAX_PLAYERS]);
        void HandleGame();
        bool DoesCollideAABB(const AABB& a, const AABB& b);
    };
}
