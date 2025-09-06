#include "game.h"
//#include <cstdio>

namespace GekkoGame {
    void Gamestate::Init(int num_players) {
        state = {};
        state.flags.players = num_players - 1;
        state.flags.balls = 0;
        state.flags.started = true;
        state.flags.finished = false;
        // setup pedal positions
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
            state.e_vx[i] = -8000;
            state.e_vy[i] = 5000;
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
            ///printf("[%d] x:%d,y:%d\n", i, state.e_px[i], state.e_py[i]);
            if (i <= 1) {
                if (state.e_py[i] < 0) state.e_py[i] = FIELD_SIZE * GAME_SCALE;
                if (state.e_py[i] > FIELD_SIZE * GAME_SCALE) state.e_py[i] = 0;
            } else {
                if (state.e_px[i] < 0) state.e_px[i] = FIELD_SIZE * GAME_SCALE;
                if (state.e_px[i] > FIELD_SIZE * GAME_SCALE) state.e_px[i] = 0;
            }
        }

        // make balls bounce of walls
        for (int i = MAX_PLAYERS; i <= MAX_PLAYERS + state.flags.balls; i++){
            if (state.e_px[i] < 0 || state.e_px[i] > FIELD_SIZE * GAME_SCALE) state.e_vx[i] *= -1;
            if (state.e_py[i] < 0 || state.e_py[i] > FIELD_SIZE * GAME_SCALE) state.e_vy[i] *= -1;
        }
    }
}

