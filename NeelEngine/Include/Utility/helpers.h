#pragma once

#include <locale>
#include <codecvt>
#include <cstdint>
#include <string>
#include <d3dcompiler.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For HRESULT
#include <comdef.h> // For _com_error class (used to decode HR result codes).

#include "shader_options.h"
#include "d3dx12.h"

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		_com_error err(hr);
		OutputDebugString(err.ErrorMessage());

		throw std::exception(err.ErrorMessage());
	}
}

inline void ThrowIfFalse(bool value)
{
	ThrowIfFailed(value ? S_OK : E_FAIL);
}

// Hashers for view descriptions.
namespace std
{
	// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
	template <typename T>
	inline void hash_combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	template <>
	struct hash<D3D12_SHADER_RESOURCE_VIEW_DESC>
	{
		std::size_t operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& srv_desc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, srv_desc.Format);
			hash_combine(seed, srv_desc.ViewDimension);
			hash_combine(seed, srv_desc.Shader4ComponentMapping);

			switch (srv_desc.ViewDimension)
			{
			case D3D12_SRV_DIMENSION_BUFFER:
				hash_combine(seed, srv_desc.Buffer.FirstElement);
				hash_combine(seed, srv_desc.Buffer.NumElements);
				hash_combine(seed, srv_desc.Buffer.StructureByteStride);
				hash_combine(seed, srv_desc.Buffer.Flags);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE1D:
				hash_combine(seed, srv_desc.Texture1D.MostDetailedMip);
				hash_combine(seed, srv_desc.Texture1D.MipLevels);
				hash_combine(seed, srv_desc.Texture1D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, srv_desc.Texture1DArray.MostDetailedMip);
				hash_combine(seed, srv_desc.Texture1DArray.MipLevels);
				hash_combine(seed, srv_desc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, srv_desc.Texture1DArray.ArraySize);
				hash_combine(seed, srv_desc.Texture1DArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2D:
				hash_combine(seed, srv_desc.Texture2D.MostDetailedMip);
				hash_combine(seed, srv_desc.Texture2D.MipLevels);
				hash_combine(seed, srv_desc.Texture2D.PlaneSlice);
				hash_combine(seed, srv_desc.Texture2D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, srv_desc.Texture2DArray.MostDetailedMip);
				hash_combine(seed, srv_desc.Texture2DArray.MipLevels);
				hash_combine(seed, srv_desc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, srv_desc.Texture2DArray.ArraySize);
				hash_combine(seed, srv_desc.Texture2DArray.PlaneSlice);
				hash_combine(seed, srv_desc.Texture2DArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMS:
				//                hash_combine(seed, srvDesc.Texture2DMS.UnusedField_NothingToDefine);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
				hash_combine(seed, srv_desc.Texture2DMSArray.FirstArraySlice);
				hash_combine(seed, srv_desc.Texture2DMSArray.ArraySize);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE3D:
				hash_combine(seed, srv_desc.Texture3D.MostDetailedMip);
				hash_combine(seed, srv_desc.Texture3D.MipLevels);
				hash_combine(seed, srv_desc.Texture3D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
				hash_combine(seed, srv_desc.TextureCube.MostDetailedMip);
				hash_combine(seed, srv_desc.TextureCube.MipLevels);
				hash_combine(seed, srv_desc.TextureCube.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				hash_combine(seed, srv_desc.TextureCubeArray.MostDetailedMip);
				hash_combine(seed, srv_desc.TextureCubeArray.MipLevels);
				hash_combine(seed, srv_desc.TextureCubeArray.First2DArrayFace);
				hash_combine(seed, srv_desc.TextureCubeArray.NumCubes);
				hash_combine(seed, srv_desc.TextureCubeArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
				hash_combine(seed, srv_desc.RaytracingAccelerationStructure.Location);
				break;
			default:
				break;	
			}

			return seed;
		}
	};

	template <>
	struct hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>
	{
		std::size_t operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uav_desc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, uav_desc.Format);
			hash_combine(seed, uav_desc.ViewDimension);

			switch (uav_desc.ViewDimension)
			{
			case D3D12_UAV_DIMENSION_BUFFER:
				hash_combine(seed, uav_desc.Buffer.FirstElement);
				hash_combine(seed, uav_desc.Buffer.NumElements);
				hash_combine(seed, uav_desc.Buffer.StructureByteStride);
				hash_combine(seed, uav_desc.Buffer.CounterOffsetInBytes);
				hash_combine(seed, uav_desc.Buffer.Flags);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE1D:
				hash_combine(seed, uav_desc.Texture1D.MipSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, uav_desc.Texture1DArray.MipSlice);
				hash_combine(seed, uav_desc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, uav_desc.Texture1DArray.ArraySize);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE2D:
				hash_combine(seed, uav_desc.Texture2D.MipSlice);
				hash_combine(seed, uav_desc.Texture2D.PlaneSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, uav_desc.Texture2DArray.MipSlice);
				hash_combine(seed, uav_desc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, uav_desc.Texture2DArray.ArraySize);
				hash_combine(seed, uav_desc.Texture2DArray.PlaneSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE3D:
				hash_combine(seed, uav_desc.Texture3D.MipSlice);
				hash_combine(seed, uav_desc.Texture3D.FirstWSlice);
				hash_combine(seed, uav_desc.Texture3D.WSize);
				break;
			default:
				break;
			}

			return seed;
		}
	};
}

namespace math
{
	constexpr float kPi = 3.1415926535897932384626433832795f;
	constexpr float k2Pi = 2.0f * kPi;
	// Convert radians to degrees.
	constexpr float Degrees(const float radians)
	{
		return radians * (180.0f / kPi);
	}

	// Convert degrees to radians.
	constexpr float Radians(const float degrees)
	{
		return degrees * (kPi / 180.0f);
	}

	template <typename T>
	inline T Deadzone(T val, T deadzone)
	{
		if (std::abs(val) < deadzone)
		{
			return T(0);
		}

		return val;
	}

	// Normalize a value in the range [min - max]
	template <typename T, typename U>
	inline T NormalizeRange(U x, U min, U max)
	{
		return T(x - min) / T(max - min);
	}

	// Shift and bias a value into another range.
	template <typename T, typename U>
	inline T ShiftBias(U x, U shift, U bias)
	{
		return T(x * bias) + T(shift);
	}

	/***************************************************************************
	* These functions were taken from the MiniEngine.
	* Source code available here:
	* https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Math/Common.h
	* Retrieved: January 13, 2016
	**************************************************************************/
	template <typename T>
	inline T AlignUpWithMask(T value, size_t mask)
	{
		return static_cast<T>((static_cast<size_t>(value) + mask) & ~mask);
	}

	template <typename T>
	inline T AlignDownWithMask(T value, size_t mask)
	{
		return static_cast<T>(static_cast<size_t>(value) & ~mask);
	}

	template <typename T>
	inline T AlignUp(T value, size_t alignment)
	{
		return AlignUpWithMask(value, alignment - 1);
	}

	template <typename T>
	inline T AlignDown(T value, size_t alignment)
	{
		return AlignDownWithMask(value, alignment - 1);
	}

	template <typename T>
	inline bool IsAligned(T value, size_t alignment)
	{
		return 0 == (static_cast<size_t>(value) & (alignment - 1));
	}

	template <typename T>
	inline T DivideByMultiple(T value, size_t alignment)
	{
		return static_cast<T>((value + alignment - 1) / alignment);
	}

	/***************************************************************************/

	/**
	* Round up to the next highest power of 2.
	* @source: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	* @retrieved: January 16, 2016
	*/
	inline uint32_t NextHighestPow2(uint32_t v)
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;

		return v;
	}

	/**
	* Round up to the next highest power of 2.
	* @source: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	* @retrieved: January 16, 2016
	*/
	inline uint64_t NextHighestPow2(uint64_t v)
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v |= v >> 32;
		v++;

		return v;
	}
}

#define STR1(x) #x
#define STR(x) STR1(x)
#define WSTR1(x) L##x
#define WSTR(x) WSTR1(x)
// ReSharper disable once CppInconsistentNaming
#define NAME_D3D12_OBJECT(x) x->SetName( WSTR(__FILE__ "(" STR(__LINE__) "): " L#x) )

inline bool StringEndsWith(const std::string& subject, const std::string& suffix)
{
	// Early out test:
	if (suffix.length() > subject.length())
	{
		return false;
	}

	// Resort to difficult to read C++ logic:
	return subject.compare(
		subject.length() - suffix.length(),
		suffix.length(),
		suffix
	) == 0;
}

// convert UTF-8 string to wstring
inline std::wstring utf8_to_utf16(const std::string& str)
{
	if (str.empty())
		return std::wstring();

	size_t chars_needed = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
	if (chars_needed == 0)
		throw std::runtime_error("Failed converting UTF-8 string to UTF-16");

	std::vector<wchar_t> buffer(chars_needed);
	int chars_converted = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &buffer[0], buffer.size());
	if (chars_converted == 0)
		throw std::runtime_error("Failed converting UTF-8 string to UTF-16");

	return std::wstring(&buffer[0], chars_converted);
}

// Clamp a value between a min and max range.
template <typename T>
inline constexpr const T& clamp(const T& val, const T& min = T(0), const T& max = T(1))
{
	return val < min ? min : val > max ? max : val;
}



inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(std::wstring const& filename, std::string const& entry_point, std::string const& target, D3D_SHADER_MACRO const* defines)
{
	UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
	compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	Microsoft::WRL::ComPtr<ID3DBlob> byte_code{};
	Microsoft::WRL::ComPtr<ID3DBlob> errors{};
	HRESULT result = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point.c_str(), target.c_str(), compile_flags, 0, &byte_code, &errors);

	if (FAILED(result))
	{
		if (errors != nullptr)
			OutputDebugStringA(static_cast<LPCSTR>(errors->GetBufferPointer()));
	}

	return byte_code;
}

inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShaderPerumutation(std::string const& entry_point, ShaderOptions options)
{
	// Keep rooted until compile completes since D3D_SHADER_MACRO is just a view over this data...
	std::vector<std::string> defines = GetShaderDefines(options | ShaderOptions::None);

	std::vector<D3D_SHADER_MACRO> shader_defines;
	for (auto const& define : defines)
	{
		shader_defines.emplace_back(D3D_SHADER_MACRO{ define.c_str(), "1" });
	}

	shader_defines.emplace_back(D3D_SHADER_MACRO{ nullptr, nullptr });

	Microsoft::WRL::ComPtr<ID3DBlob> permutated_pixel_shader = CompileShader(L"Shaders/HDR_PS.hlsl", entry_point, "ps_5_1", shader_defines.data());

	return permutated_pixel_shader;
}