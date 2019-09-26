#include <FrameworkPCH.h>

#include <Game.h>
#include <Application.h>
#include <Window.h>
#include <Camera.h>

#include <algorithm>

Game::Game(const std::wstring& name, uint16_t width, uint16_t height, bool vSync)
	: m_Name(name)
	, m_Width(width)
	, m_Height(height)
	, m_vSync(vSync)
	, m_Forward(0)
	, m_Backward(0)
	, m_Left(0)
	, m_Right(0)
	, m_Up(0)
	, m_Down(0)
	, m_Pitch(0)
	, m_Yaw(0)
{
	Camera::Create();

	XMVECTOR cameraPos		= XMVectorSet(0, 0, -20, 1);
	XMVECTOR cameraTarget	= XMVectorSet(0, 0, 0, 1);
	XMVECTOR cameraUp		= XMVectorSet(0, 1, 0, 0);

	Camera& camera = Camera::Get();

	camera.set_LookAt(cameraPos, cameraTarget, cameraUp);
	camera.set_Projection(45.0f, width / (float)height, 0.1f, 100.0f);
}

Game::~Game()
{
	assert(!m_pWindow && "Use Game::Destroy() before destruction.");
}

bool Game::Initialize()
{
	// Check for DirectX Math library support.
	if (!DirectX::XMVerifyCPUSupport())
	{
		MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	m_pWindow = Application::Get().CreateRenderWindow(m_Name, m_Width, m_Height, m_vSync);
	m_pWindow->RegisterCallbacks(shared_from_this());
	m_pWindow->Show();

	return true;
}

void Game::Destroy()
{
	Camera::Destroy();
	Application::Get().DestroyWindow(m_pWindow);	
	m_pWindow.reset();
}

void Game::OnUpdate(UpdateEventArgs& e)
{
	Camera& camera = Camera::Get();

	// Update the camera.
	XMVECTOR cameraTranslate = XMVectorSet(m_Right - m_Left, 0.0f, m_Forward - m_Backward, 1.0f) * 8.0f * static_cast<float>(e.ElapsedTime);
	XMVECTOR cameraPan		 = XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) * 8.0f * static_cast<float>(e.ElapsedTime);

	camera.Translate(cameraTranslate, Space::Local);
	camera.Translate(cameraPan, Space::Local);

	XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_Pitch), XMConvertToRadians(m_Yaw), 0.0f);
	camera.set_Rotation(cameraRotation);
}

void Game::OnRender(RenderEventArgs& e)
{

}

void Game::OnKeyPressed(KeyEventArgs& e)
{
	Camera& camera = Camera::Get();

	switch (e.Key)
	{
	case KeyCode::Up:
	case KeyCode::W:
		m_Forward = 1.0f;
		break;
	case KeyCode::Left:
	case KeyCode::A:
		m_Left = 1.0f;
		break;
	case KeyCode::Down:
	case KeyCode::S:
		m_Backward = 1.0f;
		break;
	case KeyCode::Right:
	case KeyCode::D:
		m_Right = 1.0f;
		break;
	case KeyCode::Q:
		m_Down = 1.0f;
		break;
	case KeyCode::E:
		m_Up = 1.0f;
		break;
	}
}

void Game::OnKeyReleased(KeyEventArgs& e)
{
	switch (e.Key)
	{
	case KeyCode::Up:
	case KeyCode::W:
		m_Forward = 0.0f;
		break;
	case KeyCode::Left:
	case KeyCode::A:
		m_Left = 0.0f;
		break;
	case KeyCode::Down:
	case KeyCode::S:
		m_Backward = 0.0f;
		break;
	case KeyCode::Right:
	case KeyCode::D:
		m_Right = 0.0f;
		break;
	case KeyCode::Q:
		m_Down = 0.0f;
		break;
	case KeyCode::E:
		m_Up = 0.0f;
		break;
	}
}

void Game::OnMouseMoved(class MouseMotionEventArgs& e)
{
	const float mouseSpeed = 0.1f;

	if (e.LeftButton)
	{
		m_Pitch -= e.RelY * mouseSpeed;

		m_Pitch = clamp(m_Pitch, -90.0f, 90.0f);

		m_Yaw -= e.RelX * mouseSpeed;
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
	Camera& camera = Camera::Get();

	auto fov = camera.get_FoV();

	fov -= e.WheelDelta;
	fov = clamp(fov, 12.0f, 90.0f);

	camera.set_FoV(fov);
}

void Game::OnResize(ResizeEventArgs& e)
{
	m_Width = e.Width;
	m_Height = e.Height;
}

void Game::OnWindowDestroy()
{
	// If the Window which we are registered to is 
	// destroyed, then any resources which are associated 
	// to the window must be released.
	UnloadContent();
}
