#include "neel_engine_pch.h"

#include "dynamic_descriptor_heap.h"

#include "neel_engine.h"
#include "commandlist.h"
#include "root_signature.h"

DynamicDescriptorHeap::DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heap_type, uint32_t num_descriptors_per_heap)
	: descriptor_heap_type_(heap_type)
	  , num_descriptors_per_heap_(num_descriptors_per_heap)
	  , descriptor_table_bit_mask_(0)
	  , stale_descriptor_table_bit_mask_(0)
	  , current_gpu_descriptor_handle_(D3D12_DEFAULT)
	  , current_cpu_descriptor_handle_(D3D12_DEFAULT)
	  , num_free_handles_(0)
{
	descriptor_handle_increment_size_ = NeelEngine::Get().GetDescriptorHandleIncrementSize(heap_type);

	// Allocate space for staging CPU visible descriptors.
	descriptor_handle_cache_ = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(num_descriptors_per_heap_);
}

DynamicDescriptorHeap::~DynamicDescriptorHeap()
{
}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& root_signature)
{
	// If the root signature changes, all descriptors must be (re)bound to the
	// command list.
	stale_descriptor_table_bit_mask_ = 0;

	const auto& root_signature_desc = root_signature.GetRootSignatureDesc();

	// Get a bit mask that represents the root parameter indices that match the 
	// descriptor heap type for this dynamic descriptor heap.
	descriptor_table_bit_mask_ = root_signature.GetDescriptorTableBitMask(descriptor_heap_type_);
	uint32_t descriptor_table_bit_mask = descriptor_table_bit_mask_;

	uint32_t current_offset = 0;
	DWORD root_index;
	while (_BitScanForward(&root_index, descriptor_table_bit_mask) && root_index < root_signature_desc.NumParameters)
	{
		uint32_t num_descriptors = root_signature.GetNumDescriptors(root_index);

		DescriptorTableCache& descriptor_table_cache = descriptor_table_cache_[root_index];
		descriptor_table_cache.NumDescriptors = num_descriptors;
		descriptor_table_cache.BaseDescriptor = descriptor_handle_cache_.get() + current_offset;

		current_offset += num_descriptors;

		// Flip the descriptor table bit so it's not scanned again for the current index.
		descriptor_table_bit_mask ^= (1 << root_index);
	}

	// Make sure the maximum number of descriptors per descriptor heap has not been exceeded.
	assert(
		current_offset <= num_descriptors_per_heap_ &&
		"The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void DynamicDescriptorHeap::StageDescriptors(uint32_t root_parameter_index, uint32_t offset, uint32_t num_descriptors,
                                             const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor)
{
	// Cannot stage more than the maximum number of descriptors per heap.
	// Cannot stage more than MaxDescriptorTables root parameters.
	if (num_descriptors > num_descriptors_per_heap_ || root_parameter_index >= max_descriptor_tables_)
	{
		throw std::bad_alloc();
	}

	DescriptorTableCache& descriptor_table_cache = descriptor_table_cache_[root_parameter_index];

	// Check that the number of descriptors to copy does not exceed the number
	// of descriptors expected in the descriptor table.
	if ((offset + num_descriptors) > descriptor_table_cache.NumDescriptors)
	{
		throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* dst_descriptor = (descriptor_table_cache.BaseDescriptor + offset);
	for (uint32_t i = 0; i < num_descriptors; ++i)
	{
		dst_descriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptor, i, descriptor_handle_increment_size_);
	}

	// Set the root parameter index bit to make sure the descriptor table 
	// at that index is bound to the command list.
	stale_descriptor_table_bit_mask_ |= (1 << root_parameter_index);
}

uint32_t DynamicDescriptorHeap::ComputeStaleDescriptorCount() const
{
	uint32_t num_stale_descriptors = 0;
	DWORD i;
	DWORD stale_descriptors_bit_mask = stale_descriptor_table_bit_mask_;

	while (_BitScanForward(&i, stale_descriptors_bit_mask))
	{
		num_stale_descriptors += descriptor_table_cache_[i].NumDescriptors;
		stale_descriptors_bit_mask ^= (1 << i);
	}

	return num_stale_descriptors;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::RequestDescriptorHeap()
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
	if (!available_descriptor_heaps_.empty())
	{
		descriptor_heap = available_descriptor_heaps_.front();
		available_descriptor_heaps_.pop();
	}
	else
	{
		descriptor_heap = CreateDescriptorHeap();
		descriptor_heap_pool_.push(descriptor_heap);
	}

	return descriptor_heap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap() const
{
	auto device = NeelEngine::Get().GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
	descriptor_heap_desc.Type = descriptor_heap_type_;
	descriptor_heap_desc.NumDescriptors = num_descriptors_per_heap_;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
	ThrowIfFailed(device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap)));

	return descriptor_heap;
}

void DynamicDescriptorHeap::CommitStagedDescriptors(CommandList& command_list,
                                                    std::function<void(ID3D12GraphicsCommandList*, UINT,
                                                                       D3D12_GPU_DESCRIPTOR_HANDLE)> set_func)
{
	// Compute the number of descriptors that need to be copied 
	uint32_t num_descriptors_to_commit = ComputeStaleDescriptorCount();

	if (num_descriptors_to_commit > 0)
	{
		auto device = NeelEngine::Get().GetDevice();
		auto d3d12_graphics_command_list = command_list.GetGraphicsCommandList().Get();
		assert(d3d12_graphics_command_list != nullptr);

		if (!current_descriptor_heap_ || num_free_handles_ < num_descriptors_to_commit)
		{
			current_descriptor_heap_ = RequestDescriptorHeap();
			current_cpu_descriptor_handle_ = current_descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
			current_gpu_descriptor_handle_ = current_descriptor_heap_->GetGPUDescriptorHandleForHeapStart();
			num_free_handles_ = num_descriptors_per_heap_;

			command_list.SetDescriptorHeap(descriptor_heap_type_, current_descriptor_heap_.Get());

			// When updating the descriptor heap on the command list, all descriptor
			// tables must be (re)recopied to the new descriptor heap (not just
			// the stale descriptor tables).
			stale_descriptor_table_bit_mask_ = descriptor_table_bit_mask_;
		}

		DWORD root_index;
		// Scan from LSB to MSB for a bit set in staleDescriptorsBitMask
		while (_BitScanForward(&root_index, stale_descriptor_table_bit_mask_))
		{
			UINT num_src_descriptors = descriptor_table_cache_[root_index].NumDescriptors;
			D3D12_CPU_DESCRIPTOR_HANDLE* p_src_descriptor_handles = descriptor_table_cache_[root_index].BaseDescriptor;

			D3D12_CPU_DESCRIPTOR_HANDLE p_dest_descriptor_range_starts[] =
			{
				current_cpu_descriptor_handle_
			};
			UINT p_dest_descriptor_range_sizes[] =
			{
				num_src_descriptors
			};

			// Copy the staged CPU visible descriptors to the GPU visible descriptor heap.
			device->CopyDescriptors(1, p_dest_descriptor_range_starts, p_dest_descriptor_range_sizes,
			                        num_src_descriptors, p_src_descriptor_handles, nullptr, descriptor_heap_type_);

			// Set the descriptors on the command list using the passed-in setter function.
			set_func(d3d12_graphics_command_list, root_index, current_gpu_descriptor_handle_);

			// Offset current CPU and GPU descriptor handles.
			current_cpu_descriptor_handle_.Offset(num_src_descriptors, descriptor_handle_increment_size_);
			current_gpu_descriptor_handle_.Offset(num_src_descriptors, descriptor_handle_increment_size_);
			num_free_handles_ -= num_src_descriptors;

			// Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor.
			stale_descriptor_table_bit_mask_ ^= (1 << root_index);
		}
	}
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDraw(CommandList& command_list)
{
	CommitStagedDescriptors(command_list, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(CommandList& command_list)
{
	CommitStagedDescriptors(command_list, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(CommandList& comand_list,
                                                                  D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor)
{
	if (!current_descriptor_heap_ || num_free_handles_ < 1)
	{
		current_descriptor_heap_ = RequestDescriptorHeap();
		current_cpu_descriptor_handle_ = current_descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
		current_gpu_descriptor_handle_ = current_descriptor_heap_->GetGPUDescriptorHandleForHeapStart();
		num_free_handles_ = num_descriptors_per_heap_;

		comand_list.SetDescriptorHeap(descriptor_heap_type_, current_descriptor_heap_.Get());

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)recopied to the new descriptor heap (not just
		// the stale descriptor tables).
		stale_descriptor_table_bit_mask_ = descriptor_table_bit_mask_;
	}

	auto device = NeelEngine::Get().GetDevice();

	D3D12_GPU_DESCRIPTOR_HANDLE h_gpu = current_gpu_descriptor_handle_;
	device->CopyDescriptorsSimple(1, current_cpu_descriptor_handle_, cpu_descriptor, descriptor_heap_type_);

	current_cpu_descriptor_handle_.Offset(1, descriptor_handle_increment_size_);
	current_gpu_descriptor_handle_.Offset(1, descriptor_handle_increment_size_);
	num_free_handles_ -= 1;

	return h_gpu;
}

void DynamicDescriptorHeap::Reset()
{
	available_descriptor_heaps_ = descriptor_heap_pool_;
	current_descriptor_heap_.Reset();
	current_cpu_descriptor_handle_ = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	current_gpu_descriptor_handle_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	num_free_handles_ = 0;
	descriptor_table_bit_mask_ = 0;
	stale_descriptor_table_bit_mask_ = 0;

	// Reset the table cache
	for (auto& i : descriptor_table_cache_)
	{
		i.Reset();
	}
}
