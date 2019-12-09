#pragma once

#include "gltf_scene.h"
#include "root_signature.h"
#include "render_target.h"

class DefaultScene : public Scene
{
public:
	DefaultScene();
	virtual ~DefaultScene();

	void Load(const std::string& filename) override;

	void Update(UpdateEventArgs& e) override;

	void PrepareRender(CommandList& command_list) override;

	void Render(CommandList& command_list) override;

	RenderTarget& GetRenderTarget() override;
protected:

private:
	RootSignature	sponza_root_signature_;
	RenderTarget	hdr_render_target_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> default_pipeline_state_;

	D3D12_RECT scissor_rect_;

	enum RootParameters
	{
		kMeshConstantBuffer,		// ConstantBuffer<Mat> MatCB							: register(b0);
		kMeshMaterialBuffer,		// ConstantBuffer<MaterialFake> MaterialCB				: register( b0, space1 );   
		kLightPropertiesCb,			// ConstantBuffer<LightProperties> LightPropertiesCB	: register( b1 );
		kPointLights,				// StructuredBuffer<PointLight> PointLights				: register( t0 );
		kSpotLights,				// StructuredBuffer<SpotLight> SpotLights				: register( t1 );
		kTextures,					// Texture2D DiffuseTexture								: register( t2 );
		kNumRootParameters
	};

};