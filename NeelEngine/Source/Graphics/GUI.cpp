#include "neel_engine_pch.h"

#include "GUI.h"

#include "neel_engine.h"
#include "commandqueue.h"
#include "commandlist.h"
#include "render_target.h"
#include "root_signature.h"
#include "texture.h"
#include "window.h"


#include "imgui_impl_win32.h"
#include "directxtex.h"

// Root parameters for the ImGui root signature.
enum RootParameters
{
	MatrixCB,           // cbuffer vertexBuffer : register(b0)
	FontTexture,        // Texture2D texture0 : register(t0);
	NumRootParameters
};

//--------------------------------------------------------------------------------------
// Get surface information for a particular format
//--------------------------------------------------------------------------------------
void GetSurfaceInfo(
	_In_ size_t width,
	_In_ size_t height,
	_In_ DXGI_FORMAT format,
	size_t* out_num_bytes,
	_Out_opt_ size_t* out_row_bytes,
	_Out_opt_ size_t* out_num_rows);

GUI::GUI()
	: p_imgui_ctx_(nullptr)
{}

GUI::~GUI()
{
	Destroy();
}

bool GUI::Initialize(std::shared_ptr<Window> window)
{
	window_ = window;
	p_imgui_ctx_ = ImGui::CreateContext();
	ImGui::SetCurrentContext(p_imgui_ctx_);
	if (!window_ || !ImGui_ImplWin32_Init(window_->GetWindowHandle()))
	{
		return false;
	}

	ImGuiIO& io = ImGui::GetIO();

	io.FontGlobalScale = window->GetDPIScaling();
	// Allow user UI scaling using CTRL+Mouse Wheel scrolling
	io.FontAllowUserScaling = true;

	// Build texture atlas
	unsigned char* pixel_data = nullptr;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixel_data, &width, &height);

	auto device			= NeelEngine::Get().GetDevice();
	auto command_queue	= NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto command_list	= command_queue->GetCommandList();

	auto font_texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);

	font_texture_ = std::make_unique<Texture>(font_texture_desc);
	font_texture_->SetName("ImGui Font Texture");

	size_t row_pitch, slice_pitch;
	GetSurfaceInfo(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, &slice_pitch, &row_pitch, nullptr);

	D3D12_SUBRESOURCE_DATA subresource_data;
	subresource_data.pData		= pixel_data;
	subresource_data.RowPitch	= row_pitch;
	subresource_data.SlicePitch = slice_pitch;

	command_list->CopyTextureSubresource(*font_texture_, 0, 1, &subresource_data);
	command_list->GenerateMips(*font_texture_);

	command_queue->ExecuteCommandList(command_list);

	// Create the root signature for the ImGUI shaders.
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
	{
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}
	
	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_DESCRIPTOR_RANGE1 descriptor_range1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 root_parameters[RootParameters::NumRootParameters];
	root_parameters[RootParameters::MatrixCB].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
	root_parameters[RootParameters::FontTexture].InitAsDescriptorTable(1, &descriptor_range1, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC linear_repeat_sampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT);
	linear_repeat_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	linear_repeat_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
	root_signature_description.Init_1_1(RootParameters::NumRootParameters, root_parameters, 1, &linear_repeat_sampler, root_signature_flags);

	root_signature_ = std::make_unique<RootSignature>(root_signature_description.Desc_1_1, feature_data.HighestVersion);

	const D3D12_INPUT_ELEMENT_DESC input_layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_RT_FORMAT_ARRAY rtv_formats = {};
	rtv_formats.NumRenderTargets = 1;
	rtv_formats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	D3D12_BLEND_DESC blend_desc = {};
	blend_desc.RenderTarget[0].BlendEnable = true;
	blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterizer_desc = {};
	rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizer_desc.FrontCounterClockwise = FALSE;
	rasterizer_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizer_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizer_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizer_desc.DepthClipEnable = true;
	rasterizer_desc.MultisampleEnable = FALSE;
	rasterizer_desc.AntialiasedLineEnable = FALSE;
	rasterizer_desc.ForcedSampleCount = 0;
	rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = {};
	depth_stencil_desc.DepthEnable = false;
	depth_stencil_desc.StencilEnable = false;

	// Setup the pipeline state.
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencilState;
	} pipeline_state_stream;

	// Load ImGui shaders.
	Microsoft::WRL::ComPtr<ID3DBlob> imgui_vertex_shader;
	Microsoft::WRL::ComPtr<ID3DBlob> imgui_pixel_shader;
	
	ThrowIfFailed(D3DReadFileToBlob(L"EngineShaders/ImGUI_VS.cso", &imgui_vertex_shader));
	ThrowIfFailed(D3DReadFileToBlob(L"EngineShaders/ImGUI_PS.cso", &imgui_pixel_shader));

	pipeline_state_stream.pRootSignature = root_signature_->GetRootSignature().Get();
	pipeline_state_stream.InputLayout = { input_layout, 3 };
	pipeline_state_stream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipeline_state_stream.VS = CD3DX12_SHADER_BYTECODE(imgui_vertex_shader.Get());
	pipeline_state_stream.PS = CD3DX12_SHADER_BYTECODE(imgui_pixel_shader.Get());
	pipeline_state_stream.RTVFormats = rtv_formats;
	pipeline_state_stream.SampleDesc = { 1, 0 };
	pipeline_state_stream.BlendDesc = CD3DX12_BLEND_DESC(blend_desc);
	pipeline_state_stream.RasterizerState = CD3DX12_RASTERIZER_DESC(rasterizer_desc);
	pipeline_state_stream.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(depth_stencil_desc);

	D3D12_PIPELINE_STATE_STREAM_DESC pipeline_state_stream_desc = {
		sizeof(PipelineStateStream), &pipeline_state_stream
	};
	ThrowIfFailed(device->CreatePipelineState(&pipeline_state_stream_desc, IID_PPV_ARGS(&pipeline_state_)));

	return true;
}

void GUI::NewFrame()
{
	ImGui::SetCurrentContext(p_imgui_ctx_);
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void GUI::Render(std::shared_ptr<CommandList> command_list, const RenderTarget& render_target)
{
	ImGui::SetCurrentContext(p_imgui_ctx_);
	ImGui::Render();

	ImGuiIO& io = ImGui::GetIO();
	ImDrawData* draw_data = ImGui::GetDrawData();

	// Check if there is anything to render.
	if (!draw_data || draw_data->CmdListsCount == 0)
		return;

	ImVec2 display_pos = draw_data->DisplayPos;

	command_list->SetGraphicsRootSignature(*root_signature_);
	command_list->SetPipelineState(pipeline_state_);
	command_list->SetRenderTarget(render_target);

	// Set root arguments.
	//DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixOrthographicRH( drawData->DisplaySize.x, drawData->DisplaySize.y, 0.0f, 1.0f );
	float l = draw_data->DisplayPos.x;
	float r = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float t = draw_data->DisplayPos.y;
	float b = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	float mvp[4][4] =
	{
		{ 2.0f / (r - l),   0.0f,           0.0f,       0.0f },
		{ 0.0f,         2.0f / (t - b),     0.0f,       0.0f },
		{ 0.0f,         0.0f,           0.5f,       0.0f },
		{ (r + l) / (l - r),  (t + b) / (b - t),    0.5f,       1.0f },
	};

	command_list->SetGraphics32BitConstants(RootParameters::MatrixCB, mvp);
	command_list->SetShaderResourceView(RootParameters::FontTexture, 0, *font_texture_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	D3D12_VIEWPORT viewport = {};
	viewport.Width = draw_data->DisplaySize.x;
	viewport.Height = draw_data->DisplaySize.y;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	command_list->SetViewport(viewport);
	command_list->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const DXGI_FORMAT index_format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	// It may happen that ImGui doesn't actually render anything. In this case,
	// any pending resource barriers in the command_list will not be flushed (since 
	// resource barriers are only flushed when a draw command is executed).
	// In that case, manually flushing the resource barriers will ensure that
	// they are properly flushed before exiting this function.
	command_list->FlushResourceBarriers();

	for (int i = 0; i < draw_data->CmdListsCount; ++i)
	{
		const ImDrawList* draw_list = draw_data->CmdLists[i];

		command_list->SetDynamicVertexBuffer(0, draw_list->VtxBuffer.size(), sizeof(ImDrawVert), draw_list->VtxBuffer.Data);
		command_list->SetDynamicIndexBuffer(draw_list->IdxBuffer.size(), index_format, draw_list->IdxBuffer.Data);

		int index_offset = 0;
		for (int j = 0; j < draw_list->CmdBuffer.size(); ++j)
		{
			const ImDrawCmd& im_draw_cmd = draw_list->CmdBuffer[j];
			if (im_draw_cmd.UserCallback)
			{
				im_draw_cmd.UserCallback(draw_list, &im_draw_cmd);
			}
			else
			{
				ImVec4 clip_rect = im_draw_cmd.ClipRect;
				D3D12_RECT scissor_rect;
				scissor_rect.left = static_cast<LONG>(clip_rect.x - display_pos.x);
				scissor_rect.top = static_cast<LONG>(clip_rect.y - display_pos.y);
				scissor_rect.right = static_cast<LONG>(clip_rect.z - display_pos.x);
				scissor_rect.bottom = static_cast<LONG>(clip_rect.w - display_pos.y);

				if (scissor_rect.right - scissor_rect.left > 0.0f &&
					scissor_rect.bottom - scissor_rect.top > 0.0)
				{
					command_list->SetScissorRect(scissor_rect);
					command_list->DrawIndexed(im_draw_cmd.ElemCount, 1, index_offset);
				}
			}
			index_offset += im_draw_cmd.ElemCount;
		}
	}
}

void GUI::Destroy()
{
	if (window_)
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext(p_imgui_ctx_);
		p_imgui_ctx_ = nullptr;
		window_.reset();
	}
}

void GUI::SetScaling(float scale)
{

}

//--------------------------------------------------------------------------------------
// Get surface information for a particular format
//--------------------------------------------------------------------------------------
void GetSurfaceInfo(
	_In_ size_t width,
	_In_ size_t height,
	_In_ DXGI_FORMAT format,
	size_t* out_num_bytes,
	_Out_opt_ size_t* out_row_bytes,
	_Out_opt_ size_t* out_num_rows)
{
	size_t num_bytes = 0;
	size_t row_bytes = 0;
	size_t num_rows = 0;

	bool bc = false;
	bool packed = false;
	bool planar = false;
	size_t bpe = 0;
	switch (format)
	{
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		bc = true;
		bpe = 8;
		break;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		bc = true;
		bpe = 16;
		break;

	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_YUY2:
		packed = true;
		bpe = 4;
		break;

	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		packed = true;
		bpe = 8;
		break;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
		planar = true;
		bpe = 2;
		break;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		planar = true;
		bpe = 4;
		break;
	}

	if (bc)
	{
		size_t num_blocks_wide = 0;
		if (width > 0)
		{
			num_blocks_wide = std::max<size_t>(1, (width + 3) / 4);
		}
		size_t num_blocks_high = 0;
		if (height > 0)
		{
			num_blocks_high = std::max<size_t>(1, (height + 3) / 4);
		}
		row_bytes = num_blocks_wide * bpe;
		num_rows = num_blocks_high;
		num_bytes = row_bytes * num_blocks_high;
	}
	else if (packed)
	{
		row_bytes = ((width + 1) >> 1) * bpe;
		num_rows = height;
		num_bytes = row_bytes * height;
	}
	else if (format == DXGI_FORMAT_NV11)
	{
		row_bytes = ((width + 3) >> 2) * 4;
		num_rows = height * 2; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
		num_bytes = row_bytes * num_rows;
	}
	else if (planar)
	{
		row_bytes = ((width + 1) >> 1) * bpe;
		num_bytes = (row_bytes * height) + ((row_bytes * height + 1) >> 1);
		num_rows = height + ((height + 1) >> 1);
	}
	else
	{
		size_t bpp = BitsPerPixel(format);
		row_bytes = (width * bpp + 7) / 8; // round up to nearest byte
		num_rows = height;
		num_bytes = row_bytes * height;
	}

	if (out_num_bytes)
	{
		*out_num_bytes = num_bytes;
	}
	if (out_row_bytes)
	{
		*out_row_bytes = row_bytes;
	}
	if (out_num_rows)
	{
		*out_num_rows = num_rows;
	}
}