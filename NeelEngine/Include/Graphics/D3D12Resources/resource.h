#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <string>

class Resource
{
public:
	explicit Resource(const std::string& name = "");
	explicit Resource(const D3D12_RESOURCE_DESC& resource_desc,
	                  const D3D12_CLEAR_VALUE* clear_value = nullptr,
	                  const std::string& name = "");
	explicit Resource(Microsoft::WRL::ComPtr<ID3D12Resource> resource, const std::string& name = "");

	Resource(const Resource& copy);
	Resource(Resource&& copy);

	Resource& operator=(const Resource& other);
	Resource& operator=(Resource&& other) noexcept;

	virtual ~Resource();

	/**
	 * Check to see if the underlying resource is valid.
	 */
	bool IsValid() const
	{
		return (d3d12_resource_ != nullptr);
	}

	// Get access to the underlying D3D12 resource
	Microsoft::WRL::ComPtr<ID3D12Resource> GetD3D12Resource() const
	{
		return d3d12_resource_;
	}

	D3D12_RESOURCE_DESC GetD3D12ResourceDesc() const
	{
		D3D12_RESOURCE_DESC res_desc = {};
		if (d3d12_resource_)
		{
			res_desc = d3d12_resource_->GetDesc();
		}

		return res_desc;
	}

	// Replace the D3D12 resource
	// Should only be called by the CommandList.
	virtual void SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource,
	                              const D3D12_CLEAR_VALUE* clear_value = nullptr);

	/**
	 * Get the SRV for a resource.
	 *
	 * @param srv_desc The description of the SRV to return. The default is nullptr
	 * which returns the default SRV for the resource (the SRV that is created when no
	 * description is provided.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr) const = 0;

	/**
	 * Get the UAV for a (sub)resource.
	 *
	 * @param uav_desc The description of the UAV to return.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr) const = 0;

	/**
	 * Set the name of the resource. Useful for debugging purposes.
	 * The name of the resource will persist if the underlying D3D12 resource is
	 * replaced with SetD3D12Resource.
	 */
	void SetName(const std::string& name);

	/**
	 * Release the underlying resource.
	 * This is useful for swap chain resizing.
	 */
	virtual void Reset();

	/**
	 * Check if the resource format supports a specific feature.
	 */
	bool CheckFormatSupport(D3D12_FORMAT_SUPPORT1 format_support) const;
	bool CheckFormatSupport(D3D12_FORMAT_SUPPORT2 format_support) const;


protected:
	// The underlying D3D12 resource.
	Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource_;
	D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support_{};
	std::unique_ptr<D3D12_CLEAR_VALUE> d3d12_clear_value_;
	std::string resource_name_;

private:
	// Check the format support and populate the format_support_ structure.
	void CheckFeatureSupport();
};
