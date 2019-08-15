#include <HybridRayTracingDemo.h>
#include <Application.h>
#include <Window.h>

#include <Wrappers/CommandQueue.h>
#include <Utility/Helpers.h>

#include <d3dx12.h>

HybridRayTracingDemo::HybridRayTracingDemo(const std::wstring& name, int width, int height, bool vSync)
	: game(name, width, height, vSync)
	, m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
	, m_ContentLoaded(false)
{

}

bool HybridRayTracingDemo::LoadContent()
{
	m_ContentLoaded = true;

	return true;
}

void HybridRayTracingDemo::UnloadContent()
{
	m_ContentLoaded = false;
}

void HybridRayTracingDemo::OnUpdate(UpdateEventArgs& e)
{
	game::OnUpdate(e);
}

void HybridRayTracingDemo::OnRender(RenderEventArgs& e)
{
	game::OnRender(e);

	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList  = commandQueue->GetCommandList();

	uint8_t currentBackBufferIndex	= m_pWindow->GetCurrentBackBufferIndex();
	auto backBuffer					= m_pWindow->GetCurrentBackBuffer();
	auto rtv						= m_pWindow->GetCurrentRenderTargetView();

	// Clear the render target.
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		commandList->ResourceBarrier(1, &barrier);

		FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	}

	// Present
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1, &barrier);

		m_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

		currentBackBufferIndex = m_pWindow->Present();

		commandQueue->WaitForFenceValue(m_FenceValues[currentBackBufferIndex]);
	}

}

void HybridRayTracingDemo::OnKeyPressed(KeyEventArgs& e)
{

}

void HybridRayTracingDemo::OnMouseWheel(MouseWheelEventArgs& e)
{

}

void HybridRayTracingDemo::OnResize(ResizeEventArgs& e)
{

}
