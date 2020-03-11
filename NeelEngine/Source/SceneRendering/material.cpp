#include "neel_engine_pch.h"

#include "material.h"

Material::Material()
{
}

Material::~Material()
{
}

void Material::Load(const fx::gltf::Document& document, int material_index, CommandList& command_list, std::vector<Texture>& texture_store, const std::string& filename)
{
	const fx::gltf::Material& material = document.materials[material_index];
	
	if(!material.pbrMetallicRoughness.empty())
	{
		auto& m = material;

		material_data_.BaseColorFactor		= XMFLOAT4(m.pbrMetallicRoughness.baseColorFactor.data());

		material_data_.MetallicFactor		= m.pbrMetallicRoughness.metallicFactor;
		material_data_.RoughnessFactor		= m.pbrMetallicRoughness.roughnessFactor;

		material_data_.NormalScale			= m.normalTexture.scale;

		material_data_.AoStrength			= m.occlusionTexture.strength;

		material_data_.EmissiveFactor		= XMFLOAT3(m.emissiveFactor.data());

		material_data_.BaseColorIndex		= m.pbrMetallicRoughness.baseColorTexture.index;
		material_data_.NormalIndex			= m.normalTexture.index;
		material_data_.MetalRoughIndex		= m.pbrMetallicRoughness.metallicRoughnessTexture.index;
		material_data_.OcclusionIndex		= m.occlusionTexture.index;
		material_data_.EmissiveIndex		= m.emissiveTexture.index;

		if (m.pbrMetallicRoughness.baseColorTexture.index >= 0)
		{
			const fx::gltf::Image& image = document.images[document.textures[m.pbrMetallicRoughness.baseColorTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(texture_store[m.pbrMetallicRoughness.baseColorTexture.index], file_name, TextureUsage::Albedo);
			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
		}
			
		if (m.normalTexture.index >= 0)
		{
			const fx::gltf::Image& image = document.images[document.textures[m.normalTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(texture_store[m.normalTexture.index], file_name, TextureUsage::Normalmap);
			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
		}
			
		if (m.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
		{
			const fx::gltf::Image& image = document.images[document.textures[m.pbrMetallicRoughness.metallicRoughnessTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(texture_store[m.pbrMetallicRoughness.metallicRoughnessTexture.index], file_name, TextureUsage::MetalRoughnessmap);

			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
		}
			
		if (m.occlusionTexture.index >= 0)
		{
		
			const fx::gltf::Image& image = document.images[document.textures[m.occlusionTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(texture_store[m.occlusionTexture.index], file_name, TextureUsage::AmbientOcclusionmap);

			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
		}
		
		if (m.emissiveTexture.index >= 0)
		{
			const fx::gltf::Image& image = document.images[document.textures[m.emissiveTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(texture_store[m.emissiveTexture.index], file_name, TextureUsage::Emissivemap);

			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
		}	
	}
	else
	{
		// Use a default material
		material_data_.BaseColorFactor	= DirectX::XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		material_data_.MetallicFactor	= 0;
		material_data_.RoughnessFactor	= 0.5f;
	}
}



