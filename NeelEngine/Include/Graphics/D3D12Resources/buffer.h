#pragma once

#include "resource.h"

class Buffer : public Resource
{
public:
	explicit Buffer(const std::string& name = "");
	explicit Buffer(const D3D12_RESOURCE_DESC& res_desc,
	                size_t num_elements, size_t element_size,
	                const std::string& name = "");

	/**
	 * Create the views for the buffer resource.
	 * Used by the CommandList when setting the buffer contents.
	 */
	virtual void CreateViews(size_t num_elements, size_t element_size) = 0;
};
