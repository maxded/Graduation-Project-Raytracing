#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

enum class ShaderOptions : uint64_t
{
	// clang-format off
	None = 0,
	USE_AUTO_COLOR				= (1u << 0u),
	USE_MANUAL_SRGB				= (1u << 1u),
	HAS_BASECOLORMAP			= (1u << 2u),
	HAS_TANGENTS				= (1u << 3u),
	HAS_NORMALMAP				= (1u << 4u),
	HAS_METALROUGHNESSMAP		= (1u << 5u),
	HAS_OCCLUSIONMAP			= (1u << 6u),
	HAS_OCCLUSIONMAP_COMBINED	= (1u << 7u),
	HAS_EMISSIVEMAP				= (1u << 8u)
};

inline constexpr ShaderOptions operator|(ShaderOptions a, ShaderOptions b) noexcept
{
	return static_cast<ShaderOptions>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

inline ShaderOptions& operator|=(ShaderOptions& a, ShaderOptions b) noexcept
{
	return reinterpret_cast<ShaderOptions&>(reinterpret_cast<uint64_t&>(a) |= static_cast<uint64_t>(b));
}

inline constexpr ShaderOptions operator&(ShaderOptions a, ShaderOptions b) noexcept
{
	return static_cast<ShaderOptions>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}

inline ShaderOptions& operator&=(ShaderOptions& a, ShaderOptions b) noexcept
{
	return reinterpret_cast<ShaderOptions&>(reinterpret_cast<uint64_t&>(a) &= static_cast<uint64_t>(b));
}

inline constexpr ShaderOptions operator~(ShaderOptions a) noexcept
{
	return static_cast<ShaderOptions>(~static_cast<uint64_t>(a));
}

inline constexpr ShaderOptions operator^(ShaderOptions a, ShaderOptions b) noexcept
{
	return static_cast<ShaderOptions>(static_cast<uint64_t>(a) ^ static_cast<uint64_t>(b));
}

inline ShaderOptions& operator^=(ShaderOptions& a, ShaderOptions b) noexcept
{
	return reinterpret_cast<ShaderOptions&>(reinterpret_cast<uint64_t&>(a) ^= static_cast<uint64_t>(b));
}

inline constexpr bool IsSet(ShaderOptions options, ShaderOptions flag) noexcept
{
	return (options & flag) == flag;
}

inline std::vector<std::string> GetShaderDefines(ShaderOptions options)
{
	std::vector<std::string> defines;

	if (IsSet(options, ShaderOptions::USE_AUTO_COLOR))
	{
		defines.emplace_back("USE_AUTO_COLOR");
	}
	if (IsSet(options, ShaderOptions::USE_MANUAL_SRGB))
	{
		defines.emplace_back("USE_MANUAL_SRGB");
	}
	if (IsSet(options, ShaderOptions::HAS_BASECOLORMAP))
	{
		defines.emplace_back("HAS_BASECOLORMAP");
	}
	if (IsSet(options, ShaderOptions::HAS_TANGENTS))
	{
		defines.emplace_back("HAS_TANGENTS");
	}
	if (IsSet(options, ShaderOptions::HAS_NORMALMAP))
	{
		defines.emplace_back("HAS_NORMALMAP");
	}
	if (IsSet(options, ShaderOptions::HAS_METALROUGHNESSMAP))
	{
		defines.emplace_back("HAS_METALROUGHNESSMAP");
	}
	if (IsSet(options, ShaderOptions::HAS_OCCLUSIONMAP))
	{
		defines.emplace_back("HAS_OCCLUSIONMAP");
	}
	if (IsSet(options, ShaderOptions::HAS_OCCLUSIONMAP_COMBINED))
	{
		if (!IsSet(options, ShaderOptions::HAS_METALROUGHNESSMAP))
		{
			throw std::runtime_error("Invalid ShaderOptions. HAS_OCCLUSIONMAP_COMBINED requires HAS_METALROUGHNESSMAP");
		}

		defines.emplace_back("HAS_OCCLUSIONMAP_COMBINED");
	}
	if (IsSet(options, ShaderOptions::HAS_EMISSIVEMAP))
	{
		defines.emplace_back("HAS_EMISSIVEMAP");
	}

	return defines;
}
