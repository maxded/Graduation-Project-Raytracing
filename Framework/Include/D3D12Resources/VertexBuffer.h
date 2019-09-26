#pragma once

#include <Buffer.h>

class VertexBuffer : public Buffer
{
public:
	VertexBuffer(const std::string& name = "Vertex Buffer");
	virtual ~VertexBuffer();

	virtual void CreateViews(size_t numElements, size_t elementSize) override;

	void CreateViews(std::vector<uint32_t> numElements, std::vector<uint32_t> elementSize);

	/**
	 * Get the vertex buffer view for binding to the Input Assembler stage.
	 */
	std::vector<D3D12_VERTEX_BUFFER_VIEW> GetVertexBufferViews() const
	{
		return m_VertexBufferViews;
	}

	uint32_t GetNumVertices() const
	{
		return m_NumVertices;
	}


	/**
	* Get the SRV for a resource.
	*/
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;

	/**
	* Get the UAV for a (sub)resource.
	*/
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;
protected:
private:
	uint32_t m_NumVertices;
	uint32_t m_VertexStride;

	uint32_t m_GPUOffset;

	std::vector<D3D12_VERTEX_BUFFER_VIEW> m_VertexBufferViews;
};
