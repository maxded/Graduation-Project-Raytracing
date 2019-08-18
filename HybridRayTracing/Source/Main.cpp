#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <Application.h>
#include <HybridRayTracingDemo.h>

#include <dxgidebug.h>

void ReportLiveObjects()
{
	IDXGIDebug1* dxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	dxgiDebug->Release();
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	int retCode = 0;

	Application::Create(hInstance);
	{
		std::shared_ptr<HybridRayTracingDemo> game = std::make_shared<HybridRayTracingDemo>(L"Hybrid Ray Tracing Demo", 1280, 720);
		retCode = Application::Get().Run(game);
	}
	Application::Destroy();

	atexit(&ReportLiveObjects);

	return retCode;
}