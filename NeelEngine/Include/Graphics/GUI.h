#pragma once

#include "imgui.h"

#include <d3dx12.h>
#include <wrl.h>

class CommandList;
class Texture;
class RenderTarget;
class RootSignature;
class Window;

class GUI
{
public:
	GUI();
	virtual ~GUI();

	// Initialize the ImGui context.
	virtual bool Initialize(std::shared_ptr<Window> window);

	// Begin a new frame.
	virtual void NewFrame();
	virtual void Render(std::shared_ptr<CommandList> command_list, const RenderTarget& render_target);

	// Destroy the ImGui context.
	virtual void Destroy();

	// Set the scaling for this ImGuiContext.
	void SetScaling(float scale);

protected:

private:
	ImGuiContext* p_imgui_ctx_;
	std::unique_ptr<Texture> font_texture_;
	std::unique_ptr<RootSignature> root_signature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
	std::shared_ptr<Window> window_;
};