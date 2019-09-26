#include <FrameworkPCH.h>

#include <Buffer.h>

Buffer::Buffer(const std::string& name)
	: Resource(name)
{}

Buffer::Buffer(const D3D12_RESOURCE_DESC& resDesc,
	size_t numElements, size_t elementSize,
	const std::string& name)
	: Resource(resDesc, nullptr, name)
{}
