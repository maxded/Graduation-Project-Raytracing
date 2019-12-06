#pragma once

#include <DirectXMath.h>

struct MaterialFake
{
	MaterialFake(
		DirectX::XMFLOAT4 emissive = { 0.0f, 0.0f, 0.0f, 1.0f },
		DirectX::XMFLOAT4 ambient = { 0.1f, 0.1f, 0.1f, 1.0f },
		DirectX::XMFLOAT4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f },
		DirectX::XMFLOAT4 specular = { 1.0f, 1.0f, 1.0f, 1.0f },
		float specular_power = 128.0f
	)
		: Emissive(emissive)
		, Ambient(ambient)
		, Diffuse(diffuse)
		, Specular(specular)
		, SpecularPower(specular_power)
		, Padding{0, 0, 0}
	{}

	DirectX::XMFLOAT4 Emissive;
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Ambient;
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Diffuse;
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Specular;
	//----------------------------------- (16 byte boundary)
	float             SpecularPower;
	uint32_t          Padding[3];
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 5 = 80 bytes

	// Define some interesting materials.
	static const MaterialFake Red;
	static const MaterialFake Green;
	static const MaterialFake Blue;
	static const MaterialFake Cyan;
	static const MaterialFake Magenta;
	static const MaterialFake Yellow;
	static const MaterialFake White;
	static const MaterialFake Black;
	static const MaterialFake Emerald;
	static const MaterialFake Jade;
	static const MaterialFake Obsidian;
	static const MaterialFake Pearl;
	static const MaterialFake Ruby;
	static const MaterialFake Turquoise;
	static const MaterialFake Brass;
	static const MaterialFake Bronze;
	static const MaterialFake Chrome;
	static const MaterialFake Copper;
	static const MaterialFake Gold;
	static const MaterialFake Silver;
	static const MaterialFake BlackPlastic;
	static const MaterialFake CyanPlastic;
	static const MaterialFake GreenPlastic;
	static const MaterialFake RedPlastic;
	static const MaterialFake WhitePlastic;
	static const MaterialFake YellowPlastic;
	static const MaterialFake BlackRubber;
	static const MaterialFake CyanRubber;
	static const MaterialFake GreenRubber;
	static const MaterialFake RedRubber;
	static const MaterialFake WhiteRubber;
	static const MaterialFake YellowRubber;
};