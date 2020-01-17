#include "neel_engine_pch.h"

#include "window.h"
#include "neel_engine.h"
#include "commandqueue.h"
#include "commandlist.h"
#include "game.h"
#include "resource_state_tracker.h"

Window::Window(HWND h_wnd, const std::wstring& window_name, uint16_t client_width, uint16_t client_height, bool v_sync)
	: h_wnd_(h_wnd)
	  , window_name_(window_name)
	  , client_width_(client_width)
	  , client_height_(client_height)
	  , v_sync_(v_sync)
	  , fullscreen_(false)
	  , previous_mouse_x_(0)
	  , previous_mouse_y_(0)
	  , fence_values_{0}
	  , frame_values_{0}
{
	dpi_scaling_ = GetDpiForWindow(h_wnd) / 96.0f;
	
	NeelEngine& app = NeelEngine::Get();

	is_tearing_supported_ = app.IsTearingSupported();

	for (int i = 0; i < buffer_count_; ++i)
	{
		back_buffer_textures_[i].SetName("Backbuffer[" + std::to_string(i) + "]");
	}

	dxgi_swap_chain_ = CreateSwapChain();
	UpdateRenderTargetViews();
}

Window::~Window()
{
	assert(!h_wnd_ && "Use Application::DestroyWindow before destruction.");
}

HWND Window::GetWindowHandle() const
{
	return h_wnd_;
}

float Window::GetDPIScaling() const
{
	return dpi_scaling_;
}

void Window::Initialize()
{
	gui_.Initialize(shared_from_this());
}

const std::wstring& Window::GetWindowName() const
{
	return window_name_;
}

void Window::Show() const
{
	::ShowWindow(h_wnd_, SW_SHOW);
}

/**
* Hide the window.
*/
void Window::Hide() const
{
	::ShowWindow(h_wnd_, SW_HIDE);
}

void Window::Destroy()
{
	gui_.Destroy();
	
	if (auto p_game = p_game_.lock())
	{
		// Notify the registered game that the window is being destroyed.
		p_game->OnWindowDestroy();
	}

	if (h_wnd_)
	{
		DestroyWindow(h_wnd_);
		h_wnd_ = nullptr;
	}
}

int Window::GetClientWidth() const
{
	return client_width_;
}

int Window::GetClientHeight() const
{
	return client_height_;
}

bool Window::IsVSync() const
{
	return v_sync_;
}

void Window::SetVSync(bool v_sync)
{
	v_sync_ = v_sync;
}

void Window::ToggleVSync()
{
	SetVSync(!v_sync_);
}

bool Window::IsFullScreen() const
{
	return fullscreen_;
}

// Set the fullscreen State of the window.
void Window::SetFullscreen(bool fullscreen)
{
	if (fullscreen_ != fullscreen)
	{
		fullscreen_ = fullscreen;

		if (fullscreen_) // Switching to fullscreen.
		{
			// Store the current window dimensions so they can be restored 
			// when switching out of fullscreen State.
			::GetWindowRect(h_wnd_, &window_rect_);

			// Set the window style to a borderless window so the client area fills
			// the entire screen.
			UINT window_style = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX |
				WS_MAXIMIZEBOX);

			::SetWindowLongW(h_wnd_, GWL_STYLE, window_style);

			// Query the name of the nearest display device for the window.
			// This is required to set the fullscreen dimensions of the window
			// when using a multi-monitor setup.
			HMONITOR h_monitor = ::MonitorFromWindow(h_wnd_, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitor_info = {};
			monitor_info.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(h_monitor, &monitor_info);

			::SetWindowPos(h_wnd_, HWND_TOP,
			               monitor_info.rcMonitor.left,
			               monitor_info.rcMonitor.top,
			               monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
			               monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
			               SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(h_wnd_, SW_MAXIMIZE);
		}
		else
		{
			// Restore all the window decorators.
			::SetWindowLong(h_wnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			::SetWindowPos(h_wnd_, HWND_NOTOPMOST,
			               window_rect_.left,
			               window_rect_.top,
			               window_rect_.right - window_rect_.left,
			               window_rect_.bottom - window_rect_.top,
			               SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(h_wnd_, SW_NORMAL);
		}
	}
}

void Window::ToggleFullscreen()
{
	SetFullscreen(!fullscreen_);
}


void Window::RegisterCallbacks(std::shared_ptr<Game> p_game)
{
	p_game_ = p_game;

	return;
}

void Window::OnUpdate(UpdateEventArgs& e)
{
	// Wait for the swapchain to finish presenting
	::WaitForSingleObjectEx(swap_chain_event_, 100, TRUE);

	//gui_.NewFrame();
	
	update_clock_.Tick();

	if (auto p_game = p_game_.lock())
	{
		UpdateEventArgs update_event_args(update_clock_.GetDeltaSeconds(), update_clock_.GetTotalSeconds(),
		                                  e.FrameNumber);
		p_game->OnUpdate(update_event_args);
	}
}

void Window::OnRender(RenderEventArgs& e)
{
	render_clock_.Tick();

	if (auto p_game = p_game_.lock())
	{
		RenderEventArgs render_event_args(render_clock_.GetDeltaSeconds(), render_clock_.GetTotalSeconds(),
		                                  e.FrameNumber);
		p_game->OnRender(render_event_args);
	}
}

void Window::OnKeyPressed(KeyEventArgs& e)
{
	if (auto p_game = p_game_.lock())
	{
		p_game->OnKeyPressed(e);
	}
}

void Window::OnKeyReleased(KeyEventArgs& e)
{
	if (auto p_game = p_game_.lock())
	{
		p_game->OnKeyReleased(e);
	}
}

// The mouse was moved
void Window::OnMouseMoved(MouseMotionEventArgs& e)
{
	e.RelX = e.X - previous_mouse_x_;
	e.RelY = e.Y - previous_mouse_y_;

	previous_mouse_x_ = e.X;
	previous_mouse_y_ = e.Y;

	if (auto p_game = p_game_.lock())
	{
		p_game->OnMouseMoved(e);
	}
}

// A button on the mouse was pressed
void Window::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	previous_mouse_x_ = e.X;
	previous_mouse_y_ = e.Y;

	if (auto p_game = p_game_.lock())
	{
		p_game->OnMouseButtonPressed(e);
	}
}

// A button on the mouse was released
void Window::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	if (auto p_game = p_game_.lock())
	{
		p_game->OnMouseButtonReleased(e);
	}
}

// The mouse wheel was moved.
void Window::OnMouseWheel(MouseWheelEventArgs& e)
{
	if (auto p_game = p_game_.lock())
	{
		p_game->OnMouseWheel(e);
	}
}

void Window::OnResize(ResizeEventArgs& e)
{
	// Update the client size.
	if (client_width_ != e.Width || client_height_ != e.Height)
	{
		client_width_ = std::max(1, e.Width);
		client_height_ = std::max(1, e.Height);

		NeelEngine::Get().Flush();

		// Release all references to back buffer textures.
		render_target_.AttachTexture(kColor0, Texture());
		for (int i = 0; i < buffer_count_; ++i)
		{
			ResourceStateTracker::RemoveGlobalResourceState(back_buffer_textures_[i].GetD3D12Resource().Get());
			back_buffer_textures_[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
		ThrowIfFailed(dxgi_swap_chain_->GetDesc(&swap_chain_desc));
		ThrowIfFailed(dxgi_swap_chain_->ResizeBuffers(buffer_count_, client_width_,
		                                              client_height_, swap_chain_desc.BufferDesc.Format,
		                                              swap_chain_desc.Flags));

		current_back_buffer_index_ = dxgi_swap_chain_->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews();
	}

	if (auto p_game = p_game_.lock())
	{
		p_game->OnResize(e);
	}
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
	NeelEngine& app = NeelEngine::Get();

	Microsoft::WRL::ComPtr<IDXGISwapChain4> dxgi_swap_chain4;
	Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory4;
	UINT create_factory_flags = 0;
#if defined(_DEBUG)
	create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dxgi_factory4)));

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
	swap_chain_desc.Width		= client_width_;
	swap_chain_desc.Height		= client_height_;
	swap_chain_desc.Format		= DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.Stereo		= FALSE;
	swap_chain_desc.SampleDesc	= {1, 0};
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = buffer_count_;
	swap_chain_desc.Scaling		= DXGI_SCALING_STRETCH;
	swap_chain_desc.SwapEffect	= DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.AlphaMode	= DXGI_ALPHA_MODE_UNSPECIFIED;
	swap_chain_desc.Flags		= is_tearing_supported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	swap_chain_desc.Flags		|= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

	ID3D12CommandQueue* p_command_queue = app.GetCommandQueue()->GetD3D12CommandQueue().Get();

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain1;
	ThrowIfFailed(dxgi_factory4->CreateSwapChainForHwnd(
		p_command_queue,
		h_wnd_,
		&swap_chain_desc,
		nullptr,
		nullptr,
		&swap_chain1));

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	ThrowIfFailed(dxgi_factory4->MakeWindowAssociation(h_wnd_, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swap_chain1.As(&dxgi_swap_chain4));

	current_back_buffer_index_ = dxgi_swap_chain4->GetCurrentBackBufferIndex();
	dxgi_swap_chain4->SetMaximumFrameLatency(buffer_count_ - 1);
	swap_chain_event_ = dxgi_swap_chain4->GetFrameLatencyWaitableObject();

	return dxgi_swap_chain4;
}

void Window::UpdateRenderTargetViews()
{
	for (auto i = 0; i < buffer_count_; ++i)
	{
		ComPtr<ID3D12Resource> back_buffer;
		ThrowIfFailed(dxgi_swap_chain_->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));

		ResourceStateTracker::AddGlobalResourceState(back_buffer.Get(), D3D12_RESOURCE_STATE_COMMON);

		back_buffer_textures_[i].SetD3D12Resource(back_buffer);
		back_buffer_textures_[i].CreateViews();
	}
}

const RenderTarget& Window::GetRenderTarget() const
{
	render_target_.AttachTexture(AttachmentPoint::kColor0, back_buffer_textures_[current_back_buffer_index_]);
	return render_target_;
}

UINT Window::Present(const Texture& texture)
{
	auto command_queue = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list  = command_queue->GetCommandList();

	auto& back_buffer = back_buffer_textures_[current_back_buffer_index_];

	if (texture.IsValid())
	{
		if (texture.GetD3D12ResourceDesc().SampleDesc.Count > 1)
		{
			command_list->ResolveSubresource(back_buffer, texture);
		}
		else
		{
			command_list->CopyResource(back_buffer, texture);
		}
	}

	RenderTarget render_target;
	render_target.AttachTexture(AttachmentPoint::kColor0, back_buffer);

	//gui_.Render(command_list, render_target);

	command_list->TransitionBarrier(back_buffer, D3D12_RESOURCE_STATE_PRESENT);
	command_queue->ExecuteCommandList(command_list);

	UINT sync_interval = v_sync_ ? 1 : 0;
	UINT present_flags = is_tearing_supported_ && !v_sync_ ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(dxgi_swap_chain_->Present(sync_interval, present_flags));

	fence_values_[current_back_buffer_index_] = command_queue->Signal();
	frame_values_[current_back_buffer_index_] = NeelEngine::GetFrameCount();

	current_back_buffer_index_ = dxgi_swap_chain_->GetCurrentBackBufferIndex();

	command_queue->WaitForFenceValue(fence_values_[current_back_buffer_index_]);

	NeelEngine::Get().ReleaseStaleDescriptors(frame_values_[current_back_buffer_index_]);

	return current_back_buffer_index_;
}
