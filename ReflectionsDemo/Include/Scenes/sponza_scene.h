#pragma once

#include "gltf_scene.h"
#include "root_signature.h"
#include "render_target.h"

class SponzaScene : public Scene
{
public:
	SponzaScene();
	virtual ~SponzaScene();

	void Load(const std::string& filename) override;

	void Update(UpdateEventArgs& e) override;

	void PrepareRender(CommandList& command_list) override;

	void Render(CommandList& command_list) override;

	RenderTarget& GetRenderTarget() override;

protected:

private:
	RootSignature sponza_root_signature_;
	RenderTarget  hdr_render_target_;

	// Map containing all different pipeline states for each mesh with different shader options (permutations).
	std::unordered_map<ShaderOptions, Microsoft::WRL::ComPtr<ID3D12PipelineState>> sponza_pipeline_state_map_;

	D3D12_RECT scissor_rect_;

	enum RootParameters
	{
		MeshConstantBuffer = 0,	// ConstantBuffer<Mat> MatCB							: register(b0);  
		LightPropertiesCb,		// ConstantBuffer<LightProperties> LightPropertiesCB	: register( b1 );
		Materials,				// StructuredBuffer<MaterialData> Materials				: register( t0 );
		PointLights,			// StructuredBuffer<PointLight> PointLights				: register( t1 );
		SpotLights,				// StructuredBuffer<SpotLight> SpotLights				: register( t2 );
		DirectionalLights,		// StructuredBuffer<DirectionalLight> DirectionalLights : register (t3 );
		Textures,				// Texture2D DiffuseTexture								: register( t4 );
		NumRootParameters
	};
};