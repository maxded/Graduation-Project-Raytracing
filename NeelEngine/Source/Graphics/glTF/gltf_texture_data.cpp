#include "neel_engine_pch.h"

#include "gltf_texture_data.h"

TextureData::TextureData(const fx::gltf::Document& document, std::size_t texture_index, const std::string& filename)
{
	const fx::gltf::Image& image = document.images[document.textures[texture_index].source];

	const bool is_embedded = image.IsEmbeddedResource();

	if (!image.uri.empty() && !is_embedded)
	{
		texture_info_.Filename = fx::gltf::detail::GetDocumentRootPath(filename) + "/" + image.uri;
	}
	else
	{
		if(is_embedded)
		{
			image.MaterializeData(embedded_data_);
			
			texture_info_.BinaryData = &embedded_data_[0];
			texture_info_.BinarySize = static_cast<uint32_t>(embedded_data_.size());
		}
		else
		{
			const fx::gltf::BufferView& buffer_view = document.bufferViews[image.bufferView];
			const fx::gltf::Buffer& buffer = document.buffers[buffer_view.buffer];

			texture_info_.BinaryData = &buffer.data[buffer_view.byteOffset];
			texture_info_.BinarySize = buffer_view.byteLength;
		}
	}	
}
