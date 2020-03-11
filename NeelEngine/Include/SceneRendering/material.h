#pragma once

#include "shader_data.h"
#include "commandlist.h"
#include "texture.h"

#include "gltf.h"

class Material
{
	friend class Mesh;
public:
	Material();
	~Material();

	void Load(const fx::gltf::Document& document, int material_index, CommandList& command_list, std::vector<Texture>& texture_store, const std::string& filename);

	const MeshMaterialData& GetMaterialData() const { return material_data_; }
	
protected:
	// For Light source visualization.
	void SetEmissive(DirectX::XMFLOAT3 color) { material_data_.EmissiveFactor = color; }	
private:
	MeshMaterialData	material_data_;
};