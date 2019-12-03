#pragma once

#include <gltf.h>

class MaterialData
{
public:
	MaterialData()
		: has_data_(false)
	{
	}

	~MaterialData()
	{
	}

	void SetData(fx::gltf::Material const& material)
	{
		material_ = material;
		has_data_ = true;
	}

	fx::gltf::Material const& Data() const noexcept
	{
		return material_;
	}

	bool HasData() const
	{
		return has_data_ && !material_.pbrMetallicRoughness.empty();
	}

private:
	fx::gltf::Material material_;
	bool has_data_;
};
