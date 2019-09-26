#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_5.h>

#include <Events.h>
#include <HighResolutionClock.h>
#include <RenderTarget.h>

#include <string>
#include <memory>

class Game;

class Window : public std::enable_shared_from_this<Window>
{
public:
	// Number of swapchain back buffers.
	static const uint8_t BufferCount = 3;

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

	int GetClientWidth()  const;
	int GetClientHeight() const;

	/**
	* Should this window be rendered with vertical refresh synchronization?
	*/
	bool IsVSync() const;
	void SetVSync(bool vSync);
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
	void Show();

	/**
	 * Hide the window.
	 */
	void Hide();

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
	friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	// Only the application can create a window.
	friend class Application;
	// The DirectXTemplate class needs to register itself with a window.
	friend class Game;

	Window() = delete;
	Window(HWND hWnd, const std::wstring& windowName, uint16_t clientWidth, uint16_t clientHeight, bool vSync);
	virtual ~Window();

	// Register a Game with this window. This allows
	// the window to callback functions in the Game class.
	void RegisterCallbacks(std::shared_ptr<Game> pGame);

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

	HWND m_hWnd;

	std::wstring m_WindowName;

	uint16_t m_ClientWidth;
	uint16_t m_ClientHeight;
	bool m_VSync;
	bool m_Fullscreen;

	HighResolutionClock m_UpdateClock;
	HighResolutionClock m_RenderClock;

	UINT64 m_FenceValues[BufferCount];
	uint64_t m_FrameValues[BufferCount];

	std::weak_ptr<Game> m_pGame;

	Microsoft::WRL::ComPtr<IDXGISwapChain4>			m_dxgiSwapChain;
	Texture m_BackBufferTextures[BufferCount];
	// Marked mutable to allow modification in a const function.
	mutable RenderTarget m_RenderTarget;

	UINT m_CurrentBackBufferIndex;

	RECT m_WindowRect;
	bool m_IsTearingSupported;
};
