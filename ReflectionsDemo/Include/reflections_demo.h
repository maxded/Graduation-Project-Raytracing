#pragma once

#include "game.h"
#include "render_target.h"
#include "root_signature.h"
#include "gltf_scene.h"

class ReflectionsDemo : public Game
{
public:
	ReflectionsDemo(const std::wstring& name, int width, int height, bool v_sync = false);
	virtual ~ReflectionsDemo();

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
	std::unique_ptr<Scene> current_scene_;

	// HDR -> SDR tone mapping Rootsignature + PSO.
	RootSignature sdr_root_signature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> sdr_pipeline_state_;

	// Set to true if the Shift key is pressed.
	bool shift_;

	int width_;
	int height_;

	// Scale the HDR render target to a fraction of the window size.
	float render_scale_;
};