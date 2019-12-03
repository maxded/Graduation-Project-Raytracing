#include "neel_engine_pch.h"

#include "neel_engine.h"
#include "window.h"
#include "game.h"
#include "descriptor_allocator.h"
#include "commandqueue.h"

constexpr wchar_t kWindowClassName[] = L"Graphics Practise Environment";

using WindowPtr = std::shared_ptr<Window>;

static NeelEngine* p_singleton = nullptr;
static WindowPtr p_window = nullptr;

uint64_t NeelEngine::frame_count_ = 0;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

// A wrapper struct to allow shared pointers for the window class.
// This is needed because the constructor and destructor for the Window
// class are protected and not accessible by the std::make_shared method.
struct MakeWindow : public Window
{
	MakeWindow(HWND h_wnd, const std::wstring& window_name, int client_width, int client_height, bool v_sync)
		: Window(h_wnd, window_name, client_width, client_height, v_sync)
	{
	}
};

NeelEngine::NeelEngine(HINSTANCE h_inst)
	: h_instance_(h_inst)
	  , tearing_supported_(false)
{
	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	WNDCLASSEXW wnd_class = {0};

	wnd_class.cbSize = sizeof(WNDCLASSEX);
	wnd_class.style = CS_HREDRAW | CS_VREDRAW;
	wnd_class.lpfnWndProc = &WndProc;
	wnd_class.hInstance = h_instance_;
	wnd_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wnd_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wnd_class.lpszMenuName = nullptr;
	wnd_class.lpszClassName = kWindowClassName;

	if (!RegisterClassExW(&wnd_class))
	{
		MessageBoxA(nullptr, "Unable to register the window class.", "Error", MB_OK | MB_ICONERROR);
	}
}

NeelEngine::~NeelEngine()
{
	Flush();
}

void NeelEngine::Initialize()
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug1> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
	// Enable these if you want full validation (will slow down rendering a lot).
	//debugInterface->SetEnableGPUBasedValidation(TRUE);
	//debugInterface->SetEnableSynchronizedCommandQueueValidation(TRUE);
#endif

	auto dxgi_adapter = GetAdapter(false);
	if (!dxgi_adapter)
	{
		// If no supporting DX12 adapters exist, fall back to WARP
		dxgi_adapter = GetAdapter(true);
	}

	if (dxgi_adapter)
	{
		d3d12_device_ = CreateDevice(dxgi_adapter);
	}
	else
	{
		throw std::exception("DXGI adapter enumeration failed.");
	}

	direct_command_queue_ = std::make_shared<CommandQueue>(D3D12_COMMAND_LIST_TYPE_DIRECT);
	compute_command_queue_ = std::make_shared<CommandQueue>(D3D12_COMMAND_LIST_TYPE_COMPUTE);
	copy_command_queue_ = std::make_shared<CommandQueue>(D3D12_COMMAND_LIST_TYPE_COPY);

	tearing_supported_ = CheckTearingSupport();

	// Create descriptor allocators
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		descriptor_allocators_[i] = std::make_unique<DescriptorAllocator>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
	}

	// Initialize frame counter 
	frame_count_ = 0;
}

void NeelEngine::Create(HINSTANCE h_inst)
{
	if (!p_singleton)
	{
		p_singleton = new NeelEngine(h_inst);
		p_singleton->Initialize();
	}
}

NeelEngine& NeelEngine::Get()
{
	assert(p_singleton);
	return *p_singleton;
}

void NeelEngine::Destroy()
{
	if (p_singleton)
	{
		assert(!p_window && "Window should be destroyed before destorying the application instance");

		delete p_singleton;
		p_singleton = nullptr;
	}
}

Microsoft::WRL::ComPtr<IDXGIAdapter4> NeelEngine::GetAdapter(bool b_use_warp) const
{
	Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgi_factory)));

	Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgi_adapter1;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> dxgi_adapter4;

	if (b_use_warp)
	{
		ThrowIfFailed(dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&dxgi_adapter1)));
		ThrowIfFailed(dxgi_adapter1.As(&dxgi_adapter4));
	}
	else
	{
		SIZE_T max_dedicated_video_memory = 0;
		for (UINT i = 0; dxgi_factory->EnumAdapters1(i, &dxgi_adapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgi_adapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgi_adapter1.Get(),
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > max_dedicated_video_memory)
			{
				max_dedicated_video_memory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgi_adapter1.As(&dxgi_adapter4));
			}
		}
	}

	return dxgi_adapter4;
}

Microsoft::WRL::ComPtr<ID3D12Device2> NeelEngine::CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter) const
{
	ComPtr<ID3D12Device2> d3d12_device2;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12_device2)));
	//    NAME_D3D12_OBJECT(d3d12Device2);

	// Enable debug messages in debug mode.
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> p_info_queue;
	if (SUCCEEDED(d3d12_device2.As(&p_info_queue)))
	{
		p_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		p_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		p_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID deny_ids[] = {
			D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES,
			// This started happening after updating to an RTX 2080 Ti. I believe this to be an error in the validation layer itself.
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			// I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			// This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			// This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER new_filter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		new_filter.DenyList.NumSeverities = _countof(severities);
		new_filter.DenyList.pSeverityList = severities;
		new_filter.DenyList.NumIDs = _countof(deny_ids);
		new_filter.DenyList.pIDList = deny_ids;

		ThrowIfFailed(p_info_queue->PushStorageFilter(&new_filter));
	}
#endif

	return d3d12_device2;
}

bool NeelEngine::CheckTearingSupport() const
{
	BOOL allow_tearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			                              &allow_tearing, sizeof(allow_tearing));
		}
	}

	return allow_tearing == TRUE;
}

bool NeelEngine::IsTearingSupported() const
{
	return tearing_supported_;
}

DXGI_SAMPLE_DESC NeelEngine::GetMultisampleQualityLevels(DXGI_FORMAT format, UINT num_samples,
                                                         D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags) const
{
	DXGI_SAMPLE_DESC sample_desc = {1, 0};

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality_levels;
	quality_levels.Format = format;
	quality_levels.SampleCount = 1;
	quality_levels.Flags = flags;
	quality_levels.NumQualityLevels = 0;

	while (quality_levels.SampleCount <= num_samples && SUCCEEDED(
		d3d12_device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &quality_levels, sizeof(
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS))) && quality_levels.NumQualityLevels > 0)
	{
		// That works...
		sample_desc.Count = quality_levels.SampleCount;
		sample_desc.Quality = quality_levels.NumQualityLevels - 1;

		// But can we do better?
		quality_levels.SampleCount *= 2;
	}

	return sample_desc;
}

std::shared_ptr<Window> NeelEngine::CreateRenderWindow(const std::wstring& window_name, uint16_t client_width,
                                                       uint16_t client_height, bool v_sync) const
{
	// First check if a window already exists
	if (p_window)
	{
		return p_window;
	}

	RECT window_rect = {0, 0, client_width, client_height};
	AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

	HWND h_wnd = CreateWindowW(kWindowClassName, window_name.c_str(),
	                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
	                          window_rect.right - window_rect.left,
	                          window_rect.bottom - window_rect.top,
	                          nullptr, nullptr, h_instance_, nullptr);

	if (!h_wnd)
	{
		MessageBoxA(nullptr, "Could not create the render window.", "Error", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	WindowPtr window = std::make_shared<MakeWindow>(h_wnd, window_name, client_width, client_height, v_sync);
	p_window = window;

	return p_window;
}

void NeelEngine::DestroyWindow(std::shared_ptr<Window> window)
{
	if (window)
	{
		window->Destroy();
	}
}

std::shared_ptr<Window> NeelEngine::GetWindow()
{
	assert(p_window && "Window does not exist");
	return p_window;
}

int NeelEngine::Run(std::shared_ptr<Game> p_game) const
{
	if (!p_game->Initialize()) return 1;
	if (!p_game->LoadContent()) return 2;

	MSG msg = {nullptr};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	// Flush any commands in the commands queues before quiting.
	Flush();

	p_game->UnloadContent();
	p_game->Destroy();

	return static_cast<int>(msg.wParam);
}

void NeelEngine::Quit(int exit_code)
{
	PostQuitMessage(exit_code);
}

Microsoft::WRL::ComPtr<ID3D12Device2> NeelEngine::GetDevice() const
{
	return d3d12_device_;
}

std::shared_ptr<CommandQueue> NeelEngine::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const
{
	std::shared_ptr<CommandQueue> command_queue;
	switch (type)
	{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			command_queue = direct_command_queue_;
			break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			command_queue = compute_command_queue_;
			break;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			command_queue = copy_command_queue_;
			break;
		default:
			assert(false && "Invalid command queue type.");
	}

	return command_queue;
}

void NeelEngine::Flush() const
{
	direct_command_queue_->Flush();
	compute_command_queue_->Flush();
	copy_command_queue_->Flush();
}

DescriptorAllocation NeelEngine::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors)
{
	return descriptor_allocators_[type]->Allocate(num_descriptors);
}

void NeelEngine::ReleaseStaleDescriptors(uint64_t finished_frame)
{
	for (auto& descriptor_allocator : descriptor_allocators_)
	{
		descriptor_allocator->ReleaseStaleDescriptors(finished_frame);
	}
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> NeelEngine::CreateDescriptorHeap(
	UINT num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = num_descriptors;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
	ThrowIfFailed(d3d12_device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap)));

	return descriptor_heap;
}

UINT NeelEngine::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
	return d3d12_device_->GetDescriptorHandleIncrementSize(type);
}

// Convert the message ID into a MouseButton ID
MouseButtonEventArgs::MouseButton DecodeMouseButton(UINT message_id)
{
	MouseButtonEventArgs::MouseButton mouse_button = MouseButtonEventArgs::kNone;
	switch (message_id)
	{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
			{
				mouse_button = MouseButtonEventArgs::kLeft;
			}
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
			{
				mouse_button = MouseButtonEventArgs::kRight;
			}
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
			{
				mouse_button = MouseButtonEventArgs::kMiddel;
			}
			break;
		default:
			break;
	}

	return mouse_button;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
	if (p_window)
	{
		switch (message)
		{
			case WM_PAINT:
				{
					++NeelEngine::frame_count_;

					// Delta time will be filled in by the Window.
					UpdateEventArgs update_event_args(0.0f, 0.0f, NeelEngine::frame_count_);
					p_window->OnUpdate(update_event_args);
					RenderEventArgs render_event_args(0.0f, 0.0f, NeelEngine::frame_count_);
					// Delta time will be filled in by the Window.
					p_window->OnRender(render_event_args);
				}
				break;
			case WM_SYSKEYDOWN:
			case WM_KEYDOWN:
				{
					MSG char_msg;
					// Get the Unicode character (UTF-16)
					unsigned int c = 0;
					// For printable characters, the next message will be WM_CHAR.
					// This message contains the character code we need to send the KeyPressed event.
					// Inspired by the SDL 1.2 implementation.
					if (PeekMessage(&char_msg, hwnd, 0, 0, PM_NOREMOVE) && char_msg.message == WM_CHAR)
					{
						GetMessage(&char_msg, hwnd, 0, 0);
						c = static_cast<unsigned int>(char_msg.wParam);
					}
					bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
					bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
					bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
					KeyCode::Key key = static_cast<KeyCode::Key>(w_param);
					unsigned int scan_code = (l_param & 0x00FF0000) >> 16;
					KeyEventArgs key_event_args(key, c, KeyEventArgs::kPressed, shift, control, alt);
					p_window->OnKeyPressed(key_event_args);
				}
				break;
			case WM_SYSKEYUP:
			case WM_KEYUP:
				{
					bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
					bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
					bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
					KeyCode::Key key = static_cast<KeyCode::Key>(w_param);
					unsigned int c = 0;
					unsigned int scan_code = (l_param & 0x00FF0000) >> 16;

					// Determine which key was released by converting the key code and the scan code
					// to a printable character (if possible).
					// Inspired by the SDL 1.2 implementation.
					unsigned char keyboard_state[256];
					GetKeyboardState(keyboard_state);
					wchar_t translated_characters[4];
					if (int result = ToUnicodeEx(static_cast<UINT>(w_param), scan_code, keyboard_state,
					                             translated_characters,
					                             4, 0, NULL) > 0)
					{
						c = translated_characters[0];
					}

					KeyEventArgs key_event_args(key, c, KeyEventArgs::kReleased, shift, control, alt);
					p_window->OnKeyReleased(key_event_args);
				}
				break;
				// The default window procedure will play a system notification sound 
				// when pressing the Alt+Enter keyboard combination if this message is 
				// not handled.
			case WM_SYSCHAR:
				break;
			case WM_MOUSEMOVE:
				{
					bool l_button = (w_param & MK_LBUTTON) != 0;
					bool r_button = (w_param & MK_RBUTTON) != 0;
					bool m_button = (w_param & MK_MBUTTON) != 0;
					bool shift = (w_param & MK_SHIFT) != 0;
					bool control = (w_param & MK_CONTROL) != 0;

					int x = static_cast<int>(static_cast<short>(LOWORD(l_param)));
					int y = static_cast<int>(static_cast<short>(HIWORD(l_param)));

					MouseMotionEventArgs mouse_motion_event_args(l_button, m_button, r_button, control, shift, x, y);
					p_window->OnMouseMoved(mouse_motion_event_args);
				}
				break;
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
				{
					bool l_button = (w_param & MK_LBUTTON) != 0;
					bool r_button = (w_param & MK_RBUTTON) != 0;
					bool m_button = (w_param & MK_MBUTTON) != 0;
					bool shift = (w_param & MK_SHIFT) != 0;
					bool control = (w_param & MK_CONTROL) != 0;

					int x = static_cast<int>(static_cast<short>(LOWORD(l_param)));
					int y = static_cast<int>(static_cast<short>(HIWORD(l_param)));

					MouseButtonEventArgs mouse_button_event_args(DecodeMouseButton(message),
					                                          MouseButtonEventArgs::kPressed,
					                                          l_button, m_button, r_button, control, shift, x, y);
					p_window->OnMouseButtonPressed(mouse_button_event_args);
				}
				break;
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
				{
					bool l_button = (w_param & MK_LBUTTON) != 0;
					bool r_button = (w_param & MK_RBUTTON) != 0;
					bool m_button = (w_param & MK_MBUTTON) != 0;
					bool shift = (w_param & MK_SHIFT) != 0;
					bool control = (w_param & MK_CONTROL) != 0;

					int x = static_cast<int>(static_cast<short>(LOWORD(l_param)));
					int y = static_cast<int>(static_cast<short>(HIWORD(l_param)));

					MouseButtonEventArgs mouse_button_event_args(DecodeMouseButton(message),
					                                          MouseButtonEventArgs::kReleased,
					                                          l_button, m_button, r_button, control, shift, x, y);
					p_window->OnMouseButtonReleased(mouse_button_event_args);
				}
				break;
			case WM_MOUSEWHEEL:
				{
					// The distance the mouse wheel is rotated.
					// A positive value indicates the wheel was rotated to the right.
					// A negative value indicates the wheel was rotated to the left.
					float z_delta = static_cast<int>(static_cast<short>(HIWORD(w_param))) / static_cast<float>(
						WHEEL_DELTA);
					short key_states = static_cast<short>(LOWORD(w_param));

					bool l_button = (key_states & MK_LBUTTON) != 0;
					bool r_button = (key_states & MK_RBUTTON) != 0;
					bool m_button = (key_states & MK_MBUTTON) != 0;
					bool shift = (key_states & MK_SHIFT) != 0;
					bool control = (key_states & MK_CONTROL) != 0;

					int x = static_cast<int>(static_cast<short>(LOWORD(l_param)));
					int y = static_cast<int>(static_cast<short>(HIWORD(l_param)));

					// Convert the screen coordinates to client coordinates.
					POINT client_to_screen_point;
					client_to_screen_point.x = x;
					client_to_screen_point.y = y;
					ScreenToClient(hwnd, &client_to_screen_point);

					MouseWheelEventArgs mouse_wheel_event_args(z_delta, l_button, m_button, r_button, control, shift,
					                                        static_cast<int>(client_to_screen_point.x),
					                                        static_cast<int>(client_to_screen_point.y));
					p_window->OnMouseWheel(mouse_wheel_event_args);
				}
				break;
			case WM_SIZE:
				{
					int width = static_cast<int>(static_cast<short>(LOWORD(l_param)));
					int height = static_cast<int>(static_cast<short>(HIWORD(l_param)));

					ResizeEventArgs resize_event_args(width, height);
					p_window->OnResize(resize_event_args);
				}
				break;
			case WM_DESTROY:
				{
					p_window.reset();
					PostQuitMessage(0);
				}
				break;
			default:
				return DefWindowProcW(hwnd, message, w_param, l_param);
		}
	}
	else
	{
		return DefWindowProcW(hwnd, message, w_param, l_param);
	}

	return 0;
}
