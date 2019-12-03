#pragma once

#include "key_codes.h"

// Base class for all event args
class EventArgs
{
public:
	EventArgs()
	{
	}
};

class KeyEventArgs : public EventArgs
{
public:
	enum KeyState
	{
		kReleased = 0,
		kPressed = 1
	};

	typedef EventArgs Base;

	KeyEventArgs(KeyCode::Key key, unsigned int c, KeyState state, bool control, bool shift, bool alt)
		: Key(key)
		  , Char(c)
		  , State(state)
		  , Control(control)
		  , Shift(shift)
		  , Alt(alt)
	{
	}

	KeyCode::Key Key; // The Key Code that was pressed or released.
	unsigned int Char;
	// The 32-bit character code that was pressed. This value will be 0 if it is a non-printable character.
	KeyState State; // Was the key pressed or released?
	bool Control; // Is the Control modifier pressed
	bool Shift; // Is the Shift modifier pressed
	bool Alt; // Is the Alt modifier pressed
};

class MouseMotionEventArgs : public EventArgs
{
public:
	typedef EventArgs Base;

	MouseMotionEventArgs(bool left_button, bool middle_button, bool right_button, bool control, bool shift, int x, int y)
		: LeftButton(left_button)
		  , MiddleButton(middle_button)
		  , RightButton(right_button)
		  , Control(control)
		  , Shift(shift)
		  , X(x)
		  , Y(y)
		  , RelX(0)
		  , RelY(0)
	{
	}

	bool LeftButton; // Is the left mouse button down?
	bool MiddleButton; // Is the middle mouse button down?
	bool RightButton; // Is the right mouse button down?
	bool Control; // Is the CTRL key down?
	bool Shift; // Is the Shift key down?

	int X; // The X-position of the cursor relative to the upper-left corner of the client area.
	int Y; // The Y-position of the cursor relative to the upper-left corner of the client area.
	int RelX; // How far the mouse moved since the last event.
	int RelY; // How far the mouse moved since the last event.
};

class MouseButtonEventArgs : public EventArgs
{
public:
	enum MouseButton
	{
		kNone = 0,
		kLeft = 1,
		kRight = 2,
		kMiddel = 3
	};

	enum ButtonState
	{
		kReleased = 0,
		kPressed = 1
	};

	typedef EventArgs Base;

	MouseButtonEventArgs(MouseButton button_id, ButtonState state, bool left_button, bool middle_button, bool right_button,
	                     bool control, bool shift, int x, int y)
		: Button(button_id)
		  , State(state)
		  , LeftButton(left_button)
		  , MiddleButton(middle_button)
		  , RightButton(right_button)
		  , Control(control)
		  , Shift(shift)
		  , X(x)
		  , Y(y)
	{
	}

	MouseButton Button; // The mouse button that was pressed or released.
	ButtonState State; // Was the button pressed or released?
	bool LeftButton; // Is the left mouse button down?
	bool MiddleButton; // Is the middle mouse button down?
	bool RightButton; // Is the right mouse button down?
	bool Control; // Is the CTRL key down?
	bool Shift; // Is the Shift key down?

	int X; // The X-position of the cursor relative to the upper-left corner of the client area.
	int Y; // The Y-position of the cursor relative to the upper-left corner of the client area.
};

class MouseWheelEventArgs : public EventArgs
{
public:
	typedef EventArgs Base;

	MouseWheelEventArgs(float wheel_delta, bool left_button, bool middle_button, bool right_button, bool control,
	                    bool shift, int x, int y)
		: WheelDelta(wheel_delta)
		  , LeftButton(left_button)
		  , MiddleButton(middle_button)
		  , RightButton(right_button)
		  , Control(control)
		  , Shift(shift)
		  , X(x)
		  , Y(y)
	{
	}

	float WheelDelta;
	// How much the mouse wheel has moved. A positive value indicates that the wheel was moved to the right. A negative value indicates the wheel was moved to the left.
	bool LeftButton; // Is the left mouse button down?
	bool MiddleButton; // Is the middle mouse button down?
	bool RightButton; // Is the right mouse button down?
	bool Control; // Is the CTRL key down?
	bool Shift; // Is the Shift key down?

	int X; // The X-position of the cursor relative to the upper-left corner of the client area.
	int Y; // The Y-position of the cursor relative to the upper-left corner of the client area.
};

class ResizeEventArgs : public EventArgs
{
public:
	typedef EventArgs Base;

	ResizeEventArgs(int width, int height)
		: Width(width)
		  , Height(height)
	{
	}

	// The new width of the window
	int Width;
	// The new height of the window.
	int Height;
};

class UpdateEventArgs : public EventArgs
{
public:
	typedef EventArgs Base;

	UpdateEventArgs(double f_delta_time, double f_total_time, uint64_t frame_number)
		: ElapsedTime(f_delta_time)
		  , TotalTime(f_total_time)
		  , FrameNumber(frame_number)
	{
	}

	double ElapsedTime;
	double TotalTime;
	uint64_t FrameNumber;
};

class RenderEventArgs : public EventArgs
{
public:
	typedef EventArgs Base;

	RenderEventArgs(double f_delta_time, double f_total_time, uint64_t frame_number)
		: ElapsedTime(f_delta_time)
		  , TotalTime(f_total_time)
		  , FrameNumber(frame_number)
	{
	}

	double ElapsedTime;
	double TotalTime;
	uint64_t FrameNumber;
};

class UserEventArgs : public EventArgs
{
public:
	typedef EventArgs Base;

	UserEventArgs(int code, void* data1, void* data2)
		: Code(code)
		  , Data1(data1)
		  , Data2(data2)
	{
	}

	int Code;
	void* Data1;
	void* Data2;
};
