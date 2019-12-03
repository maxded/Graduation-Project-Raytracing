#pragma once

#include <stdint.h>
#include <memory>
#include <string>

#include "events.h"

class Window;

class Game : public std::enable_shared_from_this<Game>
{
public:
	Game(const std::wstring& name, uint16_t width, uint16_t height, bool vsync);
	virtual ~Game();

	int GetClientWidth() const
	{
		return width_;
	}

	int GetClientHeight() const
	{
		return height_;
	}

	/**
	 *  Initialize the DirectX Runtime.
	 */
	virtual bool Initialize();

	/**
	 *  Load content required for the demo.
	 */
	virtual bool LoadContent() = 0;

	/**
	 *  Unload demo specific content that was loaded in LoadContent.
	 */
	virtual void UnloadContent() = 0;

	/**
	 * Destroy any resource that are used by the game.
	 */
	virtual void Destroy();

protected:
	friend class Window;

	/**
	 *  Update the game logic.
	 */
	virtual void OnUpdate(UpdateEventArgs& e);

	/**
	 *  Render stuff.
	 */
	virtual void OnRender(RenderEventArgs& e);

	/**
	 * Invoked by the registered window when a key is pressed
	 * while the window has focus.
	 */
	virtual void OnKeyPressed(KeyEventArgs& e);

	/**
	 * Invoked when a key on the keyboard is released.
	 */
	virtual void OnKeyReleased(KeyEventArgs& e);

	/**
	 * Invoked when the mouse is moved over the registered window.
	 */
	virtual void OnMouseMoved(MouseMotionEventArgs& e);

	/**
	 * Invoked when a mouse button is pressed over the registered window.
	 */
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);

	/**
	 * Invoked when a mouse button is released over the registered window.
	 */
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);

	/**
	 * Invoked when the mouse wheel is scrolled while the registered window has focus.
	 */
	virtual void OnMouseWheel(MouseWheelEventArgs& e);

	/**
	 * Invoked when the attached window is resized.
	 */
	virtual void OnResize(ResizeEventArgs& e);

	/**
	 * Invoked when the registered window instance is destroyed.
	 */
	virtual void OnWindowDestroy();

	std::shared_ptr<Window> p_window_;

private:
	std::wstring name_;
	int width_;
	int height_;
	bool vsync_;

	// Camera controller
	float forward_;
	float backward_;
	float left_;
	float right_;
	float up_;
	float down_;

	float pitch_;
	float yaw_;
};
