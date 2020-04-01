#pragma once

#include "game.h"
#include "render_target.h"
#include "root_signature.h"
#include "gltf_scene.h"

#include "shader_table.h"
#include "acceleration_structure.h"
#include "byte_address_buffer.h"

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

	void OnGui();

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

	void RescaleRenderTargets(float scale);

private:
	Scene scene_;

	RenderTarget geometry_pass_render_target_;
	RenderTarget light_accumulation_pass_render_target_;

	Texture raytracing_output_texture_;

	D3D12_SHADER_RESOURCE_VIEW_DESC depth_buffer_view_;

	RootSignature geometry_pass_root_signature_;
	RootSignature raytracing_global_root_signature_;
	RootSignature raytracing_local_root_signature_;
	RootSignature light_accumulation_pass_root_signature_;
	RootSignature composite_pass_root_signature_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> geometry_pass_pipeline_state_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> light_accumulation_pass_pipeline_state_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> composite_pass_pipeline_state_;

	Microsoft::WRL::ComPtr<ID3D12StateObject> raytracing_pass_state_object_;
	
	AccelerationStructure bottom_level_acceleration_structure_;
	AccelerationStructure top_level_acceleration_structure_;

	// Shader tables
	static const wchar_t* c_raygen_shader_;
	static const wchar_t* c_closesthit_shader_;
	static const wchar_t* c_miss_shader_;
	static const wchar_t* c_hitgroup_;

	ShaderTable raygen_shader_table_;
	ShaderTable hitgroup_shader_table_;
	ShaderTable miss_shader_table_;

	D3D12_RECT scissor_rect_;

	const Texture* current_display_texture_;

	// Set to true if the Shift key is pressed.
	bool shift_;
	int width_;
	int height_;
	// Scale the HDR render target to a fraction of the window size.
	float render_scale_;
	int maximum_ray_bounces_ = 6;

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

	/**
	* Scene constant data.
	*/

	struct SceneConstantBuffer
	{
		DirectX::XMMATRIX InverseViewProj;
		DirectX::XMMATRIX ViewProj;
		DirectX::XMVECTOR CamPos;
		float VFOV;
		float PixelHeight;
		int RayBounces = 2;
		float Padding1;
	};

	SceneConstantBuffer scene_buffer_;

	struct MeshInfoIndex
	{
		int MeshId;
	};

	ByteAddressBuffer global_vertices_;
	ByteAddressBuffer global_indices_;

	struct MeshInfo
	{
		MeshInfo()
			: IndicesOffset(0)
			, PositionAttributeOffset(0)
			, NormalAttributeOffset(0)
			, TangentAttributeOffset(0)
			, UvAttributeOffset(0)
			, PositionStride(0)
			, NormalStride(0)
			, TangentStride(0)
			, UvStride(0)
			, HasTangents(false)
			, MaterialId(-1)
		{}

		UINT IndicesOffset;
		UINT PositionAttributeOffset;
		UINT NormalAttributeOffset;
		UINT TangentAttributeOffset;
		UINT UvAttributeOffset;

		UINT PositionStride;
		UINT NormalStride;
		UINT TangentStride;
		UINT UvStride;
		
		bool HasTangents;
		int MaterialId;
	};

	std::vector<MeshInfo> mesh_infos_;
};