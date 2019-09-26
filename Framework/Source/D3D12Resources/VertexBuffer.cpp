#include <FrameworkPCH.h>

#include <VertexBuffer.h>

VertexBuffer::VertexBuffer(const std::string& name)
	: Buffer(name)
	, m_NumVertices(0)
	, m_VertexStride(0)
	, m_VertexBufferViews({})
	, m_GPUOffset(0)
{}

VertexBuffer::~VertexBuffer()
{}

void VertexBuffer::CreateViews(size_t numElements, size_t elementSize)
{
	m_NumVertices	= numElements;
	m_VertexStride	= elementSize;

	m_VertexBufferViews.resize(1);

	m_VertexBufferViews[0].BufferLocation	= m_d3d12Resource->GetGPUVirtualAddress();
	m_VertexBufferViews[0].SizeInBytes		= static_cast<UINT>(m_NumVertices * m_VertexStride);
	m_VertexBufferViews[0].StrideInBytes	= static_cast<UINT>(m_VertexStride);
}

void VertexBuffer::CreateViews(std::vector<uint32_t> numElements, std::vector<uint32_t> elementSize)
{
	m_VertexBufferViews.resize(numElements.size());

	for (int i = 0; i < numElements.size(); i++)
	{
		uint32_t numVertices	= numElements[i];
		uint32_t elemSize		= elementSize[i];

		m_VertexBufferViews[i].BufferLocation	= m_d3d12Resource->GetGPUVirtualAddress() + m_GPUOffset;
		m_VertexBufferViews[i].SizeInBytes		= static_cast<UINT>(numVertices * elemSize);
		m_VertexBufferViews[i].StrideInBytes	= static_cast<UINT>(elemSize);

		m_GPUOffset = numVertices * elemSize;
	}

	m_NumVertices = numElements[0];
	m_VertexStride = 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	throw std::exception("VertexBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	throw std::exception("VertexBuffer::GetUnorderedAccessView should not be called.");
}