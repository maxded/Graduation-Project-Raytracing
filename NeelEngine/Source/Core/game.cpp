#include "neel_engine_pch.h"

#include "game.h"
#include "neel_engine.h"
#include "window.h"
#include "camera.h"

Game::Game(const std::wstring& name, uint16_t width, uint16_t height, bool vSync)
	: name_(name)
	  , width_(width)
	  , height_(height)
	  , vsync_(vSync)
	  , forward_(0)
	  , backward_(0)
	  , left_(0)
	  , right_(0)
	  , up_(0)
	  , down_(0)
	  , pitch_(0)
	  , yaw_(0)
{
	
}

Game::~Game()
{
	assert(!p_window_ && "Use Game::Destroy() before destruction.");
}

bool Game::Initialize()
{
	// Check for DirectX Math library support.
	if (!DirectX::XMVerifyCPUSupport())
	{
		MessageBoxA(nullptr, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	p_window_ = NeelEngine::Get().CreateRenderWindow(name_, width_, height_, vsync_);
	p_window_->RegisterCallbacks(shared_from_this());
	p_window_->Show();

	return true;
}

void Game::Destroy()
{
	Camera::Destroy();
	NeelEngine::Get().DestroyWindow(p_window_);
	p_window_.reset();
}

void Game::OnUpdate(UpdateEventArgs& e)
{
	Camera& camera = Camera::Get();

	// Update the camera.
	XMVECTOR camera_translate = XMVectorSet(right_ - left_, 0.0f, forward_ - backward_, 1.0f) * 8.0f * static_cast<
		float>(e.ElapsedTime);
	XMVECTOR camera_pan = XMVectorSet(0.0f, up_ - down_, 0.0f, 1.0f) * 8.0f * static_cast<float>(e.ElapsedTime);

	camera.Translate(camera_translate, Space::kLocal);
	camera.Translate(camera_pan, Space::kLocal);

	XMVECTOR camera_rotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(pitch_), XMConvertToRadians(yaw_),
	                                                           0.0f);
	camera.SetRotation(camera_rotation);
}

void Game::OnRender(RenderEventArgs& e)
{
}

void Game::OnKeyPressed(KeyEventArgs& e)
{
	Camera& camera = Camera::Get();

	if (!ImGui::GetIO().WantCaptureKeyboard)
	{
		switch (e.Key)
		{
		case KeyCode::W:
			forward_ = 1.0f;
			break;
		case KeyCode::A:
			left_ = 1.0f;
			break;
		case KeyCode::S:
			backward_ = 1.0f;
			break;
		case KeyCode::D:
			right_ = 1.0f;
			break;
		case KeyCode::Q:
			down_ = 1.0f;
			break;
		case KeyCode::E:
			up_ = 1.0f;
			break;
		default:;
		}
	}
}

void Game::OnKeyReleased(KeyEventArgs& e)
{
	if (!ImGui::GetIO().WantCaptureKeyboard)
	{
		switch (e.Key)
		{
		case KeyCode::Up:
		case KeyCode::W:
			forward_ = 0.0f;
			break;
		case KeyCode::Left:
		case KeyCode::A:
			left_ = 0.0f;
			break;
		case KeyCode::Down:
		case KeyCode::S:
			backward_ = 0.0f;
			break;
		case KeyCode::Right:
		case KeyCode::D:
			right_ = 0.0f;
			break;
		case KeyCode::Q:
			down_ = 0.0f;
			break;
		case KeyCode::E:
			up_ = 0.0f;
			break;
		default:
			break;
		}
	}
}

void Game::OnMouseMoved(MouseMotionEventArgs& e)
{
	const float mouse_speed = 0.2f;

	if (!ImGui::GetIO().WantCaptureMouse)
	{
		if (e.RightButton)
		{
			pitch_ += e.RelY * mouse_speed;

			pitch_ = clamp(pitch_, -90.0f, 90.0f);

			yaw_ += e.RelX * mouse_speed;
		}
	}
}

void Game::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseWheel(MouseWheelEventArgs& e)
{
	if (!ImGui::GetIO().WantCaptureMouse)
	{
		Camera& camera = Camera::Get();

		auto fov = camera.GetFoV();

		fov -= e.WheelDelta;
		fov = clamp(fov, 12.0f, 90.0f);

		camera.SetFoV(fov);
	}
}

void Game::OnResize(ResizeEventArgs& e)
{
	width_ = e.Width;
	height_ = e.Height;
}

void Game::OnWindowDestroy()
{
	// If the Window which we are registered to is 
	// destroyed, then any resources which are associated 
	// to the window must be released.
	UnloadContent();
}
