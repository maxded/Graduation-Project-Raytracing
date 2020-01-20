#pragma once

#include "game.h"
#include "render_target.h"
#include "root_signature.h"
#include "gltf_scene.h"

#include "acceleration_structure.h"

class ReflectionsDemo : public Game
{
public:
	ReflectionsDemo(const std::wstring& name, int width, int height, bool v_sync = false);
	virtual ~ReflectionsDemo();

	/**
	 *  Load content required for the demo.
	 */
	bool LoadContent() override;

	/**
	 *  Unload demo specific content that was loaded in LoadContent.
	 */
	void UnloadContent() override;

protected:
	/**
	*  Update the game logic.
	*/
	void OnUpdate(UpdateEventArgs& e) override;

	/**
	 *  Render stuff.
	 */
	void OnRender(RenderEventArgs& e) override;

	/**
	 * Invoked by the registered window when a key is pressed
	 * while the window has focus.
	 */
	void OnKeyPressed(KeyEventArgs& e) override;

	void OnKeyReleased(KeyEventArgs& e) override;

	void OnMouseMoved(MouseMotionEventArgs& e) override;

	void OnMouseWheel(MouseWheelEventArgs& e) override;

	void OnResize(ResizeEventArgs& e) override;

	void RescaleHDRRenderTarget(float scale);

private:
	Scene scene_;

	RootSignature hdr_root_signature_;
	RootSignature raytracing_global_root_signature_;
	RootSignature sdr_root_signature_;

	RenderTarget hdr_render_target_;

	Texture shadow_texture_;

	D3D12_SHADER_RESOURCE_VIEW_DESC depth_buffer_view_;

	AccelerationStructure bottom_level_acceleration_structure_;
	AccelerationStructure top_level_acceleration_structure_;

	// Shader tables
	static const wchar_t* c_raygen_shadow_pass_;
	static const wchar_t* c_miss_shadow_pass_;

	Microsoft::WRL::ComPtr<ID3D12Resource> miss_shader_table_;
	Microsoft::WRL::ComPtr<ID3D12Resource> raygen_shader_table_;

	Microsoft::WRL::ComPtr<ID3D12StateObject> shadow_pass_state_object_;

	// Map containing all different pipeline states for each mesh with different shader options (permutations).
	std::unordered_map<ShaderOptions, Microsoft::WRL::ComPtr<ID3D12PipelineState>> hdr_pipeline_state_map_;
	
	Microsoft::WRL::ComPtr<ID3D12PipelineState> sdr_pipeline_state_;

	D3D12_RECT scissor_rect_;

	// Set to true if the Shift key is pressed.
	bool shift_;
	int width_;
	int height_;
	// Scale the HDR render target to a fraction of the window size.
	float render_scale_;

	/**
	* Scene light properties for shaders.
	*/
	struct SceneLightProperties
	{
		SceneLightProperties()
			: NumPointLights(0)
			, NumSpotLights(0)
			, NumDirectionalLights(0)
		{
		}

		uint32_t NumPointLights;
		uint32_t NumSpotLights;
		uint32_t NumDirectionalLights;
	};

	SceneLightProperties light_properties_;

	std::vector<PointLight>			point_lights_;
	std::vector<SpotLight>			spot_lights_;
	std::vector<DirectionalLight>	directional_lights_;
	
	bool animate_lights_;
	bool visualise_lights_;

	/**
	* Scene constant data.
	*/
	struct SceneConstantBuffer
	{
		DirectX::XMMATRIX InverseViewProj;
		DirectX::XMVECTOR CamPos;
	};

	SceneConstantBuffer scene_buffer_;
};