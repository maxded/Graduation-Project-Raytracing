#define WIN32_LEAN_AND_MEAN

#include "neel_engine.h"
#include "reflections_demo.h"

#include <dxgidebug.h>

void ReportLiveObjects()
{
	IDXGIDebug1* dxgi_debug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug));

	dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	dxgi_debug->Release();
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	int ret_code = 0;

	NeelEngine::Create(hInstance);
	{
		std::shared_ptr<ReflectionsDemo> game = std::make_shared<ReflectionsDemo>(L"Hybrid Ray Tracing Demo", 1280, 720);
		ret_code = NeelEngine::Get().Run(game);
	}
	NeelEngine::Destroy();

	atexit(&ReportLiveObjects);

	return ret_code;
}
