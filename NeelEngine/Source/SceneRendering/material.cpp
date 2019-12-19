#include "neel_engine_pch.h"

#include "material.h"

Material::Material()
	: shader_configurations_()
{
	// Create empty textures for rendering purposes.
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

	auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, 360, 240);

	albedo_texture_				= Texture(resource_desc, nullptr, TextureUsage::Albedo, "albedo_texture");
	normal_texture_				= Texture(resource_desc, nullptr, TextureUsage::Normalmap, "normal_texture");
	metallic_roughness_texture_ = Texture(resource_desc, nullptr, TextureUsage::MetalRoughnessmap, "metal_rough_texture");
	ambient_occlusion_texture_	= Texture(resource_desc, nullptr, TextureUsage::AmbientOcclusionmap, "ao_texture");
	emissive_texture_			= Texture(resource_desc, nullptr, TextureUsage::Emissivemap, "emissive_texture");
}

Material::~Material()
{
}

void Material::Load(const fx::gltf::Document& document, int material_index, CommandList& command_list, const std::string& filename)
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

		ShaderOptions options = ShaderOptions::None;

		if (m.pbrMetallicRoughness.baseColorTexture.index >= 0)
		{
			options |= ShaderOptions::HAS_BASECOLORMAP;

			const fx::gltf::Image& image = document.images[document.textures[m.pbrMetallicRoughness.baseColorTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(albedo_texture_, file_name, TextureUsage::Albedo);

			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
	
		}
			
		if (m.normalTexture.index >= 0)
		{
			options |= ShaderOptions::HAS_NORMALMAP;

			const fx::gltf::Image& image = document.images[document.textures[m.normalTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(normal_texture_, file_name, TextureUsage::Normalmap);

			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
		}
			
		if (m.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
		{
			options |= ShaderOptions::HAS_METALROUGHNESSMAP;

			const fx::gltf::Image& image = document.images[document.textures[m.pbrMetallicRoughness.metallicRoughnessTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(metallic_roughness_texture_, file_name, TextureUsage::MetalRoughnessmap);

			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
		}
			
		if (m.occlusionTexture.index >= 0)
		{
			options |= ShaderOptions::HAS_OCCLUSIONMAP;

			if (m.occlusionTexture.index == m.pbrMetallicRoughness.metallicRoughnessTexture.index)
			{
				options |= ShaderOptions::HAS_OCCLUSIONMAP_COMBINED;
			}
			else
			{
				const fx::gltf::Image& image = document.images[document.textures[m.occlusionTexture.index].source];

				std::string file_name = "";

				if (!image.uri.empty() && !image.IsEmbeddedResource())
				{
					file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
					command_list.LoadTextureFromFile(ambient_occlusion_texture_, file_name, TextureUsage::AmbientOcclusionmap);

				}
				else
				{
					throw std::exception("embedded glTF textures not supported!");
				}
			}
		}
		
		if (m.emissiveTexture.index >= 0)
		{
			options |= ShaderOptions::HAS_EMISSIVEMAP;

			const fx::gltf::Image& image = document.images[document.textures[m.emissiveTexture.index].source];

			std::string file_name = "";

			if (!image.uri.empty() && !image.IsEmbeddedResource())
			{
				file_name = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
				command_list.LoadTextureFromFile(emissive_texture_, file_name, TextureUsage::Emissivemap);

			}
			else
			{
				throw std::exception("embedded glTF textures not supported!");
			}
		}
			
		shader_configurations_ = options;
	}
	else
	{
		// Use a default material
		shader_configurations_			= ShaderOptions::None;
		material_data_.BaseColorFactor	= DirectX::XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		material_data_.MetallicFactor	= 0;
		material_data_.RoughnessFactor	= 0.5f;
	}
}



