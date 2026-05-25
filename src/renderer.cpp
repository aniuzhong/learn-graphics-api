// renderer.cpp — factory implementation

#include "renderer.h"
#include "renderer_gl.h"
#include "renderer_d3d11.h"

namespace gfx {

IRenderer* CreateRenderer(Backend backend) {
    switch (backend) {
    case Backend::kOpenGL:
        return new RendererGl();
    case Backend::kD3D11:
        return new RendererD3D11();
    default:
        return nullptr;
    }
}

void DestroyRenderer(IRenderer* renderer) {
    delete renderer;
}

}  // namespace gfx
