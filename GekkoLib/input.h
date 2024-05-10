#pragma once

#include "gekko_types.h"
#include <deque>
#include <memory>

namespace Gekko {
	struct GameInput {
		static const i32 NULL_FRAME = -1;

		void Init(GameInput& other);

		void Init(Frame frame_num, Input inp, u32 inp_len);

		bool IsEqualTo(Input other);

		void Clear();

		GameInput();
		~GameInput();

		Frame frame;
		Input input;
		u32 input_len;
	};

	struct InputBuffer {
		static const u32 BUFF_SIZE = 128;

		InputBuffer();

		void Init(u8 delay, u8 input_window, u32 input_size);

		void AddLocalInput(Frame frame, Input input);

		void AddInput(Frame frame, Input input);

		void SetDelay(u8 delay);

		u8 GetDelay();
		
		void SetInputPredictionWindow(u8 input_window);

		Frame GetIncorrectPredictionFrame();

		std::unique_ptr<GameInput> GetInput(Frame frame, bool prediction = true);

	private:
		void ResetPrediction();

		u32 PreviousFrame(Frame frame);

		bool HandleInputPrediction(Frame frame);

	private:
		u8 _input_delay;
		u8 _input_prediction_window;

		u32 _input_size;

		Frame _last_received_input;

		Frame _last_predicted_input;
		Frame _first_predicted_input;
		Frame _incorrent_predicted_input;

		std::deque<GameInput> _inputs;
	};
}