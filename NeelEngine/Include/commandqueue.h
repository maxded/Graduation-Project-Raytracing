#pragma once

#include <queue>

#include <d3d12.h>
#include <wrl.h>

#include <atomic>
#include <cstdint>
#include <condition_variable>

#include "thread_safe_queue.h"

class CommandList;

class CommandQueue
{
public:
	CommandQueue(D3D12_COMMAND_LIST_TYPE type);
	virtual ~CommandQueue();

	// Get an available command list from the command queue.
	std::shared_ptr<CommandList> GetCommandList();

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64_t ExecuteCommandList(std::shared_ptr<CommandList> command_list);
	uint64_t ExecuteCommandLists(const std::vector<std::shared_ptr<CommandList>>& command_lists);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t fence_value) const;
	void WaitForFenceValue(uint64_t fence_value) const;
	void Flush();

	// Wait for another command queue to finish.
	void Wait(const CommandQueue& other) const;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

private:
	// Free any command lists that are finished processing on the command queue.
	void ProccessInFlightCommandLists();

	// Keep track of command allocators that are "in-flight"
	// The first member is the fence value to wait for, the second is the 
	// a shared pointer to the "in-flight" command list.
	using CommandListEntry = std::tuple<uint64_t, std::shared_ptr<CommandList>>;

	D3D12_COMMAND_LIST_TYPE command_list_type_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_command_queue_;
	Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_fence_;
	std::atomic_uint64_t fence_value_;

	ThreadSafeQueue<CommandListEntry> in_flight_command_lists_;
	ThreadSafeQueue<std::shared_ptr<CommandList>> available_command_lists_;

	// A thread to process in-flight command lists.
	std::thread process_in_flight_command_lists_thread_;
	std::atomic_bool b_process_in_flight_command_lists_;
	std::mutex process_in_flight_command_lists_thread_mutex_;
	std::condition_variable process_in_flight_command_lists_thread_cv_;
};
