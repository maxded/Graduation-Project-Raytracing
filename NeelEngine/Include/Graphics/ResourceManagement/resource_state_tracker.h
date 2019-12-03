#pragma once

#include <d3d12.h>

#include <mutex>
#include <map>
#include <unordered_map>
#include <vector>

class CommandList;
class Resource;

class ResourceStateTracker
{
public:
	ResourceStateTracker();
	virtual ~ResourceStateTracker();

	/**
	 * Push a resource barrier to the resource State tracker.
	 *
	 * @param barrier The resource barrier to push to the resource State tracker.
	 */
	void ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier);

	/**
	 * Push a transition resource barrier to the resource State tracker.
	 *
	 * @param resource The resource to transition.
	 * @param state_after The State to transition the resource to.
	 * @param sub_resource The subresource to transition. By default, this is D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
	 * which indicates that all subresources should be transitioned to the same State.
	 */
	void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_after,
	                        UINT sub_resource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void TransitionResource(const Resource& resource, D3D12_RESOURCE_STATES state_after,
	                        UINT sub_resource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	/**
	 * Push a UAV resource barrier for the given resource.
	 *
	 * @param resource The resource to add a UAV barrier for. Can be NULL which
	 * indicates that any UAV access could require the barrier.
	 */
	void UAVBarrier(const Resource* resource = nullptr);

	/**
	 * Push an aliasing barrier for the given resource.
	 *
	 * @param beforeResource The resource currently occupying the space in the heap.
	 * @param afterResource The resource that will be occupying the space in the heap.
	 *
	 * Either the beforeResource or the afterResource parameters can be NULL which
	 * indicates that any placed or reserved resource could cause aliasing.
	 */
	void AliasBarrier(const Resource* resource_before = nullptr, const Resource* resource_after = nullptr);

	/**
	 * Flush any pending resource barriers to the command list.
	 *
	 * @return The number of resource barriers that were flushed to the command list.
	 */
	uint32_t FlushPendingResourceBarriers(CommandList& command_list);

	/**
	 * Flush any (non-pending) resource barriers that have been pushed to the resource State
	 * tracker.
	 */
	void FlushResourceBarriers(CommandList& command_list);

	/**
	 * Commit final resource State to the global resource State map.
	 * This must be called when the command list is closed.
	 */
	void CommitFinalResourceStates();

	/**
	 * Reset State tracking. This must be done when the command list is reset.
	 */
	void Reset();

	/**
	 * The global State must be locked before flushing pending resource barriers
	 * and committing the final resource State to the global resource State.
	 * This ensures consistency of the global resource State between command list
	 * executions.
	 */
	static void Lock();

	/**
	 * Unlocks the global resource State after the final State have been committed
	 * to the global resource State array.
	 */
	static void Unlock();

	/**
	 * Add a resource with a given State to the global resource State array (map).
	 * This should be done when the resource is created for the first time.
	 */
	static void AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

	/**
	 * Remove a resource from the global resource State array (map).
	 * This should only be done when the resource is destroyed.
	 */
	static void RemoveGlobalResourceState(ID3D12Resource* resource);

protected:

private:
	// An array (vector) of resource barriers.
	using ResourceBarriers = std::vector<D3D12_RESOURCE_BARRIER>;

	// Pending resource transitions are committed before a command list
	// is executed on the command queue. This guarantees that resources will
	// be in the expected State at the beginning of a command list.
	ResourceBarriers pending_resource_barriers_;

	// Resource barriers that need to be committed to the command list.
	ResourceBarriers resource_barriers_;

	// Tracks the State of a particular resource and all of its subresources.
	struct ResourceState
	{
		// Initialize all of the subresources within a resource to the given State.
		explicit ResourceState(D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON)
			: State(state)
		{
		}

		// Set a subresource to a particular State.
		void SetSubresourceState(UINT subresource, D3D12_RESOURCE_STATES state)
		{
			if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
			{
				State = state;
				SubresourceState.clear();
			}
			else
			{
				SubresourceState[subresource] = state;
			}
		}

		// Get the State of a (sub)resource within the resource.
		// If the specified subresource is not found in the SubresourceState array (map)
		// then the State of the resource (D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) is
		// returned.
		D3D12_RESOURCE_STATES GetSubresourceState(UINT subresource) const
		{
			D3D12_RESOURCE_STATES state = State;
			const auto iter = SubresourceState.find(subresource);
			if (iter != SubresourceState.end())
			{
				state = iter->second;
			}
			return state;
		}

		// If the SubresourceState array (map) is empty, then the State variable defines 
		// the State of all of the subresources.
		D3D12_RESOURCE_STATES State;
		std::map<UINT, D3D12_RESOURCE_STATES> SubresourceState;
	};

	using ResourceStateMap = std::unordered_map<ID3D12Resource*, ResourceState>;

	// The final (last known State) of the resources within a command list.
	// The final resource State is committed to the global resource State when the 
	// command list is closed but before it is executed on the command queue.
	ResourceStateMap final_resource_state_;

	// The global resource State array (map) stores the State of a resource
	// between command list execution.
	static ResourceStateMap global_resource_state_;

	// The mutex protects shared access to the GlobalResourceState map.
	static std::mutex global_mutex_;
	static bool is_locked_;
};
