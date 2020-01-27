#pragma once

#include "resource.h"
#include "descriptor_allocation.h"
#include "texture_usage.h"

#include <mutex>
#include <unordered_map>

class Texture : public Resource
{
public:
	explicit Texture(TextureUsage texture_usage = TextureUsage::Albedo,
	                 const std::string& name = "Texture");
	explicit Texture(const D3D12_RESOURCE_DESC& resource_desc,
	                 const D3D12_CLEAR_VALUE* clear_value = nullptr,
	                 TextureUsage texture_usage = TextureUsage::Albedo,
	                 const std::string& name = "Texture");
	explicit Texture(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	                 TextureUsage texture_usage = TextureUsage::Albedo,
	                 const std::string& name = "Texture");

	Texture(const Texture& copy);
	Texture(Texture&& copy);

	Texture& operator=(const Texture& other);
	Texture& operator=(Texture&& other);

	virtual ~Texture();

	TextureUsage GetTextureUsage() const
	{
		return texture_usage_;
	}

	void SetTextureUsage(TextureUsage texture_usage)
	{
		texture_usage_ = texture_usage;
	}

	D3D12_RENDER_PASS_BEGINNING_ACCESS GetBeginningAccess() const;
	/**
	* Set the beginning access state of the texture for the render pass. (Only use when texture usage == render target).
	*/	
	void SetBeginningAccess(D3D12_RENDER_PASS_BEGINNING_ACCESS beginning_access);


	D3D12_RENDER_PASS_ENDING_ACCESS GetEndingAccess() const;
	/**
	* Set the ending access state of the texture for the render pass. (Only use when texture usage == render target).
	*/
	void SetEndingAccess(D3D12_RENDER_PASS_ENDING_ACCESS ending_access);

	/**
	 * Resize the texture.
	 */
	void Resize(uint32_t width, uint32_t height, uint32_t depth_or_array_size = 1);

	/**
	 * Create SRV and UAVs for the resource.
	 */
	virtual void CreateViews();

	/**
	* Get the SRV for a resource.
	*/
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr) const override;

	/**
	* Get the UAV for a (sub)resource.
	*/
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr) const override;

	/**
	 * Get the RTV for the texture.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const;

	/**
	 * Get the DSV for the texture.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

	bool CheckSRVSupport() const
	{
		return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
	}

	bool CheckRTVSupport() const
	{
		return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
	}

	bool CheckUAVSupport() const
	{
		return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
			CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
			CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
	}

	bool CheckDSVSupport() const
	{
		return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
	}

	static bool IsUAVCompatibleFormat(DXGI_FORMAT format);
	static bool IsSRGBFormat(DXGI_FORMAT format);
	static bool IsBGRFormat(DXGI_FORMAT format);
	static bool IsDepthFormat(DXGI_FORMAT format);

	// Return a typeless format from the given format.
	static DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
	// Return an sRGB format in the same format family.
	static DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT format);
	static DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT format);

	void SetFilename(std::string filename) { filename_ = filename; }
	const std::string& Filename() const { return filename_; } 

protected:

private:
	DescriptorAllocation CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const;
	DescriptorAllocation CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const;

	mutable std::unordered_map<size_t, DescriptorAllocation> shader_resource_views_;
	mutable std::unordered_map<size_t, DescriptorAllocation> unordered_access_views_;

	mutable std::mutex shader_resource_views_mutex_;
	mutable std::mutex unordered_access_views_mutex_;

	DescriptorAllocation render_target_view_;
	DescriptorAllocation depth_stencil_view_;

	TextureUsage texture_usage_;

	D3D12_RENDER_PASS_BEGINNING_ACCESS	render_pass_beginning_access_;
	D3D12_RENDER_PASS_ENDING_ACCESS		render_pass_ending_access_;

	std::string filename_;
};
