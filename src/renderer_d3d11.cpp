#include "renderer_d3d11.h"
#include <cstring>
#include <iostream>

namespace gfx {

    const char* RendererD3D11::GetName() const {
 return "Direct3D 11"; }
    Backend RendererD3D11::GetBackend() const {
 return Backend::kD3D11; }
    bool RendererD3D11::Initialize(GLFWwindow* window) {

        window_ = window;
        hwnd_   = glfwGetWin32Window(window);

        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        width_  = static_cast<uint32_t>(fb_w);
        height_ = static_cast<uint32_t>(fb_h);

        if (!CreateDeviceAndSwapChain()) return false;
        if (!CreateBackBufferRtv())      return false;
        if (!CreateDepthStencil())       return false;
        CreateDefaultStates();

        std::cout << "[D3D11] device created, " << width_ << "x" << height_ << std::endl;
        return true;
    }
    void RendererD3D11::Shutdown() {

        programs_.clear();
        shaders_.clear();
        input_layouts_.clear();
        buffers_.clear();

        rasterizer_state_.Reset();
        depth_stencil_state_.Reset();
        blend_state_.Reset();
        rtv_.Reset();
        dsv_.Reset();
        depth_texture_.Reset();
        swap_chain_.Reset();
        context_.Reset();
        device_.Reset();
    }
    void RendererD3D11::Resize(uint32_t width, uint32_t height) {

        width_  = width;
        height_ = height;
        if (!swap_chain_) return;

        // Unbind render target before resize.
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        rtv_.Reset();
        dsv_.Reset();
        depth_texture_.Reset();

        swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        CreateBackBufferRtv();
        CreateDepthStencil();

        context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), dsv_.Get());
        current_rtv_ptrs_[0] = rtv_.Get();
        current_rtv_count_   = 1;
        current_dsv_ptr_     = dsv_.Get();
    }
    Handle RendererD3D11::CreateBuffer(BufferType type, Usage /*usage*/, size_t size, const void* data) {

        D3D11_BUFFER_DESC desc = {};
        desc.Usage     = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = static_cast<UINT>(size);
        desc.BindFlags = ToD3D11BindFlags(type);

        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem = data;

        ComPtr<ID3D11Buffer> buf;
        HRESULT hr = device_->CreateBuffer(&desc,
                                           data ? &init : nullptr,
                                           buf.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreateBuffer failed: 0x"
                      << std::hex << hr << std::endl;
            return kInvalidHandle;
        }

        Handle h = next_handle_++;
        buffers_[h].buffer = std::move(buf);
        buffers_[h].type   = type;
        buffers_[h].size   = size;
        return h;
    }
    void RendererD3D11::UpdateBuffer(Handle buffer, size_t offset, size_t size,
                       const void* data) {

        auto& b = buffers_.at(buffer);
        D3D11_BOX box = {};
        box.left   = static_cast<UINT>(offset);
        box.right  = static_cast<UINT>(offset + size);
        box.bottom = 1;
        box.back   = 1;
        context_->UpdateSubresource(b.buffer.Get(), 0, &box, data, 0, 0);
    }
    void RendererD3D11::DestroyBuffer(Handle buffer) {

        buffers_.erase(buffer);
    }
    Handle RendererD3D11::CreateTexture2D(uint32_t width, uint32_t height,
                            uint32_t mip_levels, TextureFormat format,
                            const void* data) {

        if (mip_levels == 0) mip_levels = 1;

        bool is_depth = IsDepthFormat(format);

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width          = width;
        desc.Height         = height;
        desc.MipLevels      = static_cast<UINT>(mip_levels);
        desc.ArraySize      = 1;
        desc.Format         = ToDxgiTextureFormat(format);
        desc.SampleDesc.Count = 1;
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = is_depth ? D3D11_BIND_DEPTH_STENCIL
                               : (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
        if (mip_levels > 1 && !is_depth)
            desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

        UINT row_pitch = width * BytesPerPixel(format);
        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem     = data;
        init.SysMemPitch = row_pitch;

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = device_->CreateTexture2D(
            &desc, data ? &init : nullptr, tex.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreateTexture2D failed: 0x"
                      << std::hex << hr << std::endl;
            return kInvalidHandle;
        }

        ComPtr<ID3D11ShaderResourceView> srv;
        if (!is_depth) {
            hr = device_->CreateShaderResourceView(
                tex.Get(), nullptr, srv.GetAddressOf());
            if (FAILED(hr)) {
                std::cerr << "[D3D11] CreateSRV failed" << std::endl;
                return kInvalidHandle;
            }
        }

        Handle h = next_handle_++;
        textures_[h] = {std::move(tex), std::move(srv), TextureType::k2D,
                         format, width, height, 1u, mip_levels};
        return h;
    }
    Handle RendererD3D11::CreateTextureCube(uint32_t size, uint32_t mip_levels,
                              TextureFormat format,
                              const void* data[6]) {

        if (mip_levels == 0) mip_levels = 1;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width          = size;
        desc.Height         = size;
        desc.MipLevels      = static_cast<UINT>(mip_levels);
        desc.ArraySize      = 6;   // 6 faces
        desc.Format         = ToDxgiTextureFormat(format);
        desc.SampleDesc.Count = 1;
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags      = D3D11_RESOURCE_MISC_TEXTURECUBE;
        if (mip_levels > 1)
            desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

        UINT row_pitch = size * BytesPerPixel(format);

        D3D11_SUBRESOURCE_DATA init[6] = {};
        for (int i = 0; i < 6; ++i) {
            init[i].pSysMem     = data ? data[i] : nullptr;
            init[i].SysMemPitch = row_pitch;
        }

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = device_->CreateTexture2D(
            &desc, data ? init : nullptr, tex.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreateTextureCube failed" << std::endl;
            return kInvalidHandle;
        }

        ComPtr<ID3D11ShaderResourceView> srv;
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srv_desc.TextureCube.MostDetailedMip = 0;
        srv_desc.TextureCube.MipLevels = mip_levels;
        hr = device_->CreateShaderResourceView(
            tex.Get(), &srv_desc, srv.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreateCubeSRV failed" << std::endl;
            return kInvalidHandle;
        }

        Handle h = next_handle_++;
        textures_[h] = {std::move(tex), std::move(srv), TextureType::kCube,
                         format, size, size, 6u, mip_levels};
        return h;
    }
    Handle RendererD3D11::CreateTexture2DMsaa(uint32_t width, uint32_t height,
                                uint32_t samples,
                                TextureFormat format) {

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width          = width;
        desc.Height         = height;
        desc.MipLevels      = 1;
        desc.ArraySize      = 1;
        desc.Format         = ToDxgiTextureFormat(format);
        desc.SampleDesc.Count = static_cast<UINT>(samples);
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = device_->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
        if (FAILED(hr)) return kInvalidHandle;

        ComPtr<ID3D11ShaderResourceView> srv;
        hr = device_->CreateShaderResourceView(
            tex.Get(), nullptr, srv.GetAddressOf());
        if (FAILED(hr)) return kInvalidHandle;

        Handle h = next_handle_++;
        textures_[h] = {std::move(tex), std::move(srv), TextureType::k2DMsaa,
                         format, width, height, 1u, 1u};
        return h;
    }
    void RendererD3D11::UpdateTexture(Handle texture, uint32_t mip_level,
                       uint32_t x, uint32_t y, uint32_t z,
                       uint32_t width, uint32_t height, uint32_t /*depth*/,
                       const void* data) {

        auto& t = textures_.at(texture);
        UINT subresource = D3D11CalcSubresource(static_cast<UINT>(mip_level), static_cast<UINT>(z), t.mip_levels);
        UINT row_pitch = static_cast<UINT>(width * BytesPerPixel(t.format));
        context_->UpdateSubresource(t.texture.Get(), subresource, nullptr, data, row_pitch, 0);
    }
    void RendererD3D11::GenerateMipmaps(Handle texture) {

        auto& t = textures_.at(texture);
        context_->GenerateMips(t.srv.Get());
    }
    void RendererD3D11::DestroyTexture(Handle texture) {

        textures_.erase(texture);
    }
    Handle RendererD3D11::CreateSampler(const SamplerDesc& desc) {

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter         = ToD3D11Filter(desc);
        sd.AddressU       = ToD3D11AddressMode(desc.wrap_u);
        sd.AddressV       = ToD3D11AddressMode(desc.wrap_v);
        sd.AddressW       = ToD3D11AddressMode(desc.wrap_w);
        sd.MaxAnisotropy  = std::max(1u, static_cast<UINT>(desc.max_anisotropy));
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MinLOD         = desc.min_lod;
        sd.MaxLOD         = desc.max_lod;

        for (int i = 0; i < 4; ++i)
            sd.BorderColor[i] = desc.border_color[i];

        ComPtr<ID3D11SamplerState> sampler;
        HRESULT hr = device_->CreateSamplerState(&sd, sampler.GetAddressOf());
        if (FAILED(hr)) return kInvalidHandle;

        Handle h = next_handle_++;
        samplers_[h] = std::move(sampler);
        return h;
    }
    void RendererD3D11::DestroySampler(Handle sampler) {

        samplers_.erase(sampler);
    }
    Handle RendererD3D11::CreateShader(ShaderStage stage, const char* source) {

        const char* target = ToD3D11ShaderTarget(stage);
        ComPtr<ID3DBlob> blob;
        ComPtr<ID3DBlob> errors;
        HRESULT hr = D3DCompile(source, strlen(source), nullptr,
                                nullptr, nullptr, "main", target,
                                0, 0,
                                blob.GetAddressOf(),
                                errors.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] shader compile error (" << target << "):" << std::endl;
            if (errors)
                std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << std::endl;
            return kInvalidHandle;
        }

        Handle h = next_handle_++;
        shaders_[h].blob  = std::move(blob);
        shaders_[h].stage = stage;
        return h;
    }
    Handle RendererD3D11::CreateProgram(Handle vs, Handle fs, Handle /*gs*/, Handle /*tcs*/, Handle /*tes*/) {

        auto& vs_data = shaders_.at(vs);
        auto& fs_data = shaders_.at(fs);

        ProgramData prog;

        HRESULT hr = device_->CreateVertexShader(
            vs_data.blob->GetBufferPointer(),
            vs_data.blob->GetBufferSize(),
            nullptr, prog.vs.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreateVertexShader failed" << std::endl;
            return kInvalidHandle;
        }

        hr = device_->CreatePixelShader(
            fs_data.blob->GetBufferPointer(),
            fs_data.blob->GetBufferSize(),
            nullptr, prog.ps.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreatePixelShader failed" << std::endl;
            return kInvalidHandle;
        }

        // Retain VS bytecode — CreateInputLayout needs it.
        prog.vs_blob = vs_data.blob;

        Handle h = next_handle_++;
        programs_[h] = std::move(prog);
        return h;
    }
    Handle RendererD3D11::CreateComputeProgram(Handle) {
 return kInvalidHandle; }
    int32_t RendererD3D11::GetUniformLocation(Handle program, const char* name) {

        auto& prog = programs_.at(program);
        auto it = prog.uniform_locations.find(name);
        if (it != prog.uniform_locations.end())
            return it->second;

        // Assign a new slot: 64-byte aligned (mat4 = 64 bytes).
        // First uniform at offset 0, second at 64, etc.
        int32_t slot = static_cast<int32_t>(prog.uniform_locations.size());
        int32_t offset = slot * 64;
        if (static_cast<size_t>(offset + 64) > kMaxConstantBufferSize) {
            std::cerr << "[D3D11] uniform buffer overflow for '" << name
                      << "'" << std::endl;
            return -1;
        }
        prog.uniform_locations[name] = offset;

        // Lazy-create the constant buffer on first uniform.
        if (!prog.cb) {
            D3D11_BUFFER_DESC desc = {};
            desc.Usage          = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth      = static_cast<UINT>(kMaxConstantBufferSize);
            desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            HRESULT hr = device_->CreateBuffer(&desc, nullptr, prog.cb.GetAddressOf());
            if (FAILED(hr)) return -1;
        }

        return offset;
    }
    void RendererD3D11::DestroyShader(Handle shader) {

        shaders_.erase(shader);
    }
    void RendererD3D11::DestroyProgram(Handle program) {

        programs_.erase(program);
    }
    Handle RendererD3D11::CreateInputLayout(Handle program, uint32_t /*stride*/, const VertexAttrib* attribs, uint32_t count) {

        auto& prog = programs_.at(program);
        ID3DBlob* vs_blob = prog.vs_blob.Get();
        if (!vs_blob) return kInvalidHandle;

        std::vector<D3D11_INPUT_ELEMENT_DESC> descs(count);
        for (uint32_t i = 0; i < count; ++i) {
            descs[i].SemanticName         = attribs[i].semantic;
            descs[i].SemanticIndex        = 0;   // D3D11 matches by name, not index
            descs[i].Format               = ToDxgiFormat(attribs[i].format);
            descs[i].InputSlot            = 0;
            descs[i].AlignedByteOffset    = attribs[i].offset;
            descs[i].InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
            descs[i].InstanceDataStepRate = 0;
        }

        ComPtr<ID3D11InputLayout> layout;
        HRESULT hr = device_->CreateInputLayout(descs.data(), static_cast<UINT>(descs.size()),
                                                vs_blob->GetBufferPointer(),
                                                vs_blob->GetBufferSize(),
                                                layout.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreateInputLayout failed: 0x" << std::hex << hr << std::endl;
            return kInvalidHandle;
        }

        Handle h  = next_handle_++;
        auto&  il = input_layouts_[h];
        il.layout = std::move(layout);
        return h;
    }
    void RendererD3D11::DestroyInputLayout(Handle layout) {

        input_layouts_.erase(layout);
    }
    Handle RendererD3D11::CreateFramebuffer(uint32_t num_color,
                             const FramebufferAttachment* color,
                             const FramebufferAttachment* depth_stencil) {

        FramebufferData fb;

        for (uint32_t i = 0; i < num_color; ++i) {
            auto& td = textures_.at(color[i].texture);
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
            rtv_desc.Format = ToDxgiTextureFormat(td.format);
            if (td.type == TextureType::kCube) {
                rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtv_desc.Texture2DArray.MipSlice = color[i].mip_level;
                rtv_desc.Texture2DArray.FirstArraySlice =
                    static_cast<UINT>(color[i].cube_face);
                rtv_desc.Texture2DArray.ArraySize = 1;
            } else {
                rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                rtv_desc.Texture2D.MipSlice = color[i].mip_level;
            }

            ComPtr<ID3D11RenderTargetView> rtv;
            HRESULT hr = device_->CreateRenderTargetView(td.texture.Get(), &rtv_desc, rtv.GetAddressOf());
            if (FAILED(hr)) {
                std::cerr << "[D3D11] CreateRTV failed" << std::endl;
                return kInvalidHandle;
            }
            fb.color_rtvs.push_back(std::move(rtv));
        }

        if (depth_stencil && depth_stencil->texture != kInvalidHandle) {
            auto& td = textures_.at(depth_stencil->texture);
            D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
            dsv_desc.Format = ToDxgiTextureFormat(td.format);
            dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsv_desc.Texture2D.MipSlice = depth_stencil->mip_level;

            HRESULT hr = device_->CreateDepthStencilView(
                td.texture.Get(), &dsv_desc, fb.ds_view.GetAddressOf());
            if (FAILED(hr)) {
                std::cerr << "[D3D11] CreateDSV failed" << std::endl;
                return kInvalidHandle;
            }
        }

        Handle h = next_handle_++;
        framebuffers_[h] = std::move(fb);
        return h;
    }
    void RendererD3D11::DestroyFramebuffer(Handle framebuffer) {

        framebuffers_.erase(framebuffer);
    }
    void RendererD3D11::SetRasterizerState(const RasterizerDesc& desc) {

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = (desc.fill_mode == FillMode::kSolid)
                           ? D3D11_FILL_SOLID : D3D11_FILL_WIREFRAME;
        rd.CullMode              = ToD3D11CullMode(desc.cull_mode);
        rd.FrontCounterClockwise = desc.front_ccw ? TRUE : FALSE;
        rd.DepthClipEnable       = desc.depth_clip ? TRUE : FALSE;
        rd.ScissorEnable         = desc.scissor_enabled ? TRUE : FALSE;
        rd.DepthBias             = desc.depth_bias;
        rd.SlopeScaledDepthBias  = desc.depth_bias_slope;
        rd.DepthBiasClamp        = desc.depth_bias_clamp;

        device_->CreateRasterizerState(&rd,
                                        rasterizer_state_.ReleaseAndGetAddressOf());
        context_->RSSetState(rasterizer_state_.Get());
    }
    void RendererD3D11::SetDepthStencilState(const DepthStencilDesc& desc) {

        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable    = desc.depth_test ? TRUE : FALSE;
        dsd.DepthWriteMask = desc.depth_write
                                 ? D3D11_DEPTH_WRITE_MASK_ALL
                                 : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc      = ToD3D11Comparison(desc.depth_func);
        dsd.StencilEnable  = desc.stencil_test ? TRUE : FALSE;
        dsd.StencilReadMask  = desc.stencil_read_mask;
        dsd.StencilWriteMask = desc.stencil_write_mask;

        auto FillFace = [](D3D11_DEPTH_STENCILOP_DESC& out,
                            const DepthStencilDesc::StencilFace& in) {
            out.StencilFailOp      = ToD3D11StencilOp(in.fail_op);
            out.StencilDepthFailOp = ToD3D11StencilOp(in.depth_fail_op);
            out.StencilPassOp      = ToD3D11StencilOp(in.pass_op);
            out.StencilFunc        = ToD3D11Comparison(in.func);
        };
        FillFace(dsd.FrontFace, desc.front);
        FillFace(dsd.BackFace,  desc.back);

        device_->CreateDepthStencilState(
            &dsd, depth_stencil_state_.ReleaseAndGetAddressOf());
        context_->OMSetDepthStencilState(depth_stencil_state_.Get(),
                                          desc.stencil_ref);
    }
    void RendererD3D11::SetBlendState(const BlendDesc& desc) {

        D3D11_BLEND_DESC bd = {};
        bd.AlphaToCoverageEnable  = FALSE;
        bd.IndependentBlendEnable = FALSE;
        bd.RenderTarget[0].BlendEnable   = desc.enabled ? TRUE : FALSE;
        bd.RenderTarget[0].SrcBlend       = ToD3D11Blend(desc.src_color);
        bd.RenderTarget[0].DestBlend      = ToD3D11Blend(desc.dst_color);
        bd.RenderTarget[0].BlendOp        = ToD3D11BlendOp(desc.color_op);
        bd.RenderTarget[0].SrcBlendAlpha  = ToD3D11Blend(desc.src_alpha);
        bd.RenderTarget[0].DestBlendAlpha = ToD3D11Blend(desc.dst_alpha);
        bd.RenderTarget[0].BlendOpAlpha   = ToD3D11BlendOp(desc.alpha_op);
        bd.RenderTarget[0].RenderTargetWriteMask = desc.write_mask;

        device_->CreateBlendState(&bd,
                                   blend_state_.ReleaseAndGetAddressOf());
        context_->OMSetBlendState(blend_state_.Get(), nullptr, 0xFFFFFFFF);
    }
    void RendererD3D11::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = static_cast<float>(x);
        vp.TopLeftY = static_cast<float>(y);
        vp.Width    = static_cast<float>(width);
        vp.Height   = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &vp);
    }
    void RendererD3D11::SetScissor(uint32_t x, uint32_t y,
                     uint32_t width, uint32_t height) {

        D3D11_RECT rc;
        rc.left   = static_cast<LONG>(x);
        rc.top    = static_cast<LONG>(y);
        rc.right  = static_cast<LONG>(x + width);
        rc.bottom = static_cast<LONG>(y + height);
        context_->RSSetScissorRects(1, &rc);
    }
    void RendererD3D11::SetPrimitiveType(PrimitiveType type) {

        context_->IASetPrimitiveTopology(ToD3D11Topology(type));
    }
    void RendererD3D11::SetVertexLayout(const VertexAttrib*, uint32_t, uint32_t) {

        // D3D11 input layouts are immutable — use CreateInputLayout instead.
    }
    void RendererD3D11::UploadConstantBuffer() {

        if (!current_program_) return;
        auto& prog = programs_.at(current_program_);
        if (!prog.cb || !prog.cb_dirty) {
            // Still bind even if not dirty — D3D11 needs it bound every frame.
            if (prog.cb)
                context_->VSSetConstantBuffers(0, 1, prog.cb.GetAddressOf());
            return;
        }
        D3D11_MAPPED_SUBRESOURCE mr = {};
        if (SUCCEEDED(context_->Map(prog.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                                     0, &mr))) {
            std::memcpy(mr.pData, prog.cb_shadow, kMaxConstantBufferSize);
            context_->Unmap(prog.cb.Get(), 0);
        }
        context_->VSSetConstantBuffers(0, 1, prog.cb.GetAddressOf());
        prog.cb_dirty = false;
    }
    void RendererD3D11::BindProgram(Handle program) {

        auto& prog = programs_.at(program);
        context_->VSSetShader(prog.vs.Get(), nullptr, 0);
        context_->PSSetShader(prog.ps.Get(), nullptr, 0);
        current_program_ = program;
    }
    void RendererD3D11::BindInputLayout(Handle layout) {

        auto& il = input_layouts_.at(layout);
        context_->IASetInputLayout(il.layout.Get());
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
    void RendererD3D11::BindVertexBuffer(uint32_t slot, Handle buffer,
                           uint32_t stride, uint32_t offset) {

        auto& b = buffers_.at(buffer);
        ID3D11Buffer* buf = b.buffer.Get();
        UINT s = static_cast<UINT>(stride);
        UINT o = static_cast<UINT>(offset);
        context_->IASetVertexBuffers(slot, 1, &buf, &s, &o);
    }
    void RendererD3D11::BindIndexBuffer(Handle buffer, IndexFormat format) {

        auto& b = buffers_.at(buffer);
        context_->IASetIndexBuffer(b.buffer.Get(),
                                    ToDxgiIndexFormat(format), 0);
    }
    void RendererD3D11::BindTexture(uint32_t slot, Handle texture) {

        auto& t = textures_.at(texture);
        context_->PSSetShaderResources(slot, 1, t.srv.GetAddressOf());
    }
    void RendererD3D11::BindSampler(uint32_t slot, Handle sampler) {

        context_->PSSetSamplers(slot, 1, samplers_.at(sampler).GetAddressOf());
    }
    void RendererD3D11::BindUniformBuffer(uint32_t, Handle, size_t, size_t) {
}
    void RendererD3D11::BindStorageBuffer(uint32_t, Handle) {
}
    void RendererD3D11::BindFramebuffer(Handle framebuffer) {

        auto& fb = framebuffers_.at(framebuffer);
        current_rtv_count_ = static_cast<UINT>(fb.color_rtvs.size());
        for (UINT i = 0; i < current_rtv_count_; ++i)
            current_rtv_ptrs_[i] = fb.color_rtvs[i].Get();
        current_dsv_ptr_ = fb.ds_view.Get();
        context_->OMSetRenderTargets(current_rtv_count_,
                                      current_rtv_ptrs_, current_dsv_ptr_);
    }
    void RendererD3D11::BindDefaultFramebuffer() {

        current_rtv_ptrs_[0] = rtv_.Get();
        current_rtv_count_   = 1;
        current_dsv_ptr_     = dsv_.Get();
        context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), dsv_.Get());
    }
    void RendererD3D11::SetUniformInt(int32_t location, int32_t value) {

        // location = byte offset in constant buffer
        if (location < 0 || !current_program_) return;
        auto& prog = programs_.at(current_program_);
        std::memcpy(&prog.cb_shadow[location], &value, sizeof(int32_t));
        prog.cb_dirty = true;
    }
    void RendererD3D11::SetUniformFloat(int32_t location, float value) {

        if (location < 0 || !current_program_) return;
        auto& prog = programs_.at(current_program_);
        std::memcpy(&prog.cb_shadow[location], &value, sizeof(float));
        prog.cb_dirty = true;
    }
    void RendererD3D11::SetUniformVec2(int32_t location, const float* value) {

        if (location < 0 || !current_program_) return;
        auto& prog = programs_.at(current_program_);
        std::memcpy(&prog.cb_shadow[location], value, 2 * sizeof(float));
        prog.cb_dirty = true;
    }
    void RendererD3D11::SetUniformVec3(int32_t location, const float* value) {

        if (location < 0 || !current_program_) return;
        auto& prog = programs_.at(current_program_);
        std::memcpy(&prog.cb_shadow[location], value, 3 * sizeof(float));
        prog.cb_dirty = true;
    }
    void RendererD3D11::SetUniformVec4(int32_t location, const float* value) {

        if (location < 0 || !current_program_) return;
        auto& prog = programs_.at(current_program_);
        std::memcpy(&prog.cb_shadow[location], value, 4 * sizeof(float));
        prog.cb_dirty = true;
    }
    void RendererD3D11::SetUniformMat3(int32_t location, const float* value) {

        // mat3 in HLSL cbuffer is padded to 3 float4 rows (48 bytes)
        if (location < 0 || !current_program_) return;
        auto& prog = programs_.at(current_program_);
        float* dst = reinterpret_cast<float*>(&prog.cb_shadow[location]);
        for (int col = 0; col < 3; ++col) {
            std::memcpy(dst + col * 4, value + col * 3, 3 * sizeof(float));
            dst[col * 4 + 3] = 0.0f;
        }
        prog.cb_dirty = true;
    }
    void RendererD3D11::SetUniformMat4(int32_t location, const float* value) {

        if (location < 0 || !current_program_) return;
        auto& prog = programs_.at(current_program_);
        std::memcpy(&prog.cb_shadow[location], value, 16 * sizeof(float));
        prog.cb_dirty = true;
    }
    void RendererD3D11::Clear(ClearFlags flags,
                float r, float g, float b, float a,
                float depth, uint8_t stencil) {

        if (flags & ClearFlags::kColor) {
            const float color[4] = {r, g, b, a};
            for (UINT i = 0; i < current_rtv_count_; ++i)
                context_->ClearRenderTargetView(current_rtv_ptrs_[i], color);
        }
        if (flags & (ClearFlags::kDepth | ClearFlags::kStencil)) {
            UINT d3d_flags = 0;
            if (flags & ClearFlags::kDepth)   d3d_flags |= D3D11_CLEAR_DEPTH;
            if (flags & ClearFlags::kStencil) d3d_flags |= D3D11_CLEAR_STENCIL;
            if (current_dsv_ptr_)
                context_->ClearDepthStencilView(current_dsv_ptr_, d3d_flags,
                                                 depth, stencil);
        }
    }
    void RendererD3D11::Draw(uint32_t vertex_count, uint32_t first_vertex) {

        UploadConstantBuffer();
        context_->Draw(static_cast<UINT>(vertex_count),
                       static_cast<UINT>(first_vertex));
    }
    void RendererD3D11::DrawIndexed(uint32_t index_count, uint32_t first_index,
                      int32_t /*vertex_offset*/) {

        UploadConstantBuffer();
        context_->DrawIndexed(static_cast<UINT>(index_count),
                               static_cast<UINT>(first_index), 0);
    }
    void RendererD3D11::DrawInstanced(uint32_t vertex_count_per_instance,
                        uint32_t instance_count,
                        uint32_t first_vertex,
                        uint32_t first_instance) {

        UploadConstantBuffer();
        context_->DrawInstanced(static_cast<UINT>(vertex_count_per_instance),
                                 static_cast<UINT>(instance_count),
                                 static_cast<UINT>(first_vertex),
                                 static_cast<UINT>(first_instance));
    }
    void RendererD3D11::DrawIndexedInstanced(uint32_t index_count_per_instance,
                               uint32_t instance_count,
                               uint32_t first_index,
                               int32_t /*vertex_offset*/,
                               uint32_t first_instance) {

        UploadConstantBuffer();
        context_->DrawIndexedInstanced(
            static_cast<UINT>(index_count_per_instance),
            static_cast<UINT>(instance_count),
            static_cast<UINT>(first_index), 0,
            static_cast<UINT>(first_instance));
    }
    void RendererD3D11::BindImageTexture(uint32_t, Handle, uint32_t, ImageAccess) {
}
    void RendererD3D11::Dispatch(uint32_t, uint32_t, uint32_t) {
}
    void RendererD3D11::MemoryBarrier() {
}
    void RendererD3D11::PushDebugGroup(const char*) {
}
    void RendererD3D11::PopDebugGroup() {
}
    void RendererD3D11::Present() {

        swap_chain_->Present(1, 0);  // vsync on
    }
    ID3D11Device* RendererD3D11::GetDevice()  const {
 return device_.Get(); }
    ID3D11DeviceContext* RendererD3D11::GetContext() const {
 return context_.Get(); }
    const char* RendererD3D11::ToD3D11ShaderTarget(ShaderStage s) {

        switch (s) {
        case ShaderStage::kVertex:      return "vs_5_0";
        case ShaderStage::kFragment:     return "ps_5_0";
        case ShaderStage::kGeometry:     return "gs_5_0";
        case ShaderStage::kCompute:      return "cs_5_0";
        case ShaderStage::kTessControl:  return "hs_5_0";
        case ShaderStage::kTessEval:     return "ds_5_0";
        }
        return "vs_5_0";
    }
    UINT RendererD3D11::ToD3D11BindFlags(BufferType t) {

        switch (t) {
        case BufferType::kVertex:  return D3D11_BIND_VERTEX_BUFFER;
        case BufferType::kIndex:   return D3D11_BIND_INDEX_BUFFER;
        case BufferType::kUniform: return D3D11_BIND_CONSTANT_BUFFER;
        case BufferType::kStorage: return D3D11_BIND_UNORDERED_ACCESS;
        }
        return 0;
    }
    DXGI_FORMAT RendererD3D11::ToDxgiFormat(AttribFormat f) {

        switch (f) {
        case AttribFormat::kFloat:      return DXGI_FORMAT_R32_FLOAT;
        case AttribFormat::kFloat2:     return DXGI_FORMAT_R32G32_FLOAT;
        case AttribFormat::kFloat3:     return DXGI_FORMAT_R32G32B32_FLOAT;
        case AttribFormat::kFloat4:     return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case AttribFormat::kInt:        return DXGI_FORMAT_R32_SINT;
        case AttribFormat::kInt2:       return DXGI_FORMAT_R32G32_SINT;
        case AttribFormat::kInt3:       return DXGI_FORMAT_R32G32B32_SINT;
        case AttribFormat::kInt4:       return DXGI_FORMAT_R32G32B32A32_SINT;
        case AttribFormat::kUbyte4Norm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        return DXGI_FORMAT_R32G32B32_FLOAT;
    }
    DXGI_FORMAT RendererD3D11::ToDxgiIndexFormat(IndexFormat f) {

        return f == IndexFormat::kUint32
                   ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    }
    D3D11_CULL_MODE RendererD3D11::ToD3D11CullMode(CullMode m) {

        switch (m) {
        case CullMode::kNone:  return D3D11_CULL_NONE;
        case CullMode::kFront: return D3D11_CULL_FRONT;
        case CullMode::kBack:  return D3D11_CULL_BACK;
        }
        return D3D11_CULL_NONE;
    }
    D3D11_COMPARISON_FUNC RendererD3D11::ToD3D11Comparison(CompareFunc f) {

        switch (f) {
        case CompareFunc::kNever:    return D3D11_COMPARISON_NEVER;
        case CompareFunc::kLess:     return D3D11_COMPARISON_LESS;
        case CompareFunc::kEqual:    return D3D11_COMPARISON_EQUAL;
        case CompareFunc::kLequal:   return D3D11_COMPARISON_LESS_EQUAL;
        case CompareFunc::kGreater:  return D3D11_COMPARISON_GREATER;
        case CompareFunc::kNotequal: return D3D11_COMPARISON_NOT_EQUAL;
        case CompareFunc::kGequal:   return D3D11_COMPARISON_GREATER_EQUAL;
        case CompareFunc::kAlways:   return D3D11_COMPARISON_ALWAYS;
        }
        return D3D11_COMPARISON_LESS;
    }
    D3D11_STENCIL_OP RendererD3D11::ToD3D11StencilOp(StencilOp op) {

        switch (op) {
        case StencilOp::kKeep:       return D3D11_STENCIL_OP_KEEP;
        case StencilOp::kZero:       return D3D11_STENCIL_OP_ZERO;
        case StencilOp::kReplace:    return D3D11_STENCIL_OP_REPLACE;
        case StencilOp::kIncrClamp:  return D3D11_STENCIL_OP_INCR_SAT;
        case StencilOp::kDecrClamp:  return D3D11_STENCIL_OP_DECR_SAT;
        case StencilOp::kInvert:     return D3D11_STENCIL_OP_INVERT;
        case StencilOp::kIncrWrap:   return D3D11_STENCIL_OP_INCR;
        case StencilOp::kDecrWrap:   return D3D11_STENCIL_OP_DECR;
        }
        return D3D11_STENCIL_OP_KEEP;
    }
    D3D11_BLEND RendererD3D11::ToD3D11Blend(BlendFactor f) {

        switch (f) {
        case BlendFactor::kZero:         return D3D11_BLEND_ZERO;
        case BlendFactor::kOne:          return D3D11_BLEND_ONE;
        case BlendFactor::kSrcColor:     return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::kInvSrcColor:  return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::kSrcAlpha:     return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::kInvSrcAlpha:  return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::kDstColor:     return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::kInvDstColor:  return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::kDstAlpha:     return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::kInvDstAlpha:  return D3D11_BLEND_INV_DEST_ALPHA;
        }
        return D3D11_BLEND_ONE;
    }
    D3D11_BLEND_OP RendererD3D11::ToD3D11BlendOp(BlendOp op) {

        switch (op) {
        case BlendOp::kAdd:    return D3D11_BLEND_OP_ADD;
        case BlendOp::kSub:    return D3D11_BLEND_OP_SUBTRACT;
        case BlendOp::kRevSub: return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendOp::kMin:    return D3D11_BLEND_OP_MIN;
        case BlendOp::kMax:    return D3D11_BLEND_OP_MAX;
        }
        return D3D11_BLEND_OP_ADD;
    }
    D3D11_PRIMITIVE_TOPOLOGY RendererD3D11::ToD3D11Topology(PrimitiveType t) {

        switch (t) {
        case PrimitiveType::kTriangles:      return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveType::kTriangleStrip:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveType::kPoints:         return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveType::kLines:          return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        }
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
    DXGI_FORMAT RendererD3D11::ToDxgiTextureFormat(TextureFormat f) {

        switch (f) {
        case TextureFormat::kR8Unorm:          return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::kRg8Unorm:         return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::kRgba8Unorm:       return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::kRgba8Srgb:        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case TextureFormat::kR16F:             return DXGI_FORMAT_R16_FLOAT;
        case TextureFormat::kRg16F:            return DXGI_FORMAT_R16G16_FLOAT;
        case TextureFormat::kRgba16F:          return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::kR32F:             return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::kRg32F:            return DXGI_FORMAT_R32G32_FLOAT;
        case TextureFormat::kRgba32F:          return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case TextureFormat::kR11G11B10F:       return DXGI_FORMAT_R11G11B10_FLOAT;
        case TextureFormat::kDepth16:          return DXGI_FORMAT_D16_UNORM;
        case TextureFormat::kDepth24Stencil8:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::kDepth32F:         return DXGI_FORMAT_D32_FLOAT;
        }
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    UINT RendererD3D11::BytesPerPixel(TextureFormat f) {

        switch (f) {
        case TextureFormat::kR8Unorm:          return 1;
        case TextureFormat::kRg8Unorm:         return 2;
        case TextureFormat::kRgba8Unorm:
        case TextureFormat::kRgba8Srgb:        return 4;
        case TextureFormat::kR16F:             return 2;
        case TextureFormat::kRg16F:            return 4;
        case TextureFormat::kRgba16F:          return 8;
        case TextureFormat::kR32F:             return 4;
        case TextureFormat::kRg32F:            return 8;
        case TextureFormat::kRgba32F:          return 16;
        case TextureFormat::kR11G11B10F:       return 4;
        case TextureFormat::kDepth16:          return 2;
        case TextureFormat::kDepth24Stencil8:  return 4;
        case TextureFormat::kDepth32F:         return 4;
        default: return 4;
        }
    }
    bool RendererD3D11::IsDepthFormat(TextureFormat f) {

        return f == TextureFormat::kDepth16
            || f == TextureFormat::kDepth24Stencil8
            || f == TextureFormat::kDepth32F;
    }
    D3D11_FILTER RendererD3D11::ToD3D11Filter(const SamplerDesc& d) {

        bool min_p = (d.min_filter == FilterMode::kPoint ||
                       d.min_filter == FilterMode::kAnisotropic);
        bool mag_p = (d.mag_filter == FilterMode::kPoint);
        bool mip_p = (d.mip_filter == FilterMode::kPoint);
        bool aniso = (d.min_filter == FilterMode::kAnisotropic);

        if (aniso)       return D3D11_FILTER_ANISOTROPIC;
        if (min_p) {
            if (mag_p)   return mip_p ? D3D11_FILTER_MIN_MAG_MIP_POINT
                                       : D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            else         return mip_p ? D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT
                                       : D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        } else {
            if (mag_p)   return mip_p ? D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT
                                       : D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
            else         return mip_p ? D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
                                       : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        }
    }
    D3D11_TEXTURE_ADDRESS_MODE RendererD3D11::ToD3D11AddressMode(WrapMode w) {

        switch (w) {
        case WrapMode::kRepeat:        return D3D11_TEXTURE_ADDRESS_WRAP;
        case WrapMode::kClampToEdge:   return D3D11_TEXTURE_ADDRESS_CLAMP;
        case WrapMode::kClampToBorder: return D3D11_TEXTURE_ADDRESS_BORDER;
        case WrapMode::kMirrorRepeat:  return D3D11_TEXTURE_ADDRESS_MIRROR;
        }
        return D3D11_TEXTURE_ADDRESS_WRAP;
    }
    bool RendererD3D11::CreateDeviceAndSwapChain() {

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferDesc.Width  = width_;
        sd.BufferDesc.Height = height_;
        sd.BufferDesc.Format = kBackBufferFormat;
        sd.SampleDesc.Count  = 1;   // no MSAA
        sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount       = 2;
        sd.OutputWindow      = hwnd_;
        sd.Windowed          = TRUE;
        sd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL kLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };
        D3D_FEATURE_LEVEL chosen{};

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            kLevels, static_cast<UINT>(ARRAYSIZE(kLevels)),
            D3D11_SDK_VERSION, &sd,
            swap_chain_.GetAddressOf(), device_.GetAddressOf(),
            &chosen, context_.GetAddressOf());

        if (FAILED(hr)) {
#ifdef _DEBUG
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                kLevels, static_cast<UINT>(ARRAYSIZE(kLevels)),
                D3D11_SDK_VERSION, &sd,
                swap_chain_.GetAddressOf(), device_.GetAddressOf(),
                &chosen, context_.GetAddressOf());
#endif
        }

        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreateDeviceAndSwapChain failed: 0x"
                      << std::hex << hr << std::endl;
            return false;
        }
        return true;
    }
    bool RendererD3D11::CreateBackBufferRtv() {

        ComPtr<ID3D11Texture2D> back_buffer;
        HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
        if (FAILED(hr)) {
            std::cerr << "[D3D11] GetBuffer failed" << std::endl;
            return false;
        }
        hr = device_->CreateRenderTargetView(back_buffer.Get(), nullptr, rtv_.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[D3D11] CreateRenderTargetView failed" << std::endl;
            return false;
        }
        return true;
    }
    bool RendererD3D11::CreateDepthStencil() {

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width          = width_;
        desc.Height         = height_;
        desc.MipLevels      = 1;
        desc.ArraySize      = 1;
        desc.Format         = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = D3D11_BIND_DEPTH_STENCIL;

        HRESULT hr = device_->CreateTexture2D(&desc, nullptr, depth_texture_.GetAddressOf());
        if (FAILED(hr)) return false;
        hr = device_->CreateDepthStencilView(depth_texture_.Get(), nullptr, dsv_.GetAddressOf());
        return SUCCEEDED(hr);
    }
    void RendererD3D11::CreateDefaultStates() {

        // Match the GL example: no face culling, depth off.
        RasterizerDesc rs{};
        rs.cull_mode = CullMode::kNone;
        SetRasterizerState(rs);

        DepthStencilDesc ds{};
        ds.depth_test = false;
        SetDepthStencilState(ds);

        BlendDesc bs{};
        bs.enabled = false;
        SetBlendState(bs);

        context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), dsv_.Get());
        current_rtv_ptrs_[0] = rtv_.Get();
        current_rtv_count_   = 1;
        current_dsv_ptr_     = dsv_.Get();
    }

}  // namespace gfx
