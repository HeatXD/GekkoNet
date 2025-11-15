#include "game.h"
#include <cstdlib>
#include <algorithm>

namespace GekkoGame {
    void Gamestate::Init(int num_players) {
        state = {};
        state.flags.players = num_players - 1;
        state.flags.balls = 0;
        state.flags.started = true;
        state.flags.finished = false;
        //setup default move speed multiplier for each entity
        std::memset(state.move_speed, 1, sizeof(state.move_speed));
        // setup paddle positions
        for (int i = 0; i <= state.flags.players; i++) {
            if (i <= 1) {
                state.e_pos[i].x = (FIELD_SIZE * i) * GAME_SCALE;
                state.e_pos[i].y = FIELD_SIZE / 2 * GAME_SCALE;
            } else {
                state.e_pos[i].x = FIELD_SIZE / 2 * GAME_SCALE;
                state.e_pos[i].y = (FIELD_SIZE * (i - 2)) * GAME_SCALE;
            }
        }
        // setup the inital ball
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++) {
            state.e_pos[i].x = FIELD_SIZE / 2 * GAME_SCALE;
            state.e_pos[i].y = FIELD_SIZE / 2 * GAME_SCALE;
            state.e_vel[i].x = -1800;
            state.e_vel[i].y = 0;
        }
    }

    void Gamestate::Draw(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        // draw balls in play
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++) {
            Rect e_box = BOX_TYPES[2];
            float px = (float)(state.e_pos[i].x - e_box.hw) / GAME_SCALE;
            float py = (float)(state.e_pos[i].y - e_box.hh) / GAME_SCALE;
            float w = (float)(e_box.hw * 2) / GAME_SCALE;
            float h = (float)(e_box.hh * 2) / GAME_SCALE;
            SDL_FRect rect = { px, py, w, h };
            SDL_RenderFillRect(renderer, &rect);
            // draw the trail
            for (int j = 0; j < EPOS_HISTORY; j++) {
                int trail_index = (state.frame - j + EPOS_HISTORY) % EPOS_HISTORY;
                int alpha = (255 * (EPOS_HISTORY - j)) / EPOS_HISTORY;
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
                rect.x = (float)(nostate.e_prev_pos[i * EPOS_HISTORY + trail_index].x - e_box.hw) / GAME_SCALE;
                rect.y = (float)(nostate.e_prev_pos[i * EPOS_HISTORY + trail_index].y - e_box.hh) / GAME_SCALE;
                SDL_RenderFillRect(renderer, &rect);
            }
        }
        // draw players
        for (int i = 0; i <= state.flags.players; i++) {
            const uint8_t* col = &PLAYER_COLORS[i * 4];
            SDL_SetRenderDrawColor(renderer, col[0], col[1], col[2], col[3]);
            Rect e_box = i <= 1 ? BOX_TYPES[0] : BOX_TYPES[1];
            float px = (float)(state.e_pos[i].x - e_box.hw) / GAME_SCALE;
            float py = (float)(state.e_pos[i].y - e_box.hh) / GAME_SCALE;
            float w = (float)(e_box.hw * 2) / GAME_SCALE;
            float h = (float)(e_box.hh * 2) / GAME_SCALE;
            SDL_FRect rect = { px, py, w, h };
            SDL_RenderFillRect(renderer, &rect);
        }
        SDL_RenderPresent(renderer);
    }

    void Gamestate::Update(Input inputs[MAX_PLAYERS]) {
        ApplyInput(inputs);
        HandleGame();
    }

    Input Gamestate::PollInput() {
        Input inp = {};
        const bool* keys = SDL_GetKeyboardState(NULL);
        inp.left = keys[SDL_SCANCODE_LEFT];
        inp.right = keys[SDL_SCANCODE_RIGHT];
        inp.up = keys[SDL_SCANCODE_UP];
        inp.down = keys[SDL_SCANCODE_DOWN];
        return inp;
    }

    void Gamestate::ApplyInput(Input inputs[MAX_PLAYERS]) {
        memcpy(state.inputs, inputs, sizeof(Input) * MAX_PLAYERS);
    }

    void Gamestate::HandleGame() {
        state.frame++;
        // move paddles
        for (int i = 0; i <= state.flags.players; i++) {
            if (i <= 1) {
                if (state.inputs[i].up) state.e_vel[i].y = -4000;
                else if (state.inputs[i].down) state.e_vel[i].y = 4000;
                else state.e_vel[i].y = 0;
            } else {
                if (state.inputs[i].left) state.e_vel[i].x = -4000;
                else if (state.inputs[i].right) state.e_vel[i].x = 4000;
                else state.e_vel[i].x = 0;
            }
        }

        // integrate / move entities
        for (int i = 0; i < MAX_ENTITIES; i++) {
            // capture last position for tails vfx
            nostate.e_prev_pos[i * EPOS_HISTORY + (state.frame % EPOS_HISTORY)] = state.e_pos[i];
            // skip updating positions when in hitstop
            if (state.hitstop[i] > 0) {
                state.hitstop[i]--;
                continue;
            }
            // update current position
            state.e_pos[i].x += state.e_vel[i].x;
            state.e_pos[i].y += state.e_vel[i].y;
        }

        // screen wrapping paddles
        for (int i = 0; i <= state.flags.players; i++) {
            if (i <= 1) {
                if (state.e_pos[i].y < 0) state.e_pos[i].y = FIELD_SIZE * GAME_SCALE;
                if (state.e_pos[i].y > FIELD_SIZE * GAME_SCALE) state.e_pos[i].y = 0;
            } else {
                if (state.e_pos[i].x < 0) state.e_pos[i].x = FIELD_SIZE * GAME_SCALE;
                if (state.e_pos[i].x > FIELD_SIZE * GAME_SCALE) state.e_pos[i].x = 0;
            }
        }

        // make balls clamp to screen
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++) {
            state.e_pos[i].x = std::clamp(state.e_pos[i].x, -1, FIELD_SIZE * GAME_SCALE + 1);
            state.e_pos[i].y = std::clamp(state.e_pos[i].y, -1, FIELD_SIZE * GAME_SCALE + 1);
        }

        // make balls bounce of paddles
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++) {
            // skip if ball is in hitstop
            if (state.hitstop[i] > 0) continue;

            for (int j = 0; j <= state.flags.players; j++) {
                const bool horizontal = j <= 1;
                const Rect& paddle_box = horizontal ? BOX_TYPES[0] : BOX_TYPES[1];
                const Rect& ball_box = BOX_TYPES[2];

                if (DoesCollideAABB( { ball_box, state.e_pos[i] }, { paddle_box, state.e_pos[j] })) {
                    if (state.move_speed[i] < UINT8_MAX) state.move_speed[i]++;

                    int* ball_pos = horizontal ? &state.e_pos[i].x : &state.e_pos[i].y;
                    int* ball_vel = horizontal ? &state.e_vel[i].x : &state.e_vel[i].y;
                    int* cross_vel = horizontal ? &state.e_vel[i].y : &state.e_vel[i].x;

                    const int ball_cross = horizontal ? state.e_pos[i].y : state.e_pos[i].x;
                    const int paddle_cross = horizontal ? state.e_pos[j].y : state.e_pos[j].x;
                    const int paddle_pos = horizontal ? state.e_pos[j].x : state.e_pos[j].y;
                    const int paddle_cross_half = horizontal ? paddle_box.hh : paddle_box.hw;

                    // determine segment (0-7)
                    const int offset = ball_cross - paddle_cross;
                    const int segment_size = (paddle_cross_half * 2) / 8;
                    const int segment = std::min(7, std::max(0, (offset + paddle_cross_half) / segment_size));

                    // half table (segments 0-3), mirror for 4-7
                    const int velocities[4][2] = {
                        {1200, 2000},   // steepest
                        {1400, 1400},   // 45°
                        {1600, 800},
                        {1800, 0}       // straight
                    };

                    const int idx = segment < 4 ? segment : 7 - segment;
                    const int cross_sign = segment < 4 ? -1 : 1;

                    // determine which side of paddle the ball is on
                    const int side_sign = (*ball_pos - *ball_vel > paddle_pos) ? 1 : -1;

                    // set new velocities (ball bounces away from paddle)
                    *ball_vel = side_sign * velocities[idx][0] * state.move_speed[i];
                    *cross_vel = cross_sign * velocities[idx][1] * state.move_speed[i];

                    // push ball outside paddle boundary
                    const int ball_half = horizontal ? ball_box.hw : ball_box.hh;
                    const int paddle_half = horizontal ? paddle_box.hw : paddle_box.hh;
                    *ball_pos = paddle_pos + side_sign * (paddle_half + ball_half + 2);

                    // put paddle and ball into hitstop
                    const uint8_t speed = state.move_speed[i];
                    const uint8_t duration = std::clamp(speed, (uint8_t)0, (uint8_t)14);
                    state.hitstop[i] = duration;
                    state.hitstop[j] = duration;
                }
            }
        }

        // make balls bounce of walls or respawn when a player gets scored on
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++){
            bool respawn = false;
            const int limit = FIELD_SIZE * GAME_SCALE;

            Point& pos = state.e_pos[i];
            Point& vel = state.e_vel[i];

            if (pos.x < 0 || pos.x > limit) {
                const bool hitLeft = (pos.x < 0);
                const int  playerId = hitLeft ? 0 : 1;

                if (state.flags.players >= playerId) respawn = true;
                else vel.x = -vel.x;

                pos.x = std::clamp(pos.x, 0, limit);
            }

            if (pos.y < 0 || pos.y > limit) {
                const bool hitTop = (pos.y < 0);
                const int  playerId = hitTop ? 2 : 3;

                if (state.flags.players >= playerId) respawn = true;
                else vel.y = -vel.y;

                pos.y = std::clamp(pos.y, 0, limit);
            }

            if (respawn) {
                state.move_speed[i] = 1;
                pos.x = FIELD_SIZE / 2 * GAME_SCALE;
                pos.y = FIELD_SIZE / 2 * GAME_SCALE;
                vel.x = -1800;
                vel.y = 0;
            }
        }
    }

    bool Gamestate::DoesCollideAABB(const AABB& a, const AABB& b) {
        bool overlapX = std::abs(a.pos.x - b.pos.x) <= (a.ext.hw + b.ext.hw);
        bool overlapY = std::abs(a.pos.y - b.pos.y) <= (a.ext.hh + b.ext.hh);
        return overlapX && overlapY;
    }
}

