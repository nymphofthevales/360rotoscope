#pragma once
struct ButtonState {
	bool is_down;
	bool changed;
};

enum controls {
	MOVE_NORTH,
	MOVE_SOUTH,
	MOVE_WEST,
	MOVE_EAST,
	ONE,
	TWO,
	THREE,

	BUTTON_COUNT
};

struct Input {
	ButtonState buttons[BUTTON_COUNT];
};

#define isDown(b) input.buttons[b].is_down
#define pressed(b) (input.buttons[b].is_down && input.buttons[b].changed)
#define released(b) (!input->buttons[b].is_down && input->buttons[b].changed)
