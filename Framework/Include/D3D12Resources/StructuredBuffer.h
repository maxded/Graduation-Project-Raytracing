#pragma once

#include "Buffer.h"

#include "ByteAddressBuffer.h"

class StructuredBuffer : public Buffer
{
public:
	StructuredBuffer(const std::string& name = "Structured Buffer");
	StructuredBuffer(const D3D12_RESOURCE_DESC& resDesc,
		size_t numElements, size_t elementSize,
		const std::string& name = "Structured Buffer");

	/**
	* Get the number of elements contained in this buffer.
	*/
	virtual size_t GetNumElements() const
	{
		return m_NumElements;
	}

	/**
	* Get the size in bytes of each element in this buffer.
	*/
	virtual size_t GetElementSize() const
	{
		return m_ElementSize;
	}

	/**
	 * Create the views for the buffer resource.
	 * Used by the CommandList when setting the buffer contents.
	 */
	virtual void CreateViews(size_t numElements, size_t elementSize) override;

	/**
	 * Get the SRV for a resource.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const
	{
		return m_SRV.GetDescriptorHandle();
	}

	/**
	 * Get the UAV for a (sub)resource.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override
	{
		// Buffers don't have subresources.
		return m_UAV.GetDescriptorHandle();
	}

	const ByteAddressBuffer& GetCounterBuffer() const
	{
		return m_CounterBuffer;
	}

private:
	size_t m_NumElements;
	size_t m_ElementSize;

	DescriptorAllocation m_SRV;
	DescriptorAllocation m_UAV;

	// A buffer to store the internal counter for the structured buffer.
	ByteAddressBuffer m_CounterBuffer;
};