#include "neel_engine_pch.h"

#include "buffer.h"

Buffer::Buffer(const std::string& name)
	: Resource(name)
{
}

Buffer::Buffer(const D3D12_RESOURCE_DESC& res_desc,
               size_t num_elements, size_t element_size,
               const std::string& name)
	: Resource(res_desc, nullptr, name)
{
}


