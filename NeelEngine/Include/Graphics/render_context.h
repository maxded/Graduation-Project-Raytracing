#pragma once

#include "shader_options.h"
#include "commandlist.h"

#include <unordered_map>

struct  RenderContext
{
	CommandList& CommandList;
	ShaderOptions CurrentOptions;
	ShaderOptions OverrideOptions;
	std::unordered_map<ShaderOptions, Microsoft::WRL::ComPtr<ID3D12PipelineState>>& PipelineStateMap;
};
