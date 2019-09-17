#include <FrameworkPCH.h>

#include "ConstantBuffer.h"
#include "Application.h"
#include "d3dx12.h"

ConstantBuffer::ConstantBuffer(const std::wstring& name)
	: Buffer(name)
	, m_SizeInBytes(0)
{

}

ConstantBuffer::~ConstantBuffer()
{}

void ConstantBuffer::CreateViews(size_t numElements, size_t elementSize)
{
	m_SizeInBytes = numElements * elementSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC descriptor;
	descriptor.BufferLocation	= m_d3d12Resource->GetGPUVirtualAddress();
	descriptor.SizeInBytes		= static_cast<UINT>(Math::AlignUp(m_SizeInBytes, 16));

	auto device = Application::Get().GetDevice();

	
}


D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	throw std::exception("ConstantBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	throw std::exception("ConstantBuffer::GetUnorderedAccessView should not be called.");
}