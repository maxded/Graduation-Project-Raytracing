#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <wrl.h>
#include <dxgi1_5.h>

#include "events.h"
#include "high_resolution_clock.h"
#include "render_target.h"

#include <string>
#include <memory>

class Game;

class Window : public std::enable_shared_from_this<Window>
{
public:
	// Number of swapchain back buffers.
	static const uint8_t buffer_count_ = 3;

	/**
	* Get a handle to this window's instance.
	* @returns The handle to the window instance or nullptr if this is not a valid window.
	*/
	HWND GetWindowHandle() const;

	/**
	* Destroy this window.
	*/
	void Destroy();

	const std::wstring& GetWindowName() const;

	int GetClientWidth() const;
	int GetClientHeight() const;

	/**
	* Should this window be rendered with vertical refresh synchronization?
	*/
	bool IsVSync() const;
	void SetVSync(bool v_sync);
	void ToggleVSync();

	/**
	* Should this window be fullscreen?
	*/
	bool IsFullScreen() const;
	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen();

	/**
	 * Show this window.
	 */
	void Show() const;

	/**
	 * Hide the window.
	 */
	void Hide() const;

	/**
	 * Get the render target of the window. This method should be called every
	 * frame since the color attachment point changes depending on the window's
	 * current back buffer.
	 */
	const RenderTarget& GetRenderTarget() const;

	/**
	 * Present the swapchain's back buffer to the screen.
	 * Returns the current back buffer index after the present.
	 */
	UINT Present(const Texture& texture = Texture());

protected:
	// The Window procedure needs to call protected methods of this class.
	friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

	// Only the application can create a window.
	friend class NeelEngine;
	// The DirectXTemplate class needs to register itself with a window.
	friend class Game;

	Window() = delete;
	Window(HWND h_wnd, const std::wstring& window_name, uint16_t client_width, uint16_t client_height, bool v_sync);
	virtual ~Window();

	// Register a Game with this window. This allows
	// the window to callback functions in the Game class.
	void RegisterCallbacks(std::shared_ptr<Game> p_game);

	// Update and Draw can only be called by the application.
	virtual void OnUpdate(UpdateEventArgs& e);
	virtual void OnRender(RenderEventArgs& e);

	// A keyboard key was pressed
	virtual void OnKeyPressed(KeyEventArgs& e);
	// A keyboard key was released
	virtual void OnKeyReleased(KeyEventArgs& e);

	// The mouse was moved
	virtual void OnMouseMoved(MouseMotionEventArgs& e);
	// A button on the mouse was pressed
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
	// A button on the mouse was released
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
	// The mouse wheel was moved.
	virtual void OnMouseWheel(MouseWheelEventArgs& e);

	// The window was resized.
	virtual void OnResize(ResizeEventArgs& e);

	// Create the swapchian.
	Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain();

	// Update the render target views for the swapchain back buffers.
	void UpdateRenderTargetViews();

private:
	// Windows should not be copied.
	Window(const Window& copy) = delete;
	Window& operator=(const Window& other) = delete;

	HWND h_wnd_;

	std::wstring window_name_;

	uint16_t client_width_;
	uint16_t client_height_;
	bool v_sync_;
	bool fullscreen_;

	int previous_mouse_x_;
	int previous_mouse_y_;

	HighResolutionClock update_clock_;
	HighResolutionClock render_clock_;

	UINT64 fence_values_[buffer_count_];
	uint64_t frame_values_[buffer_count_];

	std::weak_ptr<Game> p_game_;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> dxgi_swap_chain_;
	Texture back_buffer_textures_[buffer_count_];
	// Marked mutable to allow modification in a const function.
	mutable RenderTarget render_target_;

	UINT current_back_buffer_index_{};

	RECT window_rect_{};
	bool is_tearing_supported_;
};
