#pragma once

#include <Game.h>
#include <Light.h>
#include <RenderTarget.h>
#include <RootSignature.h>
#include <Scene.h>

class HybridRayTracingDemo : public Game
{
public:
	HybridRayTracingDemo(const std::wstring& name, int width, int height, bool vSync = false);
	virtual ~HybridRayTracingDemo();

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

	virtual void OnKeyReleased(KeyEventArgs& e) override;

	virtual void OnMouseMoved(MouseMotionEventArgs& e) override;

	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;

	virtual void OnResize(ResizeEventArgs& e) override;

	void RescaleHDRRenderTarget(float scale);

private:
	std::vector<std::shared_ptr<Scene>>		m_Scenes;
	int										m_CurrentSceneIndex;

	// HDR Render target
	RenderTarget m_HDRRenderTarget;

	// Root signatures
	RootSignature m_HDRRootSignature;
	RootSignature m_SDRRootSignature;

	// Pipeline state object.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_HDRPipelineState;
	// HDR -> SDR tone mapping PSO.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SDRPipelineState;

	D3D12_RECT m_ScissorRect;

	// Set to true if the Shift key is pressed.
	bool m_Shift;

	int m_Width;
	int m_Height;

	// Scale the HDR render target to a fraction of the window size.
	float m_RenderScale;
};