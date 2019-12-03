#include "neel_engine_pch.h"

#include "resource_state_tracker.h"

#include "commandlist.h"
#include "resource.h"

// Static definitions.
std::mutex ResourceStateTracker::global_mutex_;
bool ResourceStateTracker::is_locked_ = false;
ResourceStateTracker::ResourceStateMap ResourceStateTracker::global_resource_state_;

ResourceStateTracker::ResourceStateTracker()
{
}

ResourceStateTracker::~ResourceStateTracker()
{
}

void ResourceStateTracker::ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
{
	if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
	{
		const D3D12_RESOURCE_TRANSITION_BARRIER& transition_barrier = barrier.Transition;

		// First check if there is already a known "final" State for the given resource.
		// If there is, the resource has been used on the command list before and
		// already has a known State within the command list execution.
		const auto iter = final_resource_state_.find(transition_barrier.pResource);
		if (iter != final_resource_state_.end())
		{
			auto& resource_state = iter->second;
			// If the known final State of the resource is different...
			if (transition_barrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
				!resource_state.SubresourceState.empty())
			{
				// First transition all of the subresources if they are different than the StateAfter.
				for (auto subresource_state : resource_state.SubresourceState)
				{
					if (transition_barrier.StateAfter != subresource_state.second)
					{
						D3D12_RESOURCE_BARRIER new_barrier = barrier;
						new_barrier.Transition.Subresource = subresource_state.first;
						new_barrier.Transition.StateBefore = subresource_state.second;
						resource_barriers_.push_back(new_barrier);
					}
				}
			}
			else
			{
				auto final_state = resource_state.GetSubresourceState(transition_barrier.Subresource);
				if (transition_barrier.StateAfter != final_state)
				{
					// Push a new transition barrier with the correct before State.
					D3D12_RESOURCE_BARRIER new_barrier = barrier;
					new_barrier.Transition.StateBefore = final_state;
					resource_barriers_.push_back(new_barrier);
				}
			}
		}
		else // In this case, the resource is being used on the command list for the first time. 
		{
			// Add a pending barrier. The pending barriers will be resolved
			// before the command list is executed on the command queue.
			pending_resource_barriers_.push_back(barrier);
		}

		// Push the final known State (possibly replacing the previously known State for the subresource).
		final_resource_state_[transition_barrier.pResource].SetSubresourceState(
			transition_barrier.Subresource, transition_barrier.StateAfter);
	}
	else
	{
		// Just push non-transition barriers to the resource barriers array.
		resource_barriers_.push_back(barrier);
	}
}

void ResourceStateTracker::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_after,
                                              UINT sub_resource)
{
	if (resource)
	{
		ResourceBarrier(
			CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COMMON, state_after, sub_resource));
	}
}

void ResourceStateTracker::TransitionResource(const Resource& resource, D3D12_RESOURCE_STATES state_after,
                                              UINT sub_resource)
{
	TransitionResource(resource.GetD3D12Resource().Get(), state_after, sub_resource);
}

void ResourceStateTracker::UAVBarrier(const Resource* resource)
{
	ID3D12Resource* p_resource = resource != nullptr ? resource->GetD3D12Resource().Get() : nullptr;

	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(p_resource));
}

void ResourceStateTracker::AliasBarrier(const Resource* resource_before, const Resource* resource_after)
{
	ID3D12Resource* p_resource_before = resource_before != nullptr ? resource_before->GetD3D12Resource().Get() : nullptr;
	ID3D12Resource* p_resource_after = resource_after != nullptr ? resource_after->GetD3D12Resource().Get() : nullptr;

	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(p_resource_before, p_resource_after));
}

void ResourceStateTracker::FlushResourceBarriers(CommandList& command_list)
{
	UINT num_barriers = static_cast<UINT>(resource_barriers_.size());
	if (num_barriers > 0)
	{
		auto d3d12_command_list = command_list.GetGraphicsCommandList();
		d3d12_command_list->ResourceBarrier(num_barriers, resource_barriers_.data());
		resource_barriers_.clear();
	}
}

uint32_t ResourceStateTracker::FlushPendingResourceBarriers(CommandList& command_list)
{
	assert(is_locked_);

	// Resolve the pending resource barriers by checking the global State of the 
	// (sub)resources. Add barriers if the pending State and the global State do
	//  not match.
	ResourceBarriers resource_barriers;
	// Reserve enough space (worst-case, all pending barriers).
	resource_barriers.reserve(pending_resource_barriers_.size());

	for (auto pending_barrier : pending_resource_barriers_)
	{
		if (pending_barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
			// Only transition barriers should be pending...
		{
			auto pending_transition = pending_barrier.Transition;

			const auto& iter = global_resource_state_.find(pending_transition.pResource);
			if (iter != global_resource_state_.end())
			{
				// If all subresources are being transitioned, and there are multiple
				// subresources of the resource that are in a different State...
				auto& resource_state = iter->second;
				if (pending_transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
					!resource_state.SubresourceState.empty())
				{
					// Transition all subresources
					for (auto subresource_state : resource_state.SubresourceState)
					{
						if (pending_transition.StateAfter != subresource_state.second)
						{
							D3D12_RESOURCE_BARRIER new_barrier = pending_barrier;
							new_barrier.Transition.Subresource = subresource_state.first;
							new_barrier.Transition.StateBefore = subresource_state.second;
							resource_barriers.push_back(new_barrier);
						}
					}
				}
				else
				{
					// No (sub)resources need to be transitioned. Just add a single transition barrier (if needed).
					auto global_state = (iter->second).GetSubresourceState(pending_transition.Subresource);
					if (pending_transition.StateAfter != global_state)
					{
						// Fix-up the before State based on current global State of the resource.
						pending_barrier.Transition.StateBefore = global_state;
						resource_barriers.push_back(pending_barrier);
					}
				}
			}
		}
	}

	UINT num_barriers = static_cast<UINT>(resource_barriers.size());
	if (num_barriers > 0)
	{
		auto d3d12_command_list = command_list.GetGraphicsCommandList();
		d3d12_command_list->ResourceBarrier(num_barriers, resource_barriers.data());
	}

	pending_resource_barriers_.clear();

	return num_barriers;
}

void ResourceStateTracker::CommitFinalResourceStates()
{
	assert(is_locked_);

	// Commit final resource State to the global resource State array (map).
	for (const auto& resource_state : final_resource_state_)
	{
		global_resource_state_[resource_state.first] = resource_state.second;
	}

	final_resource_state_.clear();
}

void ResourceStateTracker::Reset()
{
	// Reset the pending, current, and final resource State.
	pending_resource_barriers_.clear();
	resource_barriers_.clear();
	final_resource_state_.clear();
}

void ResourceStateTracker::Lock()
{
	global_mutex_.lock();
	is_locked_ = true;
}

void ResourceStateTracker::Unlock()
{
	global_mutex_.unlock();
	is_locked_ = false;
}

void ResourceStateTracker::AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(global_mutex_);
		global_resource_state_[resource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
	}
}

void ResourceStateTracker::RemoveGlobalResourceState(ID3D12Resource* resource)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(global_mutex_);
		global_resource_state_.erase(resource);
	}
}
