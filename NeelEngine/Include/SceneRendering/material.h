#pragma once

#include "shader_data.h"
#include "commandlist.h"
#include "texture.h"
#include "shader_options.h"

#include "gltf.h"

class Material
{
	friend class Mesh;
public:
	Material();
	~Material();

	void Load(const fx::gltf::Document& document, int material_index, CommandList& command_list, const std::string& filename);

	const MeshMaterialData& GetMaterialData() const { return material_data_; }
	const ShaderOptions& GetShaderOptions() const { return shader_configurations_; }
	
protected:
	void HasTangents() { shader_configurations_ |= ShaderOptions::HAS_TANGENTS;  }

	// For Light source visualization.
	void SetEmissive(DirectX::XMFLOAT3 color) { material_data_.EmissiveFactor = color; }

	const Texture& Albedo()				const { return albedo_texture_; }
	const Texture& Normal()				const { return normal_texture_; }
	const Texture& MetalRoughness()		const { return metallic_roughness_texture_; }
	const Texture& AmbientOcclusion()	const { return ambient_occlusion_texture_; }
	const Texture& Emissive()			const { return emissive_texture_; }
	
private:
	MeshMaterialData	material_data_;
	ShaderOptions		shader_configurations_;

	Texture albedo_texture_;
	Texture normal_texture_;
	Texture metallic_roughness_texture_;
	Texture ambient_occlusion_texture_;
	Texture emissive_texture_;
};