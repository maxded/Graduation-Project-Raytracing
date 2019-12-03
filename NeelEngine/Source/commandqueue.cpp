#include "neel_engine_pch.h"

#include "commandqueue.h"

#include "neel_engine.h"
#include "resource_state_tracker.h"
#include "commandlist.h"

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type)
	: command_list_type_(type)
	  , fence_value_(0)
	  , b_process_in_flight_command_lists_(true)
{
	auto device = NeelEngine::Get().GetDevice();

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12_command_queue_)));
	ThrowIfFailed(device->CreateFence(fence_value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12_fence_)));

	switch (type)
	{
		case D3D12_COMMAND_LIST_TYPE_COPY:
			d3d12_command_queue_->SetName(L"Copy Command Queue");
			break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			d3d12_command_queue_->SetName(L"Compute Command Queue");
			break;
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			d3d12_command_queue_->SetName(L"Direct Command Queue");
			break;
		default:
			break;
	}

	process_in_flight_command_lists_thread_ = std::thread(&CommandQueue::ProccessInFlightCommandLists, this);
}

CommandQueue::~CommandQueue()
{
	b_process_in_flight_command_lists_ = false;
	process_in_flight_command_lists_thread_.join();
}

uint64_t CommandQueue::Signal()
{
	uint64_t fence_value = ++fence_value_;
	d3d12_command_queue_->Signal(d3d12_fence_.Get(), fence_value);
	return fence_value;
}

bool CommandQueue::IsFenceComplete(uint64_t fence_value) const
{
	return d3d12_fence_->GetCompletedValue() >= fence_value;
}

void CommandQueue::WaitForFenceValue(uint64_t fence_value) const
{
	if (!IsFenceComplete(fence_value))
	{
		auto event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(event && "Failed to create fence event handle.");

		// Is this function thread safe?
		d3d12_fence_->SetEventOnCompletion(fence_value, event);
		::WaitForSingleObject(event, DWORD_MAX);

		::CloseHandle(event);
	}
}

void CommandQueue::Flush()
{
	std::unique_lock<std::mutex> lock(process_in_flight_command_lists_thread_mutex_);
	process_in_flight_command_lists_thread_cv_.wait(lock, [this] { return in_flight_command_lists_.Empty(); });

	// In case the command queue was signaled directly 
	// using the CommandQueue::Signal method then the 
	// fence value of the command queue might be higher than the fence
	// value of any of the executed command lists.
	WaitForFenceValue(fence_value_);
}

std::shared_ptr<CommandList> CommandQueue::GetCommandList()
{
	std::shared_ptr<CommandList> command_list;

	// If there is a command list on the queue.
	if (!available_command_lists_.Empty())
	{
		available_command_lists_.TryPop(command_list);
	}
	else
	{
		// Otherwise create a new command list.
		command_list = std::make_shared<CommandList>(command_list_type_);
	}

	return command_list;
}

// Execute a command list.
// Returns the fence value to wait for for this command list.
uint64_t CommandQueue::ExecuteCommandList(std::shared_ptr<CommandList> command_list)
{
	return ExecuteCommandLists(std::vector<std::shared_ptr<CommandList>>({command_list}));
}

uint64_t CommandQueue::ExecuteCommandLists(const std::vector<std::shared_ptr<CommandList>>& command_lists)
{
	ResourceStateTracker::Lock();

	// Command lists that need to put back on the command list queue.
	std::vector<std::shared_ptr<CommandList>> to_be_queued;
	to_be_queued.reserve(command_lists.size() * 2); // 2x since each command list will have a pending command list.

	// Command lists that need to be executed.
	std::vector<ID3D12CommandList*> d3d12_command_lists;
	d3d12_command_lists.reserve(command_lists.size() * 2); // 2x since each command list will have a pending command list.

	for (auto command_list : command_lists)
	{
		auto pending_command_list = GetCommandList();
		bool has_pending_barriers = command_list->Close(*pending_command_list);
		pending_command_list->Close();
		// If there are no pending barriers on the pending command list, there is no reason to 
		// execute an empty command list on the command queue.
		if (has_pending_barriers)
		{
			d3d12_command_lists.push_back(pending_command_list->GetGraphicsCommandList().Get());
		}
		d3d12_command_lists.push_back(command_list->GetGraphicsCommandList().Get());

		to_be_queued.push_back(pending_command_list);
		to_be_queued.push_back(command_list);
	}

	UINT num_command_lists = static_cast<UINT>(d3d12_command_lists.size());
	d3d12_command_queue_->ExecuteCommandLists(num_command_lists, d3d12_command_lists.data());
	uint64_t fence_value = Signal();

	ResourceStateTracker::Unlock();

	// Queue command lists for reuse.
	for (auto command_list : to_be_queued)
	{
		in_flight_command_lists_.Push({fence_value, command_list});
	}

	return fence_value;
}

void CommandQueue::Wait(const CommandQueue& other) const
{
	d3d12_command_queue_->Wait(other.d3d12_fence_.Get(), other.fence_value_);
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
	return d3d12_command_queue_;
}

void CommandQueue::ProccessInFlightCommandLists()
{
	std::unique_lock<std::mutex> lock(process_in_flight_command_lists_thread_mutex_, std::defer_lock);

	while (b_process_in_flight_command_lists_)
	{
		CommandListEntry command_list_entry;

		lock.lock();
		while (in_flight_command_lists_.TryPop(command_list_entry))
		{
			auto fence_value = std::get<0>(command_list_entry);
			auto command_list = std::get<1>(command_list_entry);

			WaitForFenceValue(fence_value);

			command_list->Reset();

			available_command_lists_.Push(command_list);
		}
		lock.unlock();
		process_in_flight_command_lists_thread_cv_.notify_one();

		std::this_thread::yield();
	}
}
