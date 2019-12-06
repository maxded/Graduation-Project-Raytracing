#pragma once

#include "gltf.h"
#include <string>

class TextureData
{
public:
	struct TextureInfo
	{
		TextureInfo()
			: Filename("")
			, BinarySize(0)
			, BinaryData(nullptr)
		{}
		
		std::string Filename;

		uint32_t		BinarySize;
		const uint8_t*	BinaryData;

		bool IsBinary() const noexcept
		{
			return BinaryData != nullptr;
		}
	};

	TextureData() = delete;
	TextureData(const fx::gltf::Document& document, std::size_t texture_index, const std::string& filename);

	const TextureInfo& Info() const noexcept { return texture_info_; }
	
private:
	TextureInfo texture_info_;
	
	std::vector<uint8_t> embedded_data_;
};