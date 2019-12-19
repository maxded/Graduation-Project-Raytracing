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

	void OnGui();

protected:

private:
	RootSignature sponza_root_signature_;
	RenderTarget  hdr_render_target_;

	// Map containing all different pipeline states for each mesh with different shader options (permutations).
	std::unordered_map<ShaderOptions, Microsoft::WRL::ComPtr<ID3D12PipelineState>> sponza_pipeline_state_map_;

	D3D12_RECT scissor_rect_;

	enum RootParameters
	{
		Materials = 0,			// ConstantBuffer<MaterialData> Materials				: register( b0 );
		MeshConstantBuffer,		// ConstantBuffer<Mat> MatCB							: register( b1 );		
		LightPropertiesCb,		// ConstantBuffer<LightProperties> LightPropertiesCB	: register( b2 );
		PointLights,			// StructuredBuffer<PointLight> PointLights				: register( t0 );
		SpotLights,				// StructuredBuffer<SpotLight> SpotLights				: register( t1 );
		DirectionalLights,		// StructuredBuffer<DirectionalLight> DirectionalLights : register (t2 );
		Textures,				// Texture2D textures[5]								: register( t3 );
		NumRootParameters
	};
};