#include "raytracing_scene.h"

#include "neel_engine.h"
#include "commandqueue.h"
#include "window.h"
#include "helpers.h"
#include "upload_buffer.h"

#include "CompiledShaders/Raytracing.hlsl.h"
#include "shader_table.h"
#include "camera.h"

const wchar_t* RayTracingScene::c_hit_group_name_			= L"MyHitGroup";
const wchar_t* RayTracingScene::c_raygen_shader_name_		= L"MyRaygenShader";
const wchar_t* RayTracingScene::c_closest_hit_shader_name_	= L"MyClosestHitShader";
const wchar_t* RayTracingScene::c_miss_shader_name_			= L"MyMissShader";

RayTracingScene::RayTracingScene()
{
}

RayTracingScene::~RayTracingScene()
{
}

void RayTracingScene::Load(const std::string & filename)
{
	auto device			= NeelEngine::Get().GetDevice();
	auto command_queue	= NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto command_list	= command_queue->GetCommandList();
	
	LoadFromFile(filename, *command_list);

	CreateRootSignatures();

	CreateRaytracingPipelineStateObject();

	BuildAccelerationStructures();

	BuildShaderTables();

	CreateRayTracingOutputResource();
}

void RayTracingScene::Update(UpdateEventArgs & e)
{
	Camera& camera = Camera::Get();

	scene_buffer_.ViewProj = camera.GetViewMatrix() * camera.GetProjectionMatrix();
	scene_buffer_.CamPos   = camera.GetTranslation();
}

void RayTracingScene::PrepareRender(CommandList & command_list)
{
	
}

void RayTracingScene::Render(CommandList & command_list)
{
	command_list.SetComputeRootSignature(raytracing_global_root_signature_);
	command_list.SetStateObject(raytracing_state_object_);
	
	// Bind the heaps, acceleration strucutre and dispatch rays.
	D3D12_DISPATCH_RAYS_DESC dispatch_desc = {};

	dispatch_desc.HitGroupTable.StartAddress	= hit_group_shader_table_->GetGPUVirtualAddress();
	dispatch_desc.HitGroupTable.SizeInBytes		= hit_group_shader_table_->GetDesc().Width;
	dispatch_desc.HitGroupTable.StrideInBytes	= dispatch_desc.HitGroupTable.SizeInBytes;
	dispatch_desc.MissShaderTable.StartAddress	= miss_shader_table_->GetGPUVirtualAddress();
	dispatch_desc.MissShaderTable.SizeInBytes	= miss_shader_table_->GetDesc().Width;
	dispatch_desc.MissShaderTable.StrideInBytes = dispatch_desc.MissShaderTable.SizeInBytes;
	dispatch_desc.RayGenerationShaderRecord.StartAddress	= raygen_shader_table_->GetGPUVirtualAddress();
	dispatch_desc.RayGenerationShaderRecord.SizeInBytes		= raygen_shader_table_->GetDesc().Width;
	dispatch_desc.Width		= 1280;
	dispatch_desc.Height	= 720;
	dispatch_desc.Depth		= 1;

	command_list.SetUnorderedAccessView(GlobalRootSignatureParams::RenderOutputSlot, 0, raytracing_output_);
	command_list.SetComputeAccelerationStructure(GlobalRootSignatureParams::AccelerationStructureSlot, top_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress());
	command_list.SetComputeDynamicConstantBuffer(GlobalRootSignatureParams::SceneConstantBuffer, scene_buffer_);
	
	command_list.DispatchRays(dispatch_desc);
}

RenderTarget & RayTracingScene::GetRenderTarget()
{
	render_target_.AttachTexture(AttachmentPoint::kColor0, raytracing_output_);
	return render_target_;
}

void RayTracingScene::CreateRootSignatures()
{
	auto device = NeelEngine::Get().GetDevice();
	
	// Check root signature highest version support.
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
	{
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Global Root Signature
	{
		CD3DX12_DESCRIPTOR_RANGE1 uav_descriptor;
		uav_descriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 root_parameters[GlobalRootSignatureParams::NumRootParameters];
		root_parameters[GlobalRootSignatureParams::RenderOutputSlot].InitAsDescriptorTable(1, &uav_descriptor);
		root_parameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
		root_parameters[GlobalRootSignatureParams::SceneConstantBuffer].InitAsConstantBufferView(0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(GlobalRootSignatureParams::NumRootParameters, root_parameters, 0, nullptr);

		raytracing_global_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}

	// Local Root Signature
	{
		CD3DX12_ROOT_PARAMETER1 root_parameters[LocalRootSignatureParams::NumRootParameters];
		root_parameters[LocalRootSignatureParams::ConstantBuffer].InitAsConstantBufferView(0, 0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(LocalRootSignatureParams::NumRootParameters, root_parameters, 0, nullptr);
		root_signature_description.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

		raytracing_local_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}
}

void RayTracingScene::CreateRaytracingPipelineStateObject()
{
	auto device = NeelEngine::Get().GetDevice();
	
	CD3DX12_STATE_OBJECT_DESC raytracing_pipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// DXIL library
	// This contains the shaders and their entrypoints for the state object.
	auto lib = raytracing_pipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing, ARRAYSIZE((g_pRaytracing)));
	lib->SetDXILLibrary(&libdxil);
	
	lib->DefineExport(c_raygen_shader_name_);
	lib->DefineExport(c_closest_hit_shader_name_);
	lib->DefineExport(c_miss_shader_name_);

	// Triangle hit group
	auto hit_group = raytracing_pipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	hit_group->SetClosestHitShaderImport(c_closest_hit_shader_name_);
	hit_group->SetHitGroupExport(c_hit_group_name_);
	hit_group->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	// Shader config
	// Defines the maximum sizes in bytes for the ray payload and attribute structure.
	auto shader_config	= raytracing_pipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payload_size	= 4 * sizeof(float);   // float4 color
	UINT attribute_size = 2 * sizeof(float); // float2 barycentrics
	shader_config->Config(payload_size, attribute_size);

	// Local root siganture and shader association
	//auto local_root_signature = raytracing_pipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	//local_root_signature->SetRootSignature(raytracing_local_root_signature_.GetRootSignature().Get());

	//auto root_signature_association = raytracing_pipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	//root_signature_association->SetSubobjectToAssociate(*local_root_signature);
	//root_signature_association->AddExport(c_raygen_shader_name_);

	// GLobal root signature
	auto global_root_signature = raytracing_pipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	global_root_signature->SetRootSignature(raytracing_global_root_signature_.GetRootSignature().Get());
	
	// Pipeline config
	auto pipeline_config = raytracing_pipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	pipeline_config->Config(1); // Max recusion of rays.
	
	// Create state object
	ThrowIfFailed(device->CreateStateObject(raytracing_pipeline, IID_PPV_ARGS(&raytracing_state_object_)));
}

void RayTracingScene::BuildAccelerationStructures()
{
	auto device			= NeelEngine::Get().GetDevice();
	auto command_queue	= NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list	= command_queue->GetCommandList();

	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs(document_data_.TotalNumberMeshes);

	int index = 0;	
	for(auto& geometry : document_data_.Meshes)
	{
		for (auto& submesh : geometry.GetSubMeshes())
		{
			geometry_descs[index].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;		
			geometry_descs[index].Triangles.IndexBuffer		= submesh.IBuffer.GetIndexBufferView().BufferLocation;
			geometry_descs[index].Triangles.IndexCount		= submesh.IBuffer.GetNumIndicies();
			geometry_descs[index].Triangles.IndexFormat		= submesh.IBuffer.GetIndexBufferView().Format;
			geometry_descs[index].Triangles.Transform3x4	= 0;
			geometry_descs[index].Triangles.VertexCount		= submesh.VBuffer.GetNumVertices();
			geometry_descs[index].Triangles.VertexFormat	= DXGI_FORMAT_R32G32B32_FLOAT;
			geometry_descs[index].Triangles.VertexBuffer.StartAddress	= submesh.VBuffer.GetVertexBufferViews()[0].BufferLocation;
			geometry_descs[index].Triangles.VertexBuffer.StrideInBytes  = submesh.VBuffer.GetVertexBufferViews()[0].StrideInBytes;
			geometry_descs[index].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

			index++;
		}
	}

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	// Top level 
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC top_level_build_desc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &top_level_inputs = top_level_build_desc.Inputs;
	top_level_inputs.DescsLayout	= D3D12_ELEMENTS_LAYOUT_ARRAY;
	top_level_inputs.Flags			= build_flags;
	top_level_inputs.NumDescs		= 1;
	top_level_inputs.pGeometryDescs = nullptr;
	top_level_inputs.Type			= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	// Bottom level 
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottom_level_build_desc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &bottom_level_inputs = bottom_level_build_desc.Inputs;
	bottom_level_inputs.DescsLayout		= D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottom_level_inputs.Flags			= build_flags;
	bottom_level_inputs.NumDescs		= document_data_.TotalNumberMeshes;
	bottom_level_inputs.pGeometryDescs	= &geometry_descs[0];
	bottom_level_inputs.Type			= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	// Allocate space on GPU to build acceleration structures
	bottom_level_acceleration_structure_.AllocateAccelerationStructureBuffer(bottom_level_inputs, *command_list);
	top_level_acceleration_structure_.AllocateAccelerationStructureBuffer(top_level_inputs, *command_list);
	
	// Create an instance desc for the bottom-level acceleration structure.
	D3D12_RAYTRACING_INSTANCE_DESC instance_desc = {};
	instance_desc.Transform[0][0] = instance_desc.Transform[1][1] = instance_desc.Transform[2][2] = 1;
	instance_desc.InstanceMask = 1;
	instance_desc.AccelerationStructure = bottom_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress();

	UploadBuffer upload_buffer;
	
	auto heap_allocation = upload_buffer.Allocate(sizeof(instance_desc), sizeof(instance_desc));
	memcpy(heap_allocation.CPU, &instance_desc, sizeof(instance_desc));

	{
		bottom_level_build_desc.ScratchAccelerationStructureData = bottom_level_acceleration_structure_.GetScratchResource()->GetGPUVirtualAddress();
		bottom_level_build_desc.DestAccelerationStructureData    = bottom_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress();
	}
	
	{
		top_level_build_desc.ScratchAccelerationStructureData = top_level_acceleration_structure_.GetScratchResource()->GetGPUVirtualAddress();
		top_level_build_desc.DestAccelerationStructureData    = top_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress();
		top_level_build_desc.Inputs.InstanceDescs = heap_allocation.GPU;
	}

	// Build ray tracing acceleration structures.
	command_list->GetGraphicsCommandList()->BuildRaytracingAccelerationStructure(&bottom_level_build_desc, 0, nullptr);
	command_list->UAVBarrier(bottom_level_acceleration_structure_);
	command_list->GetGraphicsCommandList()->BuildRaytracingAccelerationStructure(&top_level_build_desc, 0, nullptr);
	
	// Execute command list and wait for fence value.
	uint64_t fence = 0;
	fence = command_queue->ExecuteCommandList(command_list);
	command_queue->WaitForFenceValue(fence);

	// Reset the scratch resources.
	bottom_level_acceleration_structure_.GetScratchResource().Reset();
	top_level_acceleration_structure_.GetScratchResource().Reset();
}

void RayTracingScene::BuildShaderTables()
{
	auto device = NeelEngine::Get().GetDevice();

	void* raygen_shader_id;
	void* miss_shader_id;
	void* hitgroup_shader_id;

	auto GetShaderidentifiers = [&](auto* state_object_properties)
	{
		raygen_shader_id = state_object_properties->GetShaderIdentifier(c_raygen_shader_name_);
		miss_shader_id = state_object_properties->GetShaderIdentifier(c_miss_shader_name_);
		hitgroup_shader_id = state_object_properties->GetShaderIdentifier(c_hit_group_name_);
	};

	UINT shader_id_size;
	{
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> state_object_properties;
		ThrowIfFailed(raytracing_state_object_.As(&state_object_properties));
		GetShaderidentifiers(state_object_properties.Get());
		shader_id_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	}

	// RayGen Shader Table
	{
		UINT num_shader_records = 1;
		UINT shader_record_size = shader_id_size;
		ShaderTable raygen_shader_table(num_shader_records, shader_record_size, "RayGenShaderTable");
		raygen_shader_table.push_back(ShaderRecord(raygen_shader_id, shader_id_size));

		raygen_shader_table_ = raygen_shader_table.GetD3D12Resource();
	}
	
	// Miss Shader Table
	{
		UINT num_shader_records = 1;
		UINT shader_record_size = shader_id_size;
		ShaderTable miss_shader_table(num_shader_records, shader_record_size, "MissShaderTable");
		miss_shader_table.push_back(ShaderRecord(miss_shader_id, shader_id_size));
		
		miss_shader_table_ = miss_shader_table.GetD3D12Resource();
	}
	
	// Hit group Shader Table
	{
		UINT num_shader_records = 1;
		UINT shader_record_size = shader_id_size;
		ShaderTable hitgroup_shader_table(num_shader_records, shader_record_size, "HitGroupShaderTable");
		hitgroup_shader_table.push_back(ShaderRecord(hitgroup_shader_id, shader_id_size));

		hit_group_shader_table_ = hitgroup_shader_table.GetD3D12Resource();
	}
}

void RayTracingScene::CreateRayTracingOutputResource()
{
	auto device = NeelEngine::Get().GetDevice();

	int width	= NeelEngine::Get().GetWindow()->GetClientWidth();
	int height	= NeelEngine::Get().GetWindow()->GetClientHeight();	
	auto format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	
	auto uav_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource;
	
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
		, D3D12_HEAP_FLAG_NONE
		, &uav_desc
		, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		, nullptr
		, IID_PPV_ARGS(&d3d12_resource)));

	raytracing_output_.SetD3D12Resource(d3d12_resource);
	raytracing_output_.SetName("RaytracingOutput Texture");
}
