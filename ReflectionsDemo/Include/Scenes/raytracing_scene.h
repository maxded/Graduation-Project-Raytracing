#pragma once

#include "gltf_scene.h"
#include "render_target.h"
#include "root_signature.h"
#include "acceleration_structure.h"

namespace GlobalRootSignatureParams
{
	enum 
	{
		RenderOutputSlot = 0,
		AccelerationStructureSlot,
		SceneConstantBuffer,
		NumRootParameters
	};
}

namespace LocalRootSignatureParams
{
	enum 
	{
		ConstantBuffer = 0,
		NumRootParameters
	};
}

class RayTracingScene : public Scene
{
public:
	RayTracingScene();
	virtual ~RayTracingScene();

	void Load(const std::string& filename) override;

	void Update(UpdateEventArgs& e) override;

	void PrepareRender(CommandList& command_list) override;

	void Render(CommandList& command_list) override;

	RenderTarget& GetRenderTarget() override;
protected:
	void CreateRootSignatures();

	void CreateRaytracingPipelineStateObject();
	
	void BuildAccelerationStructures();

	void BuildShaderTables();

	void CreateRayTracingOutputResource();

private:
	RootSignature	raytracing_global_root_signature_;
	RootSignature	raytracing_local_root_signature_;
	
	Texture raytracing_output_;
	RenderTarget render_target_;

	AccelerationStructure bottom_level_acceleration_structure_;
	AccelerationStructure top_level_acceleration_structure_;

	// Shader tables
	static const wchar_t* c_hit_group_name_;
	static const wchar_t* c_raygen_shader_name_;
	static const wchar_t* c_closest_hit_shader_name_;
	static const wchar_t* c_miss_shader_name_;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> miss_shader_table_;
	Microsoft::WRL::ComPtr<ID3D12Resource> hit_group_shader_table_;
	Microsoft::WRL::ComPtr<ID3D12Resource> raygen_shader_table_;

	Microsoft::WRL::ComPtr<ID3D12StateObject> raytracing_state_object_;
	
	struct SceneConstantBuffer
	{
		DirectX::XMMATRIX InverseViewProj;
		DirectX::XMVECTOR CamPos;
	};

	SceneConstantBuffer scene_buffer_;
};