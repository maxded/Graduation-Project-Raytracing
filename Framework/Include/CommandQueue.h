#pragma once

#include <d3d12.h>  
#include <wrl.h>    

#include <cstdint>  
#include <queue>    

class CommandQueue
{
public:
    CommandQueue(D3D12_COMMAND_LIST_TYPE type);
    virtual ~CommandQueue();

    // Get an available command list from the command queue.
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> GetCommandList();

    // Execute a command list.
    // Returns the fence value to wait for for this command list.
    uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList);

    uint64_t Signal();
    bool IsFenceComplete(uint64_t fenceValue);
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;
protected:

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);

private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
        uint64_t fenceValue;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue		= std::queue< Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> >;

    D3D12_COMMAND_LIST_TYPE                     m_CommandListType;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>  m_d3d12CommandQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence>         m_d3d12Fence;
    HANDLE                                      m_FenceEvent;
    uint64_t                                    m_FenceValue;

    CommandAllocatorQueue                       m_CommandAllocatorQueue;
    CommandListQueue                            m_CommandListQueue;
};