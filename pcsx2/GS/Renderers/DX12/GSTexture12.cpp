// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/DX12/GSTexture12.h"
#include "GS/Renderers/DX12/D3D12Builders.h"
#include "GS/Renderers/DX12/GSDevice12.h"
#include "GS/GSPerfMon.h"
#include "GS/GSGL.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/BitUtils.h"
#include "common/StringUtil.h"

#include "D3D12MemAlloc.h"

static D3D12_BARRIER_LAYOUT GetD3DLayout(GSTexture12::Layout layout)
{
	static constexpr std::array<D3D12_BARRIER_LAYOUT, static_cast<u32>(GSTexture12::Layout::Count)> s_d3d12_layout_mapping = {{
		D3D12_BARRIER_LAYOUT_UNDEFINED, // Undefined
		D3D12_BARRIER_LAYOUT_RENDER_TARGET, // RenderTarget
		D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, // DepthStencilWrite
		D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ, // DepthStencilRead
		D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, // ShaderResource
		D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, // OtherShaderResource
		D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS, // UnorderedAccessView
		D3D12_BARRIER_LAYOUT_COPY_SOURCE, // CopySrc
		D3D12_BARRIER_LAYOUT_COPY_DEST, // CopyDst
		D3D12_BARRIER_LAYOUT_COMMON, // CopySelf
		D3D12_BARRIER_LAYOUT_PRESENT, // PresentSrc
		D3D12_BARRIER_LAYOUT_RENDER_TARGET, // FeedbackLoop
	}};
	return s_d3d12_layout_mapping[static_cast<u32>(layout)];
}

GSTexture12::GSTexture12(Type type, Format format, int width, int height, int levels, DXGI_FORMAT dxgi_format,
	wil::com_ptr_nothrow<ID3D12Resource> resource, wil::com_ptr_nothrow<D3D12MA::Allocation> allocation,
	const D3D12DescriptorHandle& srv_descriptor, const D3D12DescriptorHandle& write_descriptor,
	const D3D12DescriptorHandle& uav_descriptor, WriteDescriptorType wdtype, Layout layout)
	: m_resource(std::move(resource))
	, m_allocation(std::move(allocation))
	, m_srv_descriptor(srv_descriptor)
	, m_write_descriptor(write_descriptor)
	, m_uav_descriptor(uav_descriptor)
	, m_write_descriptor_type(wdtype)
	, m_dxgi_format(dxgi_format)
	, m_layout(layout)
{
	m_type = type;
	m_format = format;
	m_size.x = width;
	m_size.y = height;
	m_mipmap_levels = levels;
}

GSTexture12::~GSTexture12()
{
	Destroy(true);
}

void GSTexture12::Destroy(bool defer)
{
	GSDevice12* const dev = GSDevice12::GetInstance();
	dev->UnbindTexture(this);

	if (defer)
	{
		dev->DeferDescriptorDestruction(dev->GetDescriptorHeapManager(), &m_srv_descriptor);

		switch (m_write_descriptor_type)
		{
			case WriteDescriptorType::RTV:
				dev->DeferDescriptorDestruction(dev->GetRTVHeapManager(), &m_write_descriptor);
				break;
			case WriteDescriptorType::DSV:
				dev->DeferDescriptorDestruction(dev->GetDSVHeapManager(), &m_write_descriptor);
				break;
			case WriteDescriptorType::None:
			default:
				break;
		}

		if (m_uav_descriptor)
			dev->DeferDescriptorDestruction(dev->GetDescriptorHeapManager(), &m_uav_descriptor);

		dev->DeferResourceDestruction(m_allocation.get(), m_resource.get());
		m_resource.reset();
		m_allocation.reset();
	}
	else
	{
		dev->GetDescriptorHeapManager().Free(&m_srv_descriptor);

		switch (m_write_descriptor_type)
		{
			case WriteDescriptorType::RTV:
				dev->GetRTVHeapManager().Free(&m_write_descriptor);
				break;
			case WriteDescriptorType::DSV:
				dev->GetDSVHeapManager().Free(&m_write_descriptor);
				break;
			case WriteDescriptorType::None:
			default:
				break;
		}

		if (m_uav_descriptor)
			dev->GetDescriptorHeapManager().Free(&m_uav_descriptor);

		m_resource.reset();
		m_allocation.reset();
	}

	m_write_descriptor_type = WriteDescriptorType::None;
}

std::unique_ptr<GSTexture12> GSTexture12::Create(Type type, Format format, int width, int height, int levels,
	DXGI_FORMAT dxgi_format, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format,
	DXGI_FORMAT uav_format)
{
	GSDevice12* const dev = GSDevice12::GetInstance();

	D3D12_RESOURCE_DESC1 desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = levels;
	desc.Format = dxgi_format;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.SamplerFeedbackMipRegion = {};
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_WITHIN_BUDGET;
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE optimized_clear_value = {};
	D3D12_BARRIER_LAYOUT initial_layout;
	Layout layout;

	switch (type)
	{
		case Type::Texture:
		{
			// This is a little annoying. basically, to do mipmap generation, we need to be a render target.
			// If it's a compressed texture, we won't be generating mips anyway, so this should be fine.
			desc.Flags = (levels > 1 && !IsCompressedFormat(format)) ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET :
			                                                           D3D12_RESOURCE_FLAG_NONE;
			layout = Layout::CopyDst;
			initial_layout = D3D12_BARRIER_LAYOUT_COPY_DEST;
		}
		break;

		case Type::RenderTarget:
		{
			// RT's tend to be larger, so we'll keep them committed for speed.
			pxAssert(levels == 1);
			allocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			optimized_clear_value.Format = rtv_format;
			layout = Layout::RenderTarget;
			initial_layout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		}
		break;

		case Type::DepthStencil:
		{
			pxAssert(levels == 1);
			allocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			optimized_clear_value.Format = dsv_format;
			layout = Layout::DepthStencilWrite;
			initial_layout = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		}
		break;

		case Type::RWTexture:
		{
			pxAssert(levels == 1);
			allocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
			layout = Layout::PixelShaderResource;
			initial_layout = D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		}
		break;

		default:
			return {};
	}

	if (uav_format != DXGI_FORMAT_UNKNOWN)
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	wil::com_ptr_nothrow<ID3D12Resource> resource;
	wil::com_ptr_nothrow<D3D12MA::Allocation> allocation;
	HRESULT hr = dev->GetAllocator()->CreateResource3(&allocationDesc, &desc, initial_layout,
		(type == Type::RenderTarget || type == Type::DepthStencil) ? &optimized_clear_value : nullptr, 0, nullptr, allocation.put(),
		IID_PPV_ARGS(resource.put()));
	if (FAILED(hr))
	{
		// OOM isn't fatal.
		if (hr != E_OUTOFMEMORY)
			Console.Error("Create texture failed: 0x%08X", hr);

		return {};
	}

	D3D12DescriptorHandle srv_descriptor, write_descriptor, uav_descriptor;
	WriteDescriptorType write_descriptor_type = WriteDescriptorType::None;
	if (srv_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateSRVDescriptor(resource.get(), levels, srv_format, &srv_descriptor))
			return {};
	}

	switch (type)
	{
		case Type::RenderTarget:
		{
			write_descriptor_type = WriteDescriptorType::RTV;
			if (!CreateRTVDescriptor(resource.get(), rtv_format, &write_descriptor))
			{
				dev->GetRTVHeapManager().Free(&srv_descriptor);
				return {};
			}
		}
		break;

		case Type::DepthStencil:
		{
			write_descriptor_type = WriteDescriptorType::DSV;
			if (!CreateDSVDescriptor(resource.get(), dsv_format, &write_descriptor))
			{
				dev->GetDSVHeapManager().Free(&srv_descriptor);
				return {};
			}
		}
		break;

		default:
			break;
	}

	if (uav_format != DXGI_FORMAT_UNKNOWN && !CreateUAVDescriptor(resource.get(), dsv_format, &uav_descriptor))
	{
		dev->GetDescriptorHeapManager().Free(&write_descriptor);
		dev->GetDescriptorHeapManager().Free(&srv_descriptor);
		return {};
	}

	return std::unique_ptr<GSTexture12>(
		new GSTexture12(type, format, width, height, levels, dxgi_format, std::move(resource), std::move(allocation),
			srv_descriptor, write_descriptor, uav_descriptor, write_descriptor_type, layout));
}

std::unique_ptr<GSTexture12> GSTexture12::Adopt(wil::com_ptr_nothrow<ID3D12Resource> resource, Type type, Format format,
	int width, int height, int levels, DXGI_FORMAT dxgi_format, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format,
	DXGI_FORMAT dsv_format, DXGI_FORMAT uav_format, Layout layout)
{
	const D3D12_RESOURCE_DESC desc = resource->GetDesc();

	D3D12DescriptorHandle srv_descriptor, write_descriptor, uav_descriptor;
	WriteDescriptorType write_descriptor_type = WriteDescriptorType::None;
	if (srv_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateSRVDescriptor(resource.get(), desc.MipLevels, srv_format, &srv_descriptor))
			return {};
	}

	if (type == Type::RenderTarget)
	{
		write_descriptor_type = WriteDescriptorType::RTV;
		if (!CreateRTVDescriptor(resource.get(), rtv_format, &write_descriptor))
		{
			GSDevice12::GetInstance()->GetRTVHeapManager().Free(&srv_descriptor);
			return {};
		}
	}
	else if (type == Type::DepthStencil)
	{
		write_descriptor_type = WriteDescriptorType::DSV;
		if (!CreateDSVDescriptor(resource.get(), dsv_format, &write_descriptor))
		{
			GSDevice12::GetInstance()->GetDSVHeapManager().Free(&srv_descriptor);
			return {};
		}
	}

	if (uav_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateUAVDescriptor(resource.get(), srv_format, &uav_descriptor))
		{
			GSDevice12::GetInstance()->GetDescriptorHeapManager().Free(&write_descriptor);
			GSDevice12::GetInstance()->GetDescriptorHeapManager().Free(&srv_descriptor);
			return {};
		}
	}

	return std::unique_ptr<GSTexture12>(new GSTexture12(type, format, static_cast<u32>(desc.Width), desc.Height,
		desc.MipLevels, desc.Format, std::move(resource), {}, srv_descriptor, write_descriptor, uav_descriptor,
		write_descriptor_type, layout));
}

bool GSTexture12::CreateSRVDescriptor(
	ID3D12Resource* resource, u32 levels, DXGI_FORMAT format, D3D12DescriptorHandle* dh)
{
	if (!GSDevice12::GetInstance()->GetDescriptorHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		format, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};
	desc.Texture2D.MipLevels = levels;

	GSDevice12::GetInstance()->GetDevice()->CreateShaderResourceView(resource, &desc, dh->cpu_handle);
	return true;
}

bool GSTexture12::CreateRTVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh)
{
	if (!GSDevice12::GetInstance()->GetRTVHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	const D3D12_RENDER_TARGET_VIEW_DESC desc = {format, D3D12_RTV_DIMENSION_TEXTURE2D};
	GSDevice12::GetInstance()->GetDevice()->CreateRenderTargetView(resource, &desc, dh->cpu_handle);
	return true;
}

bool GSTexture12::CreateDSVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh)
{
	if (!GSDevice12::GetInstance()->GetDSVHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	const D3D12_DEPTH_STENCIL_VIEW_DESC desc = {format, D3D12_DSV_DIMENSION_TEXTURE2D, D3D12_DSV_FLAG_NONE};
	GSDevice12::GetInstance()->GetDevice()->CreateDepthStencilView(resource, &desc, dh->cpu_handle);
	return true;
}

bool GSTexture12::CreateUAVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh)
{
	if (!GSDevice12::GetInstance()->GetDescriptorHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate UAV descriptor");
		return false;
	}

	const D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {format, D3D12_UAV_DIMENSION_TEXTURE2D};
	GSDevice12::GetInstance()->GetDevice()->CreateUnorderedAccessView(resource, nullptr, &desc, dh->cpu_handle);
	return true;
}

void* GSTexture12::GetNativeHandle() const
{
	return const_cast<GSTexture12*>(this);
}

ID3D12GraphicsCommandList7* GSTexture12::GetCommandBufferForUpdate()
{
	GSDevice12* const dev = GSDevice12::GetInstance();
	if (m_type != Type::Texture || m_use_fence_counter == dev->GetCurrentFenceValue())
	{
		// Console.WriteLn("Texture update within frame, can't use do beforehand");
		GSDevice12::GetInstance()->EndRenderPass();
		return dev->GetCommandList();
	}

	return dev->GetInitCommandList();
}

ID3D12Resource* GSTexture12::AllocateUploadStagingBuffer(
	const void* data, u32 pitch, u32 upload_pitch, u32 height) const
{
	const u32 buffer_size = CalcUploadSize(height, upload_pitch);
	wil::com_ptr_nothrow<ID3D12Resource> resource;
	wil::com_ptr_nothrow<D3D12MA::Allocation> allocation;

	const D3D12MA::ALLOCATION_DESC allocation_desc = {D3D12MA::ALLOCATION_FLAG_NONE, D3D12_HEAP_TYPE_UPLOAD};
	const D3D12_RESOURCE_DESC resource_desc = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, buffer_size, 1, 1, 1,
		DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};
	HRESULT hr = GSDevice12::GetInstance()->GetAllocator()->CreateResource(&allocation_desc, &resource_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, allocation.put(), IID_PPV_ARGS(resource.put()));
	if (FAILED(hr))
	{
		Console.WriteLn("(AllocateUploadStagingBuffer) CreateCommittedResource() failed with %08X", hr);
		return nullptr;
	}

	void* map_ptr;
	hr = resource->Map(0, nullptr, &map_ptr);
	if (FAILED(hr))
	{
		Console.WriteLn("(AllocateUploadStagingBuffer) Map() failed with %08X", hr);
		return nullptr;
	}

	CopyTextureDataForUpload(map_ptr, data, pitch, upload_pitch, height);

	const D3D12_RANGE write_range = {0, buffer_size};
	resource->Unmap(0, &write_range);

	// Immediately queue it for freeing after the command buffer finishes, since it's only needed for the copy.
	// This adds the reference needed to keep the buffer alive.
	GSDevice12::GetInstance()->DeferResourceDestruction(allocation.get(), resource.get());
	return resource.get();
}

void GSTexture12::CopyTextureDataForUpload(void* dst, const void* src, u32 pitch, u32 upload_pitch, u32 height) const
{
	const u32 block_size = GetCompressedBlockSize();
	const u32 count = (height + (block_size - 1)) / block_size;
	StringUtil::StrideMemCpy(dst, upload_pitch, src, pitch, std::min(upload_pitch, pitch), count);
}

bool GSTexture12::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	if (layer >= m_mipmap_levels)
		return false;

	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	// Footprint and box must be block aligned for compressed textures.
	const u32 block_size = GetCompressedBlockSize();
	const u32 width = Common::AlignUpPow2(r.width(), block_size);
	const u32 height = Common::AlignUpPow2(r.height(), block_size);
	const u32 upload_pitch = Common::AlignUpPow2<u32>(pitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const u32 required_size = CalcUploadSize(r.height(), upload_pitch);

	D3D12_TEXTURE_COPY_LOCATION srcloc;
	srcloc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcloc.PlacedFootprint.Footprint.Width = width;
	srcloc.PlacedFootprint.Footprint.Height = height;
	srcloc.PlacedFootprint.Footprint.Depth = 1;
	srcloc.PlacedFootprint.Footprint.Format = m_dxgi_format;
	srcloc.PlacedFootprint.Footprint.RowPitch = upload_pitch;

	// If the texture is larger than half our streaming buffer size, use a separate buffer.
	// Otherwise allocation will either fail, or require lots of cmdbuffer submissions.
	if (required_size > (GSDevice12::GetInstance()->GetTextureStreamBuffer().GetSize() / 2))
	{
		srcloc.pResource = AllocateUploadStagingBuffer(data, pitch, upload_pitch, height);
		if (!srcloc.pResource)
			return false;

		srcloc.PlacedFootprint.Offset = 0;
	}
	else
	{
		D3D12StreamBuffer& sbuffer = GSDevice12::GetInstance()->GetTextureStreamBuffer();
		if (!sbuffer.ReserveMemory(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
		{
			GSDevice12::GetInstance()->ExecuteCommandList(
				false, "While waiting for %u bytes in texture upload buffer", required_size);
			if (!sbuffer.ReserveMemory(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
			{
				Console.Error("Failed to reserve texture upload memory (%u bytes).", required_size);
				return false;
			}
		}

		srcloc.pResource = sbuffer.GetBuffer();
		srcloc.PlacedFootprint.Offset = sbuffer.GetCurrentOffset();
		CopyTextureDataForUpload(sbuffer.GetCurrentHostPointer(), data, pitch, upload_pitch, height);
		sbuffer.CommitMemory(required_size);
	}

	ID3D12GraphicsCommandList7* cmdlist = GetCommandBufferForUpdate();
	GL_PUSH("GSTexture12::Update({%d,%d} %dx%d Lvl:%u", r.x, r.y, r.width(), r.height(), layer);

	// first time the texture is used? don't leave it undefined
	if (m_layout == Layout::Undefined)
		TransitionToLayout(cmdlist, Layout::CopyDst);
	else if (m_layout != Layout::CopyDst)
		TransitionSubresourceToLayout(cmdlist, layer, m_layout, Layout::CopyDst);

	// if we're an rt and have been cleared, and the full rect isn't being uploaded, do the clear
	if (m_type == Type::RenderTarget)
	{
		if (!r.eq(GSVector4i(0, 0, m_size.x, m_size.y)))
			CommitClear(cmdlist);
		else
			m_state = State::Dirty;
	}

	D3D12_TEXTURE_COPY_LOCATION dstloc;
	dstloc.pResource = m_resource.get();
	dstloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstloc.SubresourceIndex = layer;

	const D3D12_BOX srcbox{0u, 0u, 0u, width, height, 1u};
	cmdlist->CopyTextureRegion(&dstloc, Common::AlignDownPow2((u32)r.x, block_size),
		Common::AlignDownPow2((u32)r.y, block_size), 0, &srcloc, &srcbox);

	if (m_layout != Layout::CopyDst)
		TransitionSubresourceToLayout(cmdlist, layer, Layout::CopyDst, m_layout);

	if (m_type == Type::Texture)
		m_needs_mipmaps_generated |= (layer == 0);

	return true;
}

bool GSTexture12::Map(GSMap& m, const GSVector4i* r, int layer)
{
	if (layer >= m_mipmap_levels || IsCompressedFormat())
		return false;

	// map for writing
	m_map_area = r ? *r : GetRect();
	m_map_level = layer;
	m.pitch = Common::AlignUpPow2(CalcUploadPitch(m_map_area.width()), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	// see note in Update() for the reason why.
	const u32 required_size = CalcUploadSize(m_map_area.height(), m.pitch);
	D3D12StreamBuffer& buffer = GSDevice12::GetInstance()->GetTextureStreamBuffer();
	if (required_size >= (buffer.GetSize() / 2))
		return false;

	if (!buffer.ReserveMemory(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
	{
		GSDevice12::GetInstance()->ExecuteCommandList(
			false, "While waiting for %u bytes in texture upload buffer", required_size);
		if (!buffer.ReserveMemory(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
			pxFailRel("Failed to reserve texture upload memory");
	}

	m.bits = static_cast<u8*>(buffer.GetCurrentHostPointer());
	return true;
}

void GSTexture12::Unmap()
{
	// this can't handle blocks/compressed formats at the moment.
	pxAssert(m_map_level < m_mipmap_levels && !IsCompressedFormat());
	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	const u32 width = m_map_area.width();
	const u32 height = m_map_area.height();
	const u32 pitch = Common::AlignUpPow2(CalcUploadPitch(width), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const u32 required_size = CalcUploadSize(height, pitch);
	D3D12StreamBuffer& buffer = GSDevice12::GetInstance()->GetTextureStreamBuffer();
	const u32 buffer_offset = buffer.GetCurrentOffset();
	buffer.CommitMemory(required_size);

	ID3D12GraphicsCommandList7* cmdlist = GetCommandBufferForUpdate();
	GL_PUSH("GSTexture12::Update({%d,%d} %dx%d Lvl:%u", m_map_area.x, m_map_area.y, m_map_area.width(),
		m_map_area.height(), m_map_level);

	// first time the texture is used? don't leave it undefined
	if (m_layout == Layout::Undefined)
		TransitionToLayout(cmdlist, Layout::CopyDst);
	else if (m_layout != Layout::CopyDst)
		TransitionSubresourceToLayout(cmdlist, m_map_level, m_layout, Layout::CopyDst);

	// if we're an rt and have been cleared, and the full rect isn't being uploaded, do the clear
	if (m_type == Type::RenderTarget)
	{
		if (!m_map_area.eq(GSVector4i(0, 0, m_size.x, m_size.y)))
			CommitClear(cmdlist);
		else
			m_state = State::Dirty;
	}

	D3D12_TEXTURE_COPY_LOCATION srcloc;
	srcloc.pResource = buffer.GetBuffer();
	srcloc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcloc.PlacedFootprint.Offset = buffer_offset;
	srcloc.PlacedFootprint.Footprint.Width = width;
	srcloc.PlacedFootprint.Footprint.Height = height;
	srcloc.PlacedFootprint.Footprint.Depth = 1;
	srcloc.PlacedFootprint.Footprint.Format = m_dxgi_format;
	srcloc.PlacedFootprint.Footprint.RowPitch = pitch;

	D3D12_TEXTURE_COPY_LOCATION dstloc;
	dstloc.pResource = m_resource.get();
	dstloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstloc.SubresourceIndex = m_map_level;

	const D3D12_BOX srcbox{0u, 0u, 0u, width, height, 1};
	cmdlist->CopyTextureRegion(&dstloc, m_map_area.x, m_map_area.y, 0, &srcloc, &srcbox);

	if (m_layout != Layout::CopyDst)
		TransitionSubresourceToLayout(cmdlist, m_map_level, Layout::CopyDst, m_layout);

	if (m_type == Type::Texture)
		m_needs_mipmaps_generated |= (m_map_level == 0);
}

void GSTexture12::GenerateMipmap()
{
	pxAssert(!IsCompressedFormat(m_format));

	for (int dst_level = 1; dst_level < m_mipmap_levels; dst_level++)
	{
		const int src_level = dst_level - 1;
		const int src_width = std::max<int>(m_size.x >> src_level, 1);
		const int src_height = std::max<int>(m_size.y >> src_level, 1);
		const int dst_width = std::max<int>(m_size.x >> dst_level, 1);
		const int dst_height = std::max<int>(m_size.y >> dst_level, 1);

		GSDevice12::GetInstance()->RenderTextureMipmap(
			this, dst_level, dst_width, dst_height, src_level, src_width, src_height);
	}

	SetUseFenceCounter(GSDevice12::GetInstance()->GetCurrentFenceValue());
}

#ifdef PCSX2_DEVBUILD

void GSTexture12::SetDebugName(std::string_view name)
{
	if (name.empty())
		return;

	D3D12::SetObjectName(m_resource.get(), name);
}

#endif

void GSTexture12::TransitionToLayout(Layout layout)
{
	TransitionToLayout(GSDevice12::GetInstance()->GetCommandList(), layout);
}

void GSTexture12::TransitionToLayout(ID3D12GraphicsCommandList7* cmdlist, Layout layout)
{
	if (m_layout == layout)
		return;

	TransitionSubresourceToLayout(cmdlist, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, m_layout, layout);
	m_layout = layout;
}

void GSTexture12::TransitionSubresourceToLayout(ID3D12GraphicsCommandList7* cmdlist, int level,
	Layout before_layout, Layout after_layout) const
{
	D3D12_TEXTURE_BARRIER barrier = {
		D3D12_BARRIER_SYNC_NONE,
		D3D12_BARRIER_SYNC_NONE,
		D3D12_BARRIER_ACCESS_COMMON,
		D3D12_BARRIER_ACCESS_COMMON,
		GetD3DLayout(before_layout),
		GetD3DLayout(after_layout),
		m_resource.get(),
		{static_cast<u32>(level), 0, 0, 0, 0, 0},
		D3D12_TEXTURE_BARRIER_FLAG_NONE,
	};

	D3D12_BARRIER_SYNC syncBefore, syncAfter;

	switch (before_layout)
	{
		case Layout::Undefined:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_NONE;
			break;

		case Layout::RenderTarget:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_RENDER_TARGET;
			break;

		case Layout::DepthStencilWrite:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
			break;

		case Layout::DepthStencilRead:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
			break;

		case Layout::PixelShaderResource:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_PIXEL_SHADING;
			break;

		case Layout::OtherShaderResource:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;
			break;

		case Layout::UnorderedAccessView:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
			break;

		case Layout::CopySrc:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_COPY_SOURCE;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_COPY;
			break;

		case Layout::CopyDst:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_COPY;
			break;

		case Layout::CopySelf:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST | D3D12_BARRIER_ACCESS_COPY_SOURCE;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_COPY;
			break;

		case Layout::PresentSrc:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_COMMON; //?
			barrier.SyncBefore = D3D12_BARRIER_SYNC_ALL;
			break;

		case Layout::FeedbackLoopRT:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET; //D3D12_BARRIER_ACCESS_COMMON; // D3D12_BARRIER_ACCESS_RENDER_TARGET | D3D12_BARRIER_ACCESS_RESOLVE_DEST | D3D12_BARRIER_ACCESS_RESOLVE_SOURCE; //??
			barrier.SyncBefore = D3D12_BARRIER_SYNC_ALL; //??
			// Can't do DEPTH_STENCIL_*
			break;

		default:
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_COMMON;
			barrier.SyncBefore = D3D12_BARRIER_SYNC_ALL;
			break;
	}

	switch (after_layout)
	{
		case Layout::Undefined:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_NONE;
			break;

		case Layout::RenderTarget:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_RENDER_TARGET;
			break;

		case Layout::DepthStencilWrite:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
			break;

		case Layout::DepthStencilRead:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
			break;

		case Layout::PixelShaderResource:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_PIXEL_SHADING;
			break;

		case Layout::OtherShaderResource:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;
			break;

		case Layout::UnorderedAccessView:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
			break;

		case Layout::CopySrc:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_COPY;
			break;

		case Layout::CopyDst:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_COPY_DEST;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_COPY;
			break;

		case Layout::CopySelf:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_COPY_DEST | D3D12_BARRIER_ACCESS_COPY_SOURCE;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_COPY;
			break;

		case Layout::PresentSrc:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_COMMON; //?
			barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL; //?
			break;

		case Layout::FeedbackLoopRT:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET; //D3D12_BARRIER_ACCESS_COMMON; //D3D12_BARRIER_ACCESS_RENDER_TARGET | D3D12_BARRIER_ACCESS_RESOLVE_DEST | D3D12_BARRIER_ACCESS_RESOLVE_SOURCE; //??
			barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL; //??
			// Can't do DEPTH_STENCIL_*
			break;

		default:
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_COMMON;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL;
			break;
	}

	D3D12_BARRIER_GROUP group{D3D12_BARRIER_TYPE_TEXTURE, 1};
	group.pTextureBarriers = &barrier;

	cmdlist->Barrier(1, &group);
}

void GSTexture12::CommitClear()
{
	if (m_state != GSTexture::State::Cleared)
		return;

	GSDevice12::GetInstance()->EndRenderPass();

	CommitClear(GSDevice12::GetInstance()->GetCommandList());
}

void GSTexture12::CommitClear(ID3D12GraphicsCommandList7* cmdlist)
{
	if (IsDepthStencil())
	{
		TransitionToLayout(cmdlist, Layout::DepthStencilWrite);
		cmdlist->ClearDepthStencilView(
			GetWriteDescriptor(), D3D12_CLEAR_FLAG_DEPTH, m_clear_value.depth, 0, 0, nullptr);
	}
	else
	{
		TransitionToLayout(cmdlist, Layout::RenderTarget);
		cmdlist->ClearRenderTargetView(GetWriteDescriptor(), GSVector4::unorm8(m_clear_value.color).v, 0, nullptr);
	}

	SetState(GSTexture::State::Dirty);
}

GSDownloadTexture12::GSDownloadTexture12(u32 width, u32 height, GSTexture::Format format)
	: GSDownloadTexture(width, height, format)
{
}

GSDownloadTexture12::~GSDownloadTexture12()
{
	if (IsMapped())
		GSDownloadTexture12::Unmap();

	if (m_buffer)
		GSDevice12::GetInstance()->DeferResourceDestruction(m_allocation.get(), m_buffer.get());
}

std::unique_ptr<GSDownloadTexture12> GSDownloadTexture12::Create(u32 width, u32 height, GSTexture::Format format)
{
	const u32 buffer_size = GetBufferSize(width, height, format, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;

	const D3D12_RESOURCE_DESC resource_desc = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, buffer_size, 1, 1, 1,
		DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};

	wil::com_ptr_nothrow<D3D12MA::Allocation> allocation;
	wil::com_ptr_nothrow<ID3D12Resource> buffer;

	HRESULT hr = GSDevice12::GetInstance()->GetAllocator()->CreateResource(&allocation_desc, &resource_desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, allocation.put(), IID_PPV_ARGS(buffer.put()));
	if (FAILED(hr))
	{
		Console.Error("(GSDownloadTexture12::Create) CreateResource() failed with HRESULT %08X", hr);
		return {};
	}

	std::unique_ptr<GSDownloadTexture12> tex(new GSDownloadTexture12(width, height, format));
	tex->m_allocation = std::move(allocation);
	tex->m_buffer = std::move(buffer);
	tex->m_buffer_size = buffer_size;
	return tex;
}

void GSDownloadTexture12::CopyFromTexture(
	const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch)
{
	GSTexture12* const tex12 = static_cast<GSTexture12*>(stex);

	pxAssert(tex12->GetFormat() == m_format);
	pxAssert(drc.width() == src.width() && drc.height() == src.height());
	pxAssert(src.z <= tex12->GetWidth() && src.w <= tex12->GetHeight());
	pxAssert(static_cast<u32>(drc.z) <= m_width && static_cast<u32>(drc.w) <= m_height);
	pxAssert(src_level < static_cast<u32>(tex12->GetMipmapLevels()));
	pxAssert((drc.left == 0 && drc.top == 0) || !use_transfer_pitch);

	u32 copy_offset, copy_size, copy_rows;
	m_current_pitch = GetTransferPitch(
		use_transfer_pitch ? static_cast<u32>(drc.width()) : m_width, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	GetTransferSize(drc, &copy_offset, &copy_size, &copy_rows);

	g_perfmon.Put(GSPerfMon::Readbacks, 1);
	GSDevice12::GetInstance()->EndRenderPass();
	tex12->CommitClear();

	if (IsMapped())
		Unmap();

	ID3D12GraphicsCommandList7* cmdlist = GSDevice12::GetInstance()->GetCommandList();
	GL_INS("ReadbackTexture: {%d,%d} %ux%u", src.left, src.top, src.width(), src.height());

	D3D12_TEXTURE_COPY_LOCATION srcloc;
	srcloc.pResource = tex12->GetResource();
	srcloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcloc.SubresourceIndex = src_level;

	D3D12_TEXTURE_COPY_LOCATION dstloc;
	dstloc.pResource = m_buffer.get();
	dstloc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dstloc.PlacedFootprint.Offset = copy_offset;
	dstloc.PlacedFootprint.Footprint.Format = tex12->GetDXGIFormat();
	dstloc.PlacedFootprint.Footprint.Width = drc.width();
	dstloc.PlacedFootprint.Footprint.Height = drc.height();
	dstloc.PlacedFootprint.Footprint.Depth = 1;
	dstloc.PlacedFootprint.Footprint.RowPitch = m_current_pitch;

	const GSTexture12::Layout old_layout = tex12->GetResourceLayout();
	if (old_layout != GSTexture12::Layout::CopySrc)
		tex12->TransitionSubresourceToLayout(cmdlist, src_level, old_layout, GSTexture12::Layout::CopySrc);

	// TODO: Rules for depth buffers here?
	const D3D12_BOX srcbox{static_cast<UINT>(src.left), static_cast<UINT>(src.top), 0u, static_cast<UINT>(src.right),
		static_cast<UINT>(src.bottom), 1u};
	cmdlist->CopyTextureRegion(&dstloc, 0, 0, 0, &srcloc, &srcbox);

	if (old_layout != GSTexture12::Layout::CopySrc)
		tex12->TransitionSubresourceToLayout(cmdlist, src_level, GSTexture12::Layout::CopySrc, old_layout);

	m_copy_fence_value = GSDevice12::GetInstance()->GetCurrentFenceValue();
	m_needs_flush = true;
}

bool GSDownloadTexture12::Map(const GSVector4i& read_rc)
{
	if (IsMapped())
		return true;

	// Never populated?
	if (!m_current_pitch)
		return false;

	u32 copy_offset, copy_size, copy_rows;
	GetTransferSize(read_rc, &copy_offset, &copy_size, &copy_rows);

	const D3D12_RANGE read_range{copy_offset, copy_offset + copy_size};
	const HRESULT hr = m_buffer->Map(0, &read_range, reinterpret_cast<void**>(const_cast<u8**>(&m_map_pointer)));
	if (FAILED(hr))
	{
		Console.Error("(GSDownloadTexture12::Map) Map() failed with HRESULT %08X", hr);
		return false;
	}

	return true;
}

void GSDownloadTexture12::Unmap()
{
	if (!IsMapped())
		return;

	const D3D12_RANGE write_range = {};
	m_buffer->Unmap(0, &write_range);
	m_map_pointer = nullptr;
}

void GSDownloadTexture12::Flush()
{
	if (!m_needs_flush)
		return;

	m_needs_flush = false;

	GSDevice12* dev = GSDevice12::GetInstance();
	if (dev->GetCompletedFenceValue() >= m_copy_fence_value)
		return;

	// Need to execute command buffer.
	if (dev->GetCurrentFenceValue() == m_copy_fence_value)
		dev->ExecuteCommandListForReadback();
	else
		dev->WaitForFence(m_copy_fence_value, GSConfig.HWSpinGPUForReadbacks);
}

#ifdef PCSX2_DEVBUILD

void GSDownloadTexture12::SetDebugName(std::string_view name)
{
	if (name.empty())
		return;

	D3D12::SetObjectName(m_buffer.get(), name);
}

#endif
