#pragma once

#include "descriptor_allocation.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <memory>
#include <string>

class CommandQueue;
class DescriptorAllocator;
class Game;
class Window;

class NeelEngine
{
public:

	/**
	* Create the application singleton with the application instance handle.
	*/
	static void Create(HINSTANCE h_inst);

	/**
	* Destroy the application instance and all windows created by this application instance.
	*/
	static void Destroy();
	/**
	* Get the application singleton.
	*/
	static NeelEngine& Get();

	/**
	 * Check to see if VSync-off is supported.
	 */
	bool IsTearingSupported() const;

	/**
	 * Check if the requested multisample quality is supported for the given format.
	 */
	DXGI_SAMPLE_DESC GetMultisampleQualityLevels(DXGI_FORMAT format, UINT num_samples,
	                                             D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags =
		                                             D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE) const;

	/**
	* Create a new DirectX11 render window instance.
	* @param window_name The name of the window. This name will appear in the title bar of the window. This name should be unique.
	* @param client_width The width (in pixels) of the window's client area.
	* @param client_height The height (in pixels) of the window's client area.
	* @param v_sync Should the rendering be synchronized with the vertical refresh rate of the screen.
	* @param windowed If true, the window will be created in windowed mode. If false, the window will be created full-screen.
	* @returns The created window instance. If an error occurred while creating the window an invalid
	* window instance is returned. If a window with the given name already exists, that window will be
	* returned.
	*/
	std::shared_ptr<Window> CreateRenderWindow(const std::wstring& window_name, uint16_t client_width,
	                                           uint16_t client_height, bool v_sync = true) const;

	/**
	* Destroy a window given the window reference.
	*/
	void DestroyWindow(std::shared_ptr<Window> window);

	/**
	* Find window
	*/
	std::shared_ptr<Window> GetWindow();

	/**
	* Run the application loop and message pump.
	* @return The error code if an error occurred.
	*/
	int Run(std::shared_ptr<Game> p_game) const;

	/**
	* Request to quit the application and close all windows.
	* @param exit_code The error code to return to the invoking process.
	*/
	void Quit(int exit_code = 0);

	/**
	 * Get the Direct3D 12 device
	 */
	Microsoft::WRL::ComPtr<ID3D12Device5> GetDevice() const;
	/**
	 * Get a command queue. Valid types are:
	 * - D3D12_COMMAND_LIST_TYPE_DIRECT : Can be used for draw, dispatch, or copy commands.
	 * - D3D12_COMMAND_LIST_TYPE_COMPUTE: Can be used for dispatch or copy commands.
	 * - D3D12_COMMAND_LIST_TYPE_COPY   : Can be used for copy commands.
	 */
	std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

	/**
	 * Flush all command queues.
	 */
	void Flush() const;

	/**
	 * Allocate a number of CPU visible descriptors.
	 */
	DescriptorAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors = 1);

	/**
	 * Release stale descriptors. This should only be called with a completed frame counter.
	 */
	void ReleaseStaleDescriptors(uint64_t finished_frame);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT num_descriptors,
	                                                                  D3D12_DESCRIPTOR_HEAP_TYPE type) const;
	UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

	static uint64_t GetFrameCount()
	{
		return frame_count_;
	}

protected:

	// Create an application instance.
	NeelEngine(HINSTANCE h_inst);
	// Destroy the application instance and all windows associated with this application.
	virtual ~NeelEngine();

	// Initialize the application instance.
	void Initialize();

	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter(bool b_use_warp) const;
	Microsoft::WRL::ComPtr<ID3D12Device5> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter) const;
	bool CheckTearingSupport() const;

private:
	friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
	NeelEngine(const NeelEngine& copy) = delete;
	NeelEngine& operator=(const NeelEngine& other) = delete;

	HINSTANCE h_instance_;

	Microsoft::WRL::ComPtr<ID3D12Device5> d3d12_device_;

	std::shared_ptr<CommandQueue> direct_command_queue_;
	std::shared_ptr<CommandQueue> compute_command_queue_;
	std::shared_ptr<CommandQueue> copy_command_queue_;

	std::unique_ptr<DescriptorAllocator> descriptor_allocators_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	bool tearing_supported_;

	static uint64_t frame_count_;
};
