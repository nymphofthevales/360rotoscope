#include <windows.h>
#include <cassert>
#include <stdio.h>
#include <iostream>
#include "input.h"

bool running{ true };

struct RenderState {
	int height, width;
	void* memory;
	BITMAPINFO bitmapInfo;
	double scale; // scale will be computed based on the difference of client's screen from 1920*1080
};

RenderState renderState;

#include "Image.cpp"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;

	switch (uMsg)
	{
	case WM_CLOSE:
	case WM_DESTROY: {
		running = false;
	}break;

	case WM_SIZE: {
		RECT rect;
		GetClientRect(hwnd, &rect);
		renderState.width = rect.right - rect.left;
		renderState.height = rect.bottom - rect.top;

		renderState.scale = renderState.width / 1920;

		int bufferSize = renderState.width * renderState.height * sizeof(unsigned int);

		if (renderState.memory)
			VirtualFree(renderState.memory, 0, MEM_RELEASE);
		renderState.memory = VirtualAlloc(0, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		renderState.bitmapInfo.bmiHeader.biSize = sizeof(renderState.bitmapInfo.bmiHeader);
		renderState.bitmapInfo.bmiHeader.biWidth = renderState.width;
		renderState.bitmapInfo.bmiHeader.biHeight = -renderState.height;
		renderState.bitmapInfo.bmiHeader.biPlanes = 1;
		renderState.bitmapInfo.bmiHeader.biBitCount = 32;
		renderState.bitmapInfo.bmiHeader.biCompression = BI_RGB;
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
	} break;
	default: {
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	}

	return result;
}

int main() {
	//create and register window
	WNDCLASSA windowClass{};
	windowClass.style = 0;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.lpszClassName = "Basic Window";

	HINSTANCE hInstance = GetModuleHandle(NULL);

	RegisterClassA(&windowClass);
	HWND windowHandle = CreateWindowA(windowClass.lpszClassName, 0, WS_BORDER | WS_POPUP | WS_MAXIMIZE | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 500, 300, 0, 0, hInstance, 0);
	HDC hdc = GetDC(windowHandle);


// initialize the variables used to hold messages and input
	MSG message;
	Input input{};

	//calculating delta time, intialization
	ULONGLONG startFrame = GetTickCount64();
	ULONGLONG endFrame;
	double dt = 0.16666;

	// main game loop
	while (running)
	{
		// set all buttons to changed == false for new frame
		for (int i = 0; i < BUTTON_COUNT; i++) {
			input.buttons[i].changed = false;
		}

		// check for windows messages, including getting input
		while (PeekMessage(&message, windowHandle, 0, 0, PM_REMOVE))
		{

			switch (message.message) {
			case WM_KEYUP:
			case WM_KEYDOWN: {
				unsigned int vk_code = (unsigned int)message.wParam;
				bool is_down = ((message.lParam & (1 << 31)) == 0);


#define process_button(b, vk)\
case vk: {\
input.buttons[b].changed = is_down != input.buttons[b].is_down;\
input.buttons[b].is_down = is_down; \
}break;

				switch (vk_code) {
					process_button(MOVE_NORTH, VK_UP);
					process_button(MOVE_SOUTH, VK_DOWN);
					process_button(MOVE_WEST, VK_LEFT);
					process_button(MOVE_EAST, VK_RIGHT);
					process_button(ONE, 0x31);
					process_button(TWO, 0x32);
					process_button(THREE, 0x33);

				}

			}break;

			default: {
				TranslateMessage(&message);
				DispatchMessage(&message);
			}

			}

		}

		// update and draw each frame

		//render all images


		//calculating delta time at frame end time
		endFrame = GetTickCount64();
		dt = (double)(endFrame - startFrame) / 1000;
		startFrame = endFrame;
	}
}


// This is the framework I put together for my Crescent Rose project. It would require tweaking for other purpose or more features, but it is a working layout for a window, input, and drawing to it. 
// The important things are: dt is delta time, input is held in the input struct defined in input.h and you'll have to change the enum there and the corresponding code under the #define in the main loop
// for the input you are looking for, you initialize an image using the Image class in image.cpp and use Image.passToBuffer to pass it to your main rendering buffer (renderState), and that the image class
// isn't necessarily safe. It shouldn't have any problems but it has potential to throw errors and not catch them. It's probably much faster to run this a couple times, figure it out, and then adapt it your
// purposes than remaking a way to draw windows. However, this cannot be considered a completed framework, infailliable and friendly. It is better said as a problem once solved.