#include "neel_engine_pch.h"

#include "resource.h"

#include "neel_engine.h"
#include "resource_state_tracker.h"

Resource::Resource(const std::string& name)
	: format_support_({})
	  , resource_name_(name)
{
}

Resource::Resource(const D3D12_RESOURCE_DESC& resource_desc, const D3D12_CLEAR_VALUE* clear_value,
                   const std::string& name)
{
	if (clear_value)
	{
		d3d12_clear_value_ = std::make_unique<D3D12_CLEAR_VALUE>(*clear_value);
	}

	auto device = NeelEngine::Get().GetDevice();

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		D3D12_RESOURCE_STATE_COMMON,
		d3d12_clear_value_.get(),
		IID_PPV_ARGS(&d3d12_resource_)
	));

	ResourceStateTracker::AddGlobalResourceState(d3d12_resource_.Get(), D3D12_RESOURCE_STATE_COMMON);

	CheckFeatureSupport();
	SetName(name);
}

Resource::Resource(Microsoft::WRL::ComPtr<ID3D12Resource> resource, const std::string& name)
	: d3d12_resource_(resource)
	  , format_support_({})
{
	CheckFeatureSupport();
	SetName(name);
}

Resource::Resource(const Resource& copy)
	: d3d12_resource_(copy.d3d12_resource_)
	  , format_support_(copy.format_support_)
	  , resource_name_(copy.resource_name_)
{
	if (d3d12_clear_value_)
		std::make_unique<D3D12_CLEAR_VALUE>(*copy.d3d12_clear_value_);
}

Resource::Resource(Resource&& copy)
	: d3d12_resource_(std::move(copy.d3d12_resource_))
	  , format_support_(copy.format_support_)
	  , d3d12_clear_value_(std::move(copy.d3d12_clear_value_))
	  , resource_name_(std::move(copy.resource_name_))
{
}

Resource& Resource::operator=(const Resource& other)
{
	if (this != &other)
	{
		d3d12_resource_ = other.d3d12_resource_;
		format_support_ = other.format_support_;
		resource_name_ = other.resource_name_;
		if (other.d3d12_clear_value_)
		{
			d3d12_clear_value_ = std::make_unique<D3D12_CLEAR_VALUE>(*other.d3d12_clear_value_);
		}
	}

	return *this;
}

Resource& Resource::operator=(Resource&& other) noexcept
{
	if (this != &other)
	{
		d3d12_resource_ = std::move(other.d3d12_resource_);
		format_support_ = other.format_support_;
		resource_name_ = std::move(other.resource_name_);
		d3d12_clear_value_ = std::move(other.d3d12_clear_value_);

		other.Reset();
	}

	return *this;
}


Resource::~Resource()
{
}

void Resource::SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource,
                                const D3D12_CLEAR_VALUE* clear_value)
{
	d3d12_resource_ = d3d12_resource;
	if (d3d12_clear_value_)
	{
		d3d12_clear_value_ = std::make_unique<D3D12_CLEAR_VALUE>(*clear_value);
	}
	else
	{
		d3d12_clear_value_.reset();
	}
	CheckFeatureSupport();
	SetName(resource_name_);
}

void Resource::SetName(const std::string& name)
{
	resource_name_ = name;
	if (d3d12_resource_ && !resource_name_.empty())
	{
		d3d12_resource_->SetName(utf8_to_utf16(resource_name_).c_str());
	}
}

void Resource::Reset()
{
	d3d12_resource_.Reset();
	format_support_ = {};
	d3d12_clear_value_.reset();
	resource_name_.clear();
}

bool Resource::CheckFormatSupport(D3D12_FORMAT_SUPPORT1 format_support) const
{
	return (format_support_.Support1 & format_support) != 0;
}

bool Resource::CheckFormatSupport(D3D12_FORMAT_SUPPORT2 format_support) const
{
	return (format_support_.Support2 & format_support) != 0;
}

void Resource::CheckFeatureSupport()
{
	if (d3d12_resource_)
	{
		auto desc = d3d12_resource_->GetDesc();
		auto device = NeelEngine::Get().GetDevice();

		format_support_.Format = desc.Format;
		ThrowIfFailed(device->CheckFeatureSupport(
			D3D12_FEATURE_FORMAT_SUPPORT,
			&format_support_,
			sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));
	}
	else
	{
		format_support_ = {};
	}
}
