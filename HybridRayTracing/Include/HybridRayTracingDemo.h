#pragma once

#include <Camera.h>
#include <Game.h>
#include <IndexBuffer.h>
#include <Light.h>
#include <Window.h>
#include <StaticMesh.h>
#include <RenderTarget.h>
#include <RootSignature.h>
#include <Texture.h>
#include <VertexBuffer.h>

#include <DirectXMath.h>

class StaticMesh;

class HybridRayTracingDemo : public Game
{
public:
	using game = Game;

	HybridRayTracingDemo(const std::wstring& name, int width, int height, bool vSync = false);
	virtual ~HybridRayTracingDemo();

	/**
	 *  Load content required for the demo.
	 */
	virtual bool LoadContent() override;

	/**
	 *  Unload demo specific content that was loaded in LoadContent.
	 */
	virtual void UnloadContent() override;
protected:
	/**
	*  Update the game logic.
	*/
	virtual void OnUpdate(UpdateEventArgs& e) override;

	/**
	 *  Render stuff.
	 */
	virtual void OnRender(RenderEventArgs& e) override;

	/**
	 * Invoked by the registered window when a key is pressed
	 * while the window has focus.
	 */
	virtual void OnKeyPressed(KeyEventArgs& e) override;

	virtual void OnKeyReleased(KeyEventArgs& e) override;

	virtual void OnMouseMoved(MouseMotionEventArgs& e) override;

	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;

	virtual void OnResize(ResizeEventArgs& e) override;

	void RescaleHDRRenderTarget(float scale);


private:
	// Some geometry to render.
	std::unique_ptr<StaticMesh> m_CubeMesh;
	std::unique_ptr<StaticMesh> m_SphereMesh;
	std::unique_ptr<StaticMesh> m_ConeMesh;
	std::unique_ptr<StaticMesh> m_TorusMesh;
	std::unique_ptr<StaticMesh> m_PlaneMesh;

	// HDR Render target
	RenderTarget m_HDRRenderTarget;

	// Root signatures
	RootSignature m_HDRRootSignature;
	RootSignature m_SDRRootSignature;

	// Pipeline state object.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_HDRPipelineState;
	// HDR -> SDR tone mapping PSO.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SDRPipelineState;

	D3D12_RECT m_ScissorRect;

	Camera m_Camera;
	struct alignas(16) CameraData
	{
		DirectX::XMVECTOR m_InitialCamPos;
		DirectX::XMVECTOR m_InitialCamRot;
		float m_InitialFov;
	};
	CameraData* m_pAlignedCameraData;

	// Camera controller
	float m_Forward;
	float m_Backward;
	float m_Left;
	float m_Right;
	float m_Up;
	float m_Down;

	float m_Pitch;
	float m_Yaw;

	// Rotate the lights in a circle.
	bool m_AnimateLights;
	// Set to true if the Shift key is pressed.
	bool m_Shift;

	int m_Width;
	int m_Height;

	// Scale the HDR render target to a fraction of the window size.
	float m_RenderScale;

	// Define some lights.
	std::vector<PointLight> m_PointLights;
	std::vector<SpotLight> m_SpotLights;
};