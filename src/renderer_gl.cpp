#include "renderer_gl.h"
#include <cstring>
#include <iostream>

namespace gfx {

    const char* RendererGl::GetName() const {
 return "OpenGL 4.6"; }
    Backend RendererGl::GetBackend() const {
 return Backend::kOpenGL; }
    bool RendererGl::Initialize(GLFWwindow* window) {

        window_ = window;
        glfwMakeContextCurrent(window);
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            std::cerr << "[GL] gladLoadGLLoader failed" << std::endl;
            return false;
        }
        std::cout << "[GL] " << glGetString(GL_VERSION) << " - " << glGetString(GL_RENDERER) << std::endl;
        return true;
    }
    void RendererGl::Shutdown() {

        for (auto& kv : programs_)       glDeleteProgram(kv.second);
        for (auto& kv : shaders_)        glDeleteShader(kv.second.id);
        for (auto& kv : input_layouts_)  glDeleteVertexArrays(1, &kv.second.vao);
        for (auto& kv : framebuffers_)   glDeleteFramebuffers(1, &kv.second);
        for (auto& kv : textures_)       glDeleteTextures(1, &kv.second.id);
        for (auto& kv : samplers_)       glDeleteSamplers(1, &kv.second);
        for (auto& kv : buffers_)        glDeleteBuffers(1, &kv.second.id);

        programs_.clear();
        shaders_.clear();
        input_layouts_.clear();
        framebuffers_.clear();
        textures_.clear();
        samplers_.clear();
        buffers_.clear();
    }
    void RendererGl::Resize(uint32_t width, uint32_t height) {

        width_  = width;
        height_ = height;
    }
    Handle RendererGl::CreateBuffer(BufferType type, Usage /*usage*/, size_t size, const void* data) {

        GLuint id = 0;
        glGenBuffers(1, &id);
        GLenum target = ToGlTarget(type);
        glBindBuffer(target, id);
        glBufferData(target, static_cast<GLsizeiptr>(size), data, GL_STATIC_DRAW);  // usage ignored for simplicity
        glBindBuffer(target, 0);

        Handle h = next_handle_++;
        buffers_[h] = {id, type};
        return h;
    }
    void RendererGl::UpdateBuffer(Handle buffer, size_t offset, size_t size, const void* data) {

        auto& b = buffers_.at(buffer);
        GLenum target = ToGlTarget(b.type);
        glBindBuffer(target, b.id);
        glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
        glBindBuffer(target, 0);
    }
    void RendererGl::DestroyBuffer(Handle buffer) {

        auto it = buffers_.find(buffer);
        if (it == buffers_.end()) return;
        glDeleteBuffers(1, &it->second.id);
        buffers_.erase(it);
    }
    Handle RendererGl::CreateTexture2D(uint32_t width, uint32_t height,
                            uint32_t mip_levels, TextureFormat format,
                            const void* data) {

        if (mip_levels == 0) mip_levels = 1;

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexStorage2D(GL_TEXTURE_2D, static_cast<GLsizei>(mip_levels),
                       ToGlInternalFormat(format),
                       static_cast<GLsizei>(width),
                       static_cast<GLsizei>(height));
        if (data) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                            static_cast<GLsizei>(width),
                            static_cast<GLsizei>(height),
                            ToGlPixelFormat(format), ToGlPixelType(format), data);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                        static_cast<GLint>(mip_levels - 1));
        glBindTexture(GL_TEXTURE_2D, 0);

        Handle h = next_handle_++;
        textures_[h] = {id, TextureType::k2D, format, width, height, 1u, mip_levels};
        return h;
    }
    Handle RendererGl::CreateTextureCube(uint32_t size, uint32_t mip_levels, TextureFormat format, const void* data[6]) {

        if (mip_levels == 0) mip_levels = 1;

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id);
        glTexStorage2D(GL_TEXTURE_CUBE_MAP, static_cast<GLsizei>(mip_levels),
                       ToGlInternalFormat(format),
                       static_cast<GLsizei>(size),
                       static_cast<GLsizei>(size));
        if (data) {
            GLenum fmt = ToGlPixelFormat(format);
            GLenum typ = ToGlPixelType(format);
            for (int i = 0; i < 6; ++i) {
                if (data[i]) {
                    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                                    0, 0, 0, static_cast<GLsizei>(size),
                                    static_cast<GLsizei>(size), fmt, typ, data[i]);
                }
            }
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,
                        static_cast<GLint>(mip_levels - 1));
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        Handle h = next_handle_++;
        textures_[h] = {id, TextureType::kCube, format, size, size, 1u, mip_levels};
        return h;
    }
    Handle RendererGl::CreateTexture2DMsaa(uint32_t width, uint32_t height, uint32_t samples, TextureFormat format) {

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, id);
        glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                                  static_cast<GLsizei>(samples),
                                  ToGlInternalFormat(format),
                                  static_cast<GLsizei>(width),
                                  static_cast<GLsizei>(height), GL_TRUE);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

        Handle h = next_handle_++;
        textures_[h] = {id, TextureType::k2DMsaa, format, width, height, 1u, 1u};
        return h;
    }
    void RendererGl::UpdateTexture(Handle texture, uint32_t mip_level,
                       uint32_t x, uint32_t y, uint32_t z,
                       uint32_t width, uint32_t height, uint32_t /*depth*/,
                       const void* data) {

        auto& t = textures_.at(texture);
        GLenum target = ToGlTarget(t.type);
        glBindTexture(target, t.id);
        GLenum fmt = ToGlPixelFormat(t.format);
        GLenum typ = ToGlPixelType(t.format);

        if (t.type == TextureType::kCube) {
            glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + z,
                             static_cast<GLint>(mip_level),
                             static_cast<GLint>(x), static_cast<GLint>(y),
                             static_cast<GLsizei>(width),
                             static_cast<GLsizei>(height), fmt, typ, data);
        } else {
            glTexSubImage2D(target, static_cast<GLint>(mip_level),
                             static_cast<GLint>(x), static_cast<GLint>(y),
                             static_cast<GLsizei>(width),
                             static_cast<GLsizei>(height), fmt, typ, data);
        }
        glBindTexture(target, 0);
    }
    void RendererGl::GenerateMipmaps(Handle texture) {

        auto& t = textures_.at(texture);
        glBindTexture(ToGlTarget(t.type), t.id);
        glGenerateMipmap(ToGlTarget(t.type));
        glBindTexture(ToGlTarget(t.type), 0);
    }
    void RendererGl::DestroyTexture(Handle texture) {

        auto it = textures_.find(texture);
        if (it == textures_.end()) return;
        glDeleteTextures(1, &it->second.id);
        textures_.erase(it);
    }
    Handle RendererGl::CreateSampler(const SamplerDesc& desc) {

        GLuint id = 0;
        glGenSamplers(1, &id);
        glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, ToGlMinFilter(desc));
        glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, ToGlMagFilter(desc));
        glSamplerParameteri(id, GL_TEXTURE_WRAP_S, ToGlWrap(desc.wrap_u));
        glSamplerParameteri(id, GL_TEXTURE_WRAP_T, ToGlWrap(desc.wrap_v));
        glSamplerParameteri(id, GL_TEXTURE_WRAP_R, ToGlWrap(desc.wrap_w));
        glSamplerParameterfv(id, GL_TEXTURE_BORDER_COLOR, desc.border_color);
        glSamplerParameterf(id, GL_TEXTURE_MIN_LOD, desc.min_lod);
        glSamplerParameterf(id, GL_TEXTURE_MAX_LOD, desc.max_lod);
        if (desc.max_anisotropy > 1) {
            glSamplerParameterf(id, GL_TEXTURE_MAX_ANISOTROPY,
                                 static_cast<float>(desc.max_anisotropy));
        }

        Handle h = next_handle_++;
        samplers_[h] = id;
        return h;
    }
    void RendererGl::DestroySampler(Handle sampler) {

        auto it = samplers_.find(sampler);
        if (it == samplers_.end()) return;
        glDeleteSamplers(1, &it->second);
        samplers_.erase(it);
    }
    Handle RendererGl::CreateShader(ShaderStage stage, const char* source) {

        GLenum type = ToGlShaderStage(stage);
        GLuint id   = glCreateShader(type);
        glShaderSource(id, 1, &source, nullptr);
        glCompileShader(id);

        GLint ok = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]{};
            glGetShaderInfoLog(id, sizeof(log), nullptr, log);
            std::cerr << "[GL] shader compile error (" << ToString(stage)
                      << "):\n" << log << std::endl;
            glDeleteShader(id);
            return kInvalidHandle;
        }

        Handle h = next_handle_++;
        shaders_[h] = {id, stage};
        return h;
    }
    Handle RendererGl::CreateProgram(Handle vs, Handle fs, Handle gs,
                          Handle tcs, Handle tes) {

        GLuint prog = glCreateProgram();
        AttachShader(prog, vs);
        AttachShader(prog, fs);
        AttachShader(prog, gs);
        AttachShader(prog, tcs);
        AttachShader(prog, tes);

        glLinkProgram(prog);

        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]{};
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            std::cerr << "[GL] program link error:\n" << log << std::endl;
            glDeleteProgram(prog);
            return kInvalidHandle;
        }

        // Detach shaders — program keeps its own copy of the compiled code.
        DetachShader(prog, vs);
        DetachShader(prog, fs);
        DetachShader(prog, gs);
        DetachShader(prog, tcs);
        DetachShader(prog, tes);

        Handle h = next_handle_++;
        programs_[h] = prog;
        return h;
    }
    Handle RendererGl::CreateComputeProgram(Handle cs) {

        GLuint prog = glCreateProgram();
        AttachShader(prog, cs);
        glLinkProgram(prog);

        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]{};
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            std::cerr << "[GL] compute program link error:\n" << log << std::endl;
            glDeleteProgram(prog);
            return kInvalidHandle;
        }
        DetachShader(prog, cs);

        Handle h = next_handle_++;
        programs_[h] = prog;
        return h;
    }
    int32_t RendererGl::GetUniformLocation(Handle program, const char* name) {

        return glGetUniformLocation(programs_.at(program), name);
    }
    void RendererGl::DestroyShader(Handle shader) {

        auto it = shaders_.find(shader);
        if (it == shaders_.end()) return;
        glDeleteShader(it->second.id);
        shaders_.erase(it);
    }
    void RendererGl::DestroyProgram(Handle program) {

        auto it = programs_.find(program);
        if (it == programs_.end()) return;
        glDeleteProgram(it->second);
        programs_.erase(it);
    }
    Handle RendererGl::CreateInputLayout(Handle /*program*/, uint32_t stride, const VertexAttrib* attribs, uint32_t count) {

        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        for (uint32_t i = 0; i < count; ++i) {
            const auto& a = attribs[i];
            GLint  comp = static_cast<GLint>(AttribComponents(a.format));
            GLenum type = ToGlAttribType(a.format);

            glEnableVertexAttribArray(a.location);
            if (a.format == AttribFormat::kInt ||
                a.format == AttribFormat::kInt2 ||
                a.format == AttribFormat::kInt3 ||
                a.format == AttribFormat::kInt4) {
                glVertexAttribIFormat(a.location, comp, type, a.offset);
            } else {
                glVertexAttribFormat(a.location, comp, type,
                                     IsNormalized(a.format) ? GL_TRUE : GL_FALSE,
                                     a.offset);
            }
            // Bind all attribs to the same vertex buffer binding point 0.
            // Per-buffer binding points are used only when interleaving
            // across multiple VBOs — this example doesn't need that.
            glVertexAttribBinding(a.location, 0);
        }

        glBindVertexArray(0);

        Handle h = next_handle_++;
        input_layouts_[h] = {vao};
        return h;
    }
    void RendererGl::DestroyInputLayout(Handle layout) {

        auto it = input_layouts_.find(layout);
        if (it == input_layouts_.end()) return;
        glDeleteVertexArrays(1, &it->second.vao);
        input_layouts_.erase(it);
    }
    Handle RendererGl::CreateFramebuffer(
        uint32_t num_color, const FramebufferAttachment* color,
        const FramebufferAttachment* depth_stencil) {

        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        std::vector<GLenum> draw_buffers;
        for (uint32_t i = 0; i < num_color; ++i) {
            GLuint tex_id = textures_.at(color[i].texture).id;
            GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
            if (color[i].cube_face != CubeFace::kPositiveX || textures_.at(color[i].texture).type == TextureType::kCube) {
                // Cubemap: attach a single face.
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<int>(color[i].cube_face),
                                        tex_id, static_cast<GLint>(color[i].mip_level));
            } else {
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                        GL_TEXTURE_2D, tex_id,
                                        static_cast<GLint>(color[i].mip_level));
            }
            draw_buffers.push_back(attachment);
        }
        glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()),
                       draw_buffers.data());

        if (depth_stencil && depth_stencil->texture != kInvalidHandle) {
            GLuint ds_id = textures_.at(depth_stencil->texture).id;
            auto   fmt   = textures_.at(depth_stencil->texture).format;
            GLenum attach = (fmt == TextureFormat::kDepth24Stencil8)
                                ? GL_DEPTH_STENCIL_ATTACHMENT
                                : GL_DEPTH_ATTACHMENT;
            glFramebufferTexture2D(GL_FRAMEBUFFER, attach, GL_TEXTURE_2D,
                                    ds_id,
                                    static_cast<GLint>(depth_stencil->mip_level));
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[GL] framebuffer incomplete: 0x"
                      << std::hex << status << std::endl;
            glDeleteFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, current_fbo_);
            return kInvalidHandle;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, current_fbo_);

        Handle h = next_handle_++;
        framebuffers_[h] = fbo;
        return h;
    }
    void RendererGl::DestroyFramebuffer(Handle framebuffer) {

        auto it = framebuffers_.find(framebuffer);
        if (it == framebuffers_.end()) return;
        if (current_fbo_ == it->second) current_fbo_ = 0;
        glDeleteFramebuffers(1, &it->second);
        framebuffers_.erase(it);
    }
    void RendererGl::SetRasterizerState(const RasterizerDesc& desc) {

        if (desc.cull_mode == CullMode::kNone) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(ToGlCullFace(desc.cull_mode));
        }
        glFrontFace(desc.front_ccw ? GL_CCW : GL_CW);
        glPolygonMode(GL_FRONT_AND_BACK, desc.fill_mode == FillMode::kSolid
                                              ? GL_FILL : GL_LINE);

        if (desc.depth_bias != 0 || desc.depth_bias_slope != 0.0f) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(desc.depth_bias_slope,
                             static_cast<float>(desc.depth_bias));
        } else {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        if (desc.scissor_enabled)
            glEnable(GL_SCISSOR_TEST);
        else
            glDisable(GL_SCISSOR_TEST);

        glEnable(GL_DEPTH_CLAMP);  // GL reverse: depth_clip == !depth_clamp
        if (desc.depth_clip)
            glDisable(GL_DEPTH_CLAMP);

        rasterizer_ = desc;
    }
    void RendererGl::SetDepthStencilState(const DepthStencilDesc& desc) {

        if (desc.depth_test)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        glDepthMask(desc.depth_write ? GL_TRUE : GL_FALSE);
        glDepthFunc(ToGlCompareFunc(desc.depth_func));

        if (desc.stencil_test)
            glEnable(GL_STENCIL_TEST);
        else
            glDisable(GL_STENCIL_TEST);

        if (desc.stencil_test) {
            glStencilMask(desc.stencil_write_mask);
            auto SetFace = [&](GLenum face, const DepthStencilDesc::StencilFace& s) {
                glStencilFuncSeparate(face,
                                       ToGlCompareFunc(s.func),
                                       desc.stencil_ref,
                                       desc.stencil_read_mask);
                glStencilOpSeparate(face,
                                     ToGlStencilOp(s.fail_op),
                                     ToGlStencilOp(s.depth_fail_op),
                                     ToGlStencilOp(s.pass_op));
            };
            SetFace(GL_FRONT, desc.front);
            SetFace(GL_BACK,  desc.back);
        }

        depth_stencil_ = desc;
    }
    void RendererGl::SetBlendState(const BlendDesc& desc) {

        if (desc.enabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);

        glBlendFuncSeparate(ToGlBlendFactor(desc.src_color),
                             ToGlBlendFactor(desc.dst_color),
                             ToGlBlendFactor(desc.src_alpha),
                             ToGlBlendFactor(desc.dst_alpha));
        glBlendEquationSeparate(ToGlBlendOp(desc.color_op),
                                 ToGlBlendOp(desc.alpha_op));
        glColorMask((desc.write_mask & 1) != 0,
                     (desc.write_mask & 2) != 0,
                     (desc.write_mask & 4) != 0,
                     (desc.write_mask & 8) != 0);
    }
    void RendererGl::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

        glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
    void RendererGl::SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

        glScissor(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
    void RendererGl::SetPrimitiveType(PrimitiveType type) {

        current_topology_ = ToGlTopology(type);
    }
    void RendererGl::SetVertexLayout(const VertexAttrib* attribs, uint32_t count,
                          uint32_t stride) {

        // Record the layout; applied on next BindVertexBuffer when a VAO is
        // already bound, or on the next CreateInputLayout.
        vertex_attribs_.assign(attribs, attribs + count);
        attrib_stride_ = stride;
    }
    void RendererGl::BindProgram(Handle program) {

        GLuint id = programs_.at(program);
        glUseProgram(id);
        current_program_ = id;
    }
    void RendererGl::BindInputLayout(Handle layout) {

        glBindVertexArray(input_layouts_.at(layout).vao);
    }
    void RendererGl::BindVertexBuffer(uint32_t slot, Handle buffer, uint32_t stride, uint32_t offset) {

        auto& b = buffers_.at(buffer);
        // GL 4.3+: binds a buffer directly to a VAO binding point.
        // The currently bound VAO receives the binding.
        glBindVertexBuffer(slot, b.id, static_cast<GLintptr>(offset), static_cast<GLsizei>(stride));
        current_vb_ = b.id;
    }
    void RendererGl::BindIndexBuffer(Handle buffer, IndexFormat format) {

        auto& b = buffers_.at(buffer);
        // GL 3.0+: binds an element array buffer to the current VAO.
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b.id);
        current_index_format_ = (format == IndexFormat::kUint32)
                                    ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
        current_ib_ = b.id;
    }
    void RendererGl::BindTexture(uint32_t slot, Handle texture) {

        glActiveTexture(GL_TEXTURE0 + slot);
        auto& t = textures_.at(texture);
        glBindTexture(ToGlTarget(t.type), t.id);
    }
    void RendererGl::BindSampler(uint32_t slot, Handle sampler) {

        glBindSampler(slot, samplers_.at(sampler));
    }
    void RendererGl::BindUniformBuffer(uint32_t slot, Handle buffer,
                            size_t offset, size_t size) {

        auto& b = buffers_.at(buffer);
        glBindBufferRange(GL_UNIFORM_BUFFER, slot, b.id,
                           static_cast<GLintptr>(offset),
                           static_cast<GLsizeiptr>(size > 0 ? size
                               : buffers_.at(buffer).size - offset));
    }
    void RendererGl::BindStorageBuffer(uint32_t slot, Handle buffer) {

        auto& b = buffers_.at(buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, b.id);
    }
    void RendererGl::BindFramebuffer(Handle framebuffer) {

        GLuint fbo = framebuffers_.at(framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        current_fbo_ = fbo;
    }
    void RendererGl::BindDefaultFramebuffer() {

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        current_fbo_ = 0;
    }
    void RendererGl::SetUniformInt(int32_t location, int32_t value) {

        glUniform1i(location, value);
    }
    void RendererGl::SetUniformFloat(int32_t location, float value) {

        glUniform1f(location, value);
    }
    void RendererGl::SetUniformVec2(int32_t location, const float* value) {

        glUniform2fv(location, 1, value);
    }
    void RendererGl::SetUniformVec3(int32_t location, const float* value) {

        glUniform3fv(location, 1, value);
    }
    void RendererGl::SetUniformVec4(int32_t location, const float* value) {

        glUniform4fv(location, 1, value);
    }
    void RendererGl::SetUniformMat3(int32_t location, const float* value) {

        glUniformMatrix3fv(location, 1, GL_FALSE, value);
    }
    void RendererGl::SetUniformMat4(int32_t location, const float* value) {

        glUniformMatrix4fv(location, 1, GL_FALSE, value);
    }
    void RendererGl::Clear(ClearFlags flags,
                float r, float g, float b, float a,
                float depth, uint8_t stencil) {

        GLbitfield mask = 0;
        if (flags & ClearFlags::kColor) {
            glClearColor(r, g, b, a);
            mask |= GL_COLOR_BUFFER_BIT;
        }
        if (flags & ClearFlags::kDepth) {
            glClearDepth(static_cast<GLdouble>(depth));
            mask |= GL_DEPTH_BUFFER_BIT;
        }
        if (flags & ClearFlags::kStencil) {
            glClearStencil(static_cast<GLint>(stencil));
            mask |= GL_STENCIL_BUFFER_BIT;
        }
        glClear(mask);
    }
    void RendererGl::Draw(uint32_t vertex_count, uint32_t first_vertex) {

        glDrawArrays(current_topology_,
                     static_cast<GLint>(first_vertex),
                     static_cast<GLsizei>(vertex_count));
    }
    void RendererGl::DrawIndexed(uint32_t index_count, uint32_t first_index,
                      int32_t vertex_offset) {

        // vertex_offset support: glDrawElementsBaseVertex (GL 3.2+)
        const void* offset_ptr = reinterpret_cast<const void*>(
            static_cast<uintptr_t>(first_index)
            * (current_index_format_ == GL_UNSIGNED_INT ? 4 : 2));
        if (vertex_offset != 0) {
            glDrawElementsBaseVertex(current_topology_,
                                     static_cast<GLsizei>(index_count),
                                     current_index_format_,
                                     offset_ptr, vertex_offset);
        } else {
            glDrawElements(current_topology_,
                           static_cast<GLsizei>(index_count),
                           current_index_format_, offset_ptr);
        }
    }
    void RendererGl::DrawInstanced(uint32_t vertex_count_per_instance,
                        uint32_t instance_count,
                        uint32_t first_vertex,
                        uint32_t first_instance) {

        if (first_instance != 0) {
            glDrawArraysInstancedBaseInstance(
                current_topology_,
                static_cast<GLint>(first_vertex),
                static_cast<GLsizei>(vertex_count_per_instance),
                static_cast<GLsizei>(instance_count),
                first_instance);
        } else {
            glDrawArraysInstanced(current_topology_,
                                  static_cast<GLint>(first_vertex),
                                  static_cast<GLsizei>(vertex_count_per_instance),
                                  static_cast<GLsizei>(instance_count));
        }
    }
    void RendererGl::DrawIndexedInstanced(uint32_t index_count_per_instance,
                               uint32_t instance_count,
                               uint32_t first_index,
                               int32_t vertex_offset,
                               uint32_t first_instance) {

        const void* offset_ptr = reinterpret_cast<const void*>(
            static_cast<uintptr_t>(first_index)
            * (current_index_format_ == GL_UNSIGNED_INT ? 4 : 2));
        if (first_instance != 0) {
            glDrawElementsInstancedBaseVertexBaseInstance(
                current_topology_,
                static_cast<GLsizei>(index_count_per_instance),
                current_index_format_, offset_ptr,
                static_cast<GLsizei>(instance_count),
                vertex_offset, first_instance);
        } else if (vertex_offset != 0) {
            glDrawElementsInstancedBaseVertex(
                current_topology_,
                static_cast<GLsizei>(index_count_per_instance),
                current_index_format_, offset_ptr,
                static_cast<GLsizei>(instance_count), vertex_offset);
        } else {
            glDrawElementsInstanced(current_topology_,
                                    static_cast<GLsizei>(index_count_per_instance),
                                    current_index_format_, offset_ptr,
                                    static_cast<GLsizei>(instance_count));
        }
    }
    void RendererGl::BindImageTexture(uint32_t slot, Handle texture, uint32_t mip_level, ImageAccess access) {

        GLenum gl_access = GL_READ_WRITE;
        switch (access) {
        case ImageAccess::kReadOnly:  gl_access = GL_READ_ONLY;  break;
        case ImageAccess::kWriteOnly: gl_access = GL_WRITE_ONLY; break;
        case ImageAccess::kReadWrite: gl_access = GL_READ_WRITE; break;
        }
        auto& t = textures_.at(texture);
        glBindImageTexture(slot, t.id, static_cast<GLint>(mip_level),
                           GL_FALSE, 0, gl_access,
                           ToGlInternalFormat(t.format));
    }
    void RendererGl::Dispatch(uint32_t groups_x, uint32_t groups_y,
                   uint32_t groups_z) {

        glDispatchCompute(groups_x, groups_y, groups_z);
    }
    void RendererGl::MemoryBarrier() {

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }
    void RendererGl::PushDebugGroup(const char* name) {

        if (glPushDebugGroup)  // GL 4.3+
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0,
                              static_cast<GLsizei>(std::strlen(name)), name);
    }
    void RendererGl::PopDebugGroup() {

        if (glPopDebugGroup)
            glPopDebugGroup();
    }
    void RendererGl::Present() {

        glfwSwapBuffers(window_);
    }
    GLuint RendererGl::GetNativeTexture(Handle h) const {
 return textures_.at(h).id; }
    GLuint RendererGl::GetNativeBuffer(Handle h)  const {
 return buffers_.at(h).id; }
    GLuint RendererGl::GetNativeProgram(Handle h) const {
 return programs_.at(h); }
    GLenum RendererGl::ToGlTarget(BufferType type) {

        switch (type) {
        case BufferType::kVertex:    return GL_ARRAY_BUFFER;
        case BufferType::kIndex:     return GL_ELEMENT_ARRAY_BUFFER;
        case BufferType::kUniform:   return GL_UNIFORM_BUFFER;
        case BufferType::kStorage:   return GL_SHADER_STORAGE_BUFFER;
        }
        return GL_ARRAY_BUFFER;
    }
    GLenum RendererGl::ToGlTarget(TextureType type) {

        switch (type) {
        case TextureType::k2D:       return GL_TEXTURE_2D;
        case TextureType::kCube:     return GL_TEXTURE_CUBE_MAP;
        case TextureType::k2DMsaa:   return GL_TEXTURE_2D_MULTISAMPLE;
        case TextureType::k3D:       return GL_TEXTURE_3D;
        }
        return GL_TEXTURE_2D;
    }
    GLenum RendererGl::ToGlShaderStage(ShaderStage s) {

        switch (s) {
        case ShaderStage::kVertex:        return GL_VERTEX_SHADER;
        case ShaderStage::kFragment:      return GL_FRAGMENT_SHADER;
        case ShaderStage::kGeometry:      return GL_GEOMETRY_SHADER;
        case ShaderStage::kCompute:       return GL_COMPUTE_SHADER;
        case ShaderStage::kTessControl:   return GL_TESS_CONTROL_SHADER;
        case ShaderStage::kTessEval:      return GL_TESS_EVALUATION_SHADER;
        }
        return GL_VERTEX_SHADER;
    }
    GLenum RendererGl::ToGlInternalFormat(TextureFormat f) {

        switch (f) {
        case TextureFormat::kR8Unorm:          return GL_R8;
        case TextureFormat::kRg8Unorm:         return GL_RG8;
        case TextureFormat::kRgba8Unorm:       return GL_RGBA8;
        case TextureFormat::kRgba8Srgb:        return GL_SRGB8_ALPHA8;
        case TextureFormat::kR16F:             return GL_R16F;
        case TextureFormat::kRg16F:            return GL_RG16F;
        case TextureFormat::kRgba16F:          return GL_RGBA16F;
        case TextureFormat::kR32F:             return GL_R32F;
        case TextureFormat::kRg32F:            return GL_RG32F;
        case TextureFormat::kRgba32F:          return GL_RGBA32F;
        case TextureFormat::kR11G11B10F:       return GL_R11F_G11F_B10F;
        case TextureFormat::kDepth16:          return GL_DEPTH_COMPONENT16;
        case TextureFormat::kDepth24Stencil8:  return GL_DEPTH24_STENCIL8;
        case TextureFormat::kDepth32F:         return GL_DEPTH_COMPONENT32F;
        }
        return GL_RGBA8;
    }
    GLenum RendererGl::ToGlPixelFormat(TextureFormat f) {

        switch (f) {
        case TextureFormat::kR8Unorm:          return GL_RED;
        case TextureFormat::kRg8Unorm:         return GL_RG;
        case TextureFormat::kRgba8Unorm:
        case TextureFormat::kRgba8Srgb:        return GL_RGBA;
        case TextureFormat::kR16F:             return GL_RED;
        case TextureFormat::kRg16F:            return GL_RG;
        case TextureFormat::kRgba16F:          return GL_RGBA;
        case TextureFormat::kR32F:             return GL_RED;
        case TextureFormat::kRg32F:            return GL_RG;
        case TextureFormat::kRgba32F:          return GL_RGBA;
        case TextureFormat::kR11G11B10F:       return GL_RGB;
        case TextureFormat::kDepth16:
        case TextureFormat::kDepth24Stencil8:
        case TextureFormat::kDepth32F:         return GL_DEPTH_COMPONENT;
        }
        return GL_RGBA;
    }
    GLenum RendererGl::ToGlPixelType(TextureFormat f) {

        switch (f) {
        case TextureFormat::kR8Unorm:
        case TextureFormat::kRg8Unorm:
        case TextureFormat::kRgba8Unorm:
        case TextureFormat::kRgba8Srgb:        return GL_UNSIGNED_BYTE;
        case TextureFormat::kR16F:
        case TextureFormat::kRg16F:
        case TextureFormat::kRgba16F:          return GL_HALF_FLOAT;
        case TextureFormat::kR32F:
        case TextureFormat::kRg32F:
        case TextureFormat::kRgba32F:          return GL_FLOAT;
        case TextureFormat::kR11G11B10F:       return GL_UNSIGNED_INT_10F_11F_11F_REV;
        case TextureFormat::kDepth16:          return GL_UNSIGNED_SHORT;
        case TextureFormat::kDepth24Stencil8:  return GL_UNSIGNED_INT_24_8;
        case TextureFormat::kDepth32F:         return GL_FLOAT;
        }
        return GL_UNSIGNED_BYTE;
    }
    GLenum RendererGl::ToGlAttribType(AttribFormat f) {

        switch (f) {
        case AttribFormat::kFloat:
        case AttribFormat::kFloat2:
        case AttribFormat::kFloat3:
        case AttribFormat::kFloat4:            return GL_FLOAT;
        case AttribFormat::kInt:
        case AttribFormat::kInt2:
        case AttribFormat::kInt3:
        case AttribFormat::kInt4:              return GL_INT;
        case AttribFormat::kUbyte4Norm:        return GL_UNSIGNED_BYTE;
        }
        return GL_FLOAT;
    }
    bool RendererGl::IsNormalized(AttribFormat f) {

        return f == AttribFormat::kUbyte4Norm;
    }
    GLenum RendererGl::ToGlTopology(PrimitiveType t) {

        switch (t) {
        case PrimitiveType::kTriangles:      return GL_TRIANGLES;
        case PrimitiveType::kTriangleStrip:  return GL_TRIANGLE_STRIP;
        case PrimitiveType::kPoints:         return GL_POINTS;
        case PrimitiveType::kLines:          return GL_LINES;
        }
        return GL_TRIANGLES;
    }
    GLenum RendererGl::ToGlCompareFunc(CompareFunc f) {

        switch (f) {
        case CompareFunc::kNever:    return GL_NEVER;
        case CompareFunc::kLess:     return GL_LESS;
        case CompareFunc::kEqual:    return GL_EQUAL;
        case CompareFunc::kLequal:   return GL_LEQUAL;
        case CompareFunc::kGreater:  return GL_GREATER;
        case CompareFunc::kNotequal: return GL_NOTEQUAL;
        case CompareFunc::kGequal:   return GL_GEQUAL;
        case CompareFunc::kAlways:   return GL_ALWAYS;
        }
        return GL_LESS;
    }
    GLenum RendererGl::ToGlBlendFactor(BlendFactor f) {

        switch (f) {
        case BlendFactor::kZero:         return GL_ZERO;
        case BlendFactor::kOne:          return GL_ONE;
        case BlendFactor::kSrcColor:     return GL_SRC_COLOR;
        case BlendFactor::kInvSrcColor:  return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::kSrcAlpha:     return GL_SRC_ALPHA;
        case BlendFactor::kInvSrcAlpha:  return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::kDstColor:     return GL_DST_COLOR;
        case BlendFactor::kInvDstColor:  return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::kDstAlpha:     return GL_DST_ALPHA;
        case BlendFactor::kInvDstAlpha:  return GL_ONE_MINUS_DST_ALPHA;
        }
        return GL_ONE;
    }
    GLenum RendererGl::ToGlBlendOp(BlendOp op) {

        switch (op) {
        case BlendOp::kAdd:    return GL_FUNC_ADD;
        case BlendOp::kSub:    return GL_FUNC_SUBTRACT;
        case BlendOp::kRevSub: return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::kMin:    return GL_MIN;
        case BlendOp::kMax:    return GL_MAX;
        }
        return GL_FUNC_ADD;
    }
    GLenum RendererGl::ToGlCullFace(CullMode m) {

        switch (m) {
        case CullMode::kFront: return GL_FRONT;
        case CullMode::kBack:  return GL_BACK;
        case CullMode::kNone:  return GL_BACK;  // unused when cull is off
        }
        return GL_BACK;
    }
    GLenum RendererGl::ToGlStencilOp(StencilOp op) {

        switch (op) {
        case StencilOp::kKeep:       return GL_KEEP;
        case StencilOp::kZero:       return GL_ZERO;
        case StencilOp::kReplace:    return GL_REPLACE;
        case StencilOp::kIncrClamp:  return GL_INCR;
        case StencilOp::kDecrClamp:  return GL_DECR;
        case StencilOp::kInvert:     return GL_INVERT;
        case StencilOp::kIncrWrap:   return GL_INCR_WRAP;
        case StencilOp::kDecrWrap:   return GL_DECR_WRAP;
        }
        return GL_KEEP;
    }
    GLenum RendererGl::ToGlMinFilter(const SamplerDesc& d) {

        if (d.min_filter == FilterMode::kAnisotropic) {
            return d.mip_filter == FilterMode::kLinear
                       ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;
        }
        if (d.min_filter == FilterMode::kLinear) {
            return d.mip_filter == FilterMode::kLinear
                       ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;
        }
        return d.mip_filter == FilterMode::kLinear
                   ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
    }
    GLenum RendererGl::ToGlMagFilter(const SamplerDesc& d) {

        return d.mag_filter == FilterMode::kLinear ? GL_LINEAR : GL_NEAREST;
    }
    GLenum RendererGl::ToGlWrap(WrapMode w) {

        switch (w) {
        case WrapMode::kRepeat:          return GL_REPEAT;
        case WrapMode::kClampToEdge:     return GL_CLAMP_TO_EDGE;
        case WrapMode::kClampToBorder:   return GL_CLAMP_TO_BORDER;
        case WrapMode::kMirrorRepeat:    return GL_MIRRORED_REPEAT;
        }
        return GL_REPEAT;
    }
    const char* RendererGl::ToString(ShaderStage s) {

        switch (s) {
        case ShaderStage::kVertex:      return "vertex";
        case ShaderStage::kFragment:     return "fragment";
        case ShaderStage::kGeometry:     return "geometry";
        case ShaderStage::kCompute:      return "compute";
        case ShaderStage::kTessControl:  return "tess control";
        case ShaderStage::kTessEval:     return "tess eval";
        }
        return "unknown";
    }
    void RendererGl::AttachShader(GLuint program, Handle shader) {

        if (shader == kInvalidHandle) return;
        glAttachShader(program, shaders_.at(shader).id);
    }
    void RendererGl::DetachShader(GLuint program, Handle shader) {

        if (shader == kInvalidHandle) return;
        glDetachShader(program, shaders_.at(shader).id);
    }

}  // namespace gfx
