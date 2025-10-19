#include "game.h"
#include <cstdlib>
#include <algorithm>
//#include <cstdio>

namespace GekkoGame {
    void Gamestate::Init(int num_players) {
        state = {};
        state.flags.players = num_players - 1;
        state.flags.balls = 0;
        state.flags.started = true;
        state.flags.finished = false;
        // setup paddle positions
        for (int i = 0; i <= state.flags.players; i++) {
            if (i <= 1) {
                state.e_px[i] = (FIELD_SIZE * i) * GAME_SCALE;
                state.e_py[i] = FIELD_SIZE / 2 * GAME_SCALE;
            } else {
                state.e_px[i] = FIELD_SIZE / 2 * GAME_SCALE;
                state.e_py[i] = (FIELD_SIZE * (i - 2)) * GAME_SCALE;
            }
        }
        // setup the inital ball
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++) {
            state.e_px[i] = FIELD_SIZE / 2 * GAME_SCALE;
            state.e_py[i] = FIELD_SIZE / 2 * GAME_SCALE;
            state.e_vx[i] = -4500;
            state.e_vy[i] = 0;
        }
    }

    void Gamestate::Draw(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        // draw balls in play
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++) {
            Rect e_box = BOX_TYPES[2];
            float px = (float)(state.e_px[i] - e_box.hw) / GAME_SCALE;
            float py = (float)(state.e_py[i] - e_box.hh) / GAME_SCALE;
            float w = (float)(e_box.hw * 2) / GAME_SCALE;
            float h = (float)(e_box.hh * 2) / GAME_SCALE;
            SDL_FRect rect = { px, py, w, h };
            SDL_RenderFillRect(renderer, &rect);
            // draw the trail
            for (int j = 0; j < EPOS_HISTORY; j++) {
                int trail_index = (state.frame - j + EPOS_HISTORY) % EPOS_HISTORY;
                int alpha = (255 * (EPOS_HISTORY - j)) / EPOS_HISTORY;
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
                rect.x = (float)(nostate.prev_e_px[i * EPOS_HISTORY + trail_index] - e_box.hw) / GAME_SCALE;
                rect.y = (float)(nostate.prev_e_py[i * EPOS_HISTORY + trail_index] - e_box.hh) / GAME_SCALE;
                SDL_RenderFillRect(renderer, &rect);
            }
        }
        // draw players
        for (int i = 0; i <= state.flags.players; i++) {
            const uint8_t* col = &PLAYER_COLORS[i * 4];
            SDL_SetRenderDrawColor(renderer, col[0], col[1], col[2], col[3]);
            Rect e_box = i <= 1 ? BOX_TYPES[0] : BOX_TYPES[1];
            float px = (float)(state.e_px[i] - e_box.hw) / GAME_SCALE;
            float py = (float)(state.e_py[i] - e_box.hh) / GAME_SCALE;
            float w = (float)(e_box.hw * 2) / GAME_SCALE;
            float h = (float)(e_box.hh * 2) / GAME_SCALE;
            SDL_FRect rect = { px, py, w, h };
            SDL_RenderFillRect(renderer, &rect);
        }
        // draw hud
        // bg
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_FRect hud_rect = { 0, FIELD_SIZE, FIELD_SIZE, FIELD_SIZE * 1.15 - FIELD_SIZE };
        SDL_RenderFillRect(renderer, &hud_rect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderRect(renderer, &hud_rect);
        //
        SDL_RenderPresent(renderer);
    }

    void Gamestate::Update(Input inputs[MAX_PLAYERS]) {
        ApplyInput(inputs);
        HandleGame();
    }

    void Gamestate::ApplyInput(Input inputs[MAX_PLAYERS]) {
        memcpy(state.inputs, inputs, sizeof(Input) * MAX_PLAYERS);
    }

    void Gamestate::HandleGame() {
        state.frame++;
        // move paddles
        for (int i = 0; i <= state.flags.players; i++) {
            if (i <= 1) {
                if (state.inputs[i].up) state.e_vy[i] = -1000;
                else if (state.inputs[i].down) state.e_vy[i] = 1000;
                else state.e_vy[i] = 0;
            } else {
                if (state.inputs[i].left) state.e_vx[i] = -1000;
                else if (state.inputs[i].right) state.e_vx[i] = 1000;
                else state.e_vx[i] = 0;
            }
        }

        // integrate / move entities
        for (int i = 0; i < MAX_ENTITIES; i++) {
            nostate.prev_e_px[i * EPOS_HISTORY + (state.frame % EPOS_HISTORY)] = state.e_px[i];
            nostate.prev_e_py[i * EPOS_HISTORY + (state.frame % EPOS_HISTORY)] = state.e_py[i];

            state.e_px[i] += state.e_vx[i];
            state.e_py[i] += state.e_vy[i];
        }

        // screen wrapping paddles
        for (int i = 0; i <= state.flags.players; i++) {
            if (i <= 1) {
                if (state.e_py[i] < 0) state.e_py[i] = FIELD_SIZE * GAME_SCALE;
                if (state.e_py[i] > FIELD_SIZE * GAME_SCALE) state.e_py[i] = 0;
            } else {
                if (state.e_px[i] < 0) state.e_px[i] = FIELD_SIZE * GAME_SCALE;
                if (state.e_px[i] > FIELD_SIZE * GAME_SCALE) state.e_px[i] = 0;
            }
        }


        // make balls bounce of paddles
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++) {
            for (int j = 0; j <= state.flags.players; j++) {
                const bool horizontal = j <= 1;
                const Rect* paddle_box = horizontal ? &BOX_TYPES[0] : &BOX_TYPES[1];
                const Rect* ball_box = &BOX_TYPES[2];

                if (DoesCollide({*ball_box, state.e_px[i], state.e_py[i]}, {*paddle_box, state.e_px[j], state.e_py[j]})) {
                    int& ball_pos = horizontal ? state.e_px[i] : state.e_py[i];
                    int& ball_vel = horizontal ? state.e_vx[i] : state.e_vy[i];
                    int& cross_vel = horizontal ? state.e_vy[i] : state.e_vx[i];

                    const int ball_cross = horizontal ? state.e_py[i] : state.e_px[i];
                    const int paddle_cross = horizontal ? state.e_py[j] : state.e_px[j];
                    const int paddle_pos = horizontal ? state.e_px[j] : state.e_py[j];
                    const int paddle_cross_half = horizontal ? paddle_box->hh : paddle_box->hw;

                    // determine segment (0-7)
                    const int offset = ball_cross - paddle_cross;
                    const int segment_size = (paddle_cross_half * 2) / 8;
                    const int segment = std::min(7, std::max(0, (offset + paddle_cross_half) / segment_size));

                    // half table (segments 0-3), mirror for 4-7
                    const int velocities[4][2] = {
                        {3000, 5000},   // steepest
                        {3500, 3500},   // 45°
                        {4000, 2000},
                        {4500, 0}    // straight
                    };

                    const int idx = segment < 4 ? segment : 7 - segment;
                    const int cross_sign = segment < 4 ? -1 : 1;

                    // determine which side of paddle the ball is on
                    const int side_sign = (ball_pos > paddle_pos) ? 1 : -1;

                    // set new velocities (ball bounces away from paddle)
                    ball_vel = side_sign * velocities[idx][0];
                    cross_vel = cross_sign * velocities[idx][1];

                    // push ball outside paddle boundary
                    const int ball_half = horizontal ? ball_box->hw : ball_box->hh;
                    const int paddle_half = horizontal ? paddle_box->hw : paddle_box->hh;
                    ball_pos = paddle_pos + side_sign * (paddle_half + ball_half + 2);
                }
            }
        }

        // make balls bounce of walls or respawn when a player gets scored on
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++){
            bool respawn = false;
            const int limit = FIELD_SIZE * GAME_SCALE;

            int& x = state.e_px[i];
            int& y = state.e_py[i];
            int& vx = state.e_vx[i];
            int& vy = state.e_vy[i];

            if (x < 0 || x > limit) {
                const bool hitLeft = (x < 0);
                const int  playerId = hitLeft ? 0 : 1;

                if (state.flags.players >= playerId) respawn = true;
                else vx = -vx;

                x = std::clamp(x, 0, limit);
            }

            if (y < 0 || y > limit) {
                const bool hitTop = (y < 0);
                const int  playerId = hitTop ? 2 : 3;

                if (state.flags.players >= playerId) respawn = true;
                else vy = -vy;

                y = std::clamp(y, 0, limit);
            }

            if (respawn) {
                x = FIELD_SIZE / 2 * GAME_SCALE;
                y = FIELD_SIZE / 2 * GAME_SCALE;
                vx = -4500;
                vy = 0;
            }
        }
    }

    bool Gamestate::DoesCollide(const AABB& a, const AABB& b) {
        bool overlapX = std::abs(a.x - b.x) <= (a.ext.hw + b.ext.hw);
        bool overlapY = std::abs(a.y - b.y) <= (a.ext.hh + b.ext.hh);
        return overlapX && overlapY;
    }
}

