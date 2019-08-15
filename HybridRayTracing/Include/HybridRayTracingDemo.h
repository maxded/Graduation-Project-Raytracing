#pragma once
#include <Game.h>
#include <Window.h>

#include <DirectXMath.h>

class HybridRayTracingDemo : public Game
{
public:
	using game = Game;

	HybridRayTracingDemo(const std::wstring& name, int width, int height, bool vSync = false);
	/**
	 *  Load content required for the demo.
	 */
	virtual bool LoadContent() override;

	/**
	 *  Unload demo specific content that was loaded in LoadContent.
	 */
	virtual void UnloadContent() override;
protected:
	/**
	*  Update the game logic.
	*/
	virtual void OnUpdate(UpdateEventArgs& e) override;

	/**
	 *  Render stuff.
	 */
	virtual void OnRender(RenderEventArgs& e) override;

	/**
	 * Invoked by the registered window when a key is pressed
	 * while the window has focus.
	 */
	virtual void OnKeyPressed(KeyEventArgs& e) override;

	/**
	 * Invoked when the mouse wheel is scrolled while the registered window has focus.
	 */
	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;


	virtual void OnResize(ResizeEventArgs& e) override;

private:
	uint64_t		m_FenceValues[Window::BufferCount] = {};
	D3D12_VIEWPORT	m_Viewport;

	bool m_ContentLoaded;
};