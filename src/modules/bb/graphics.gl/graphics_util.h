#ifndef BB_GRAPHICS_UTIL_H
#define BB_GRAPHICS_UTIL_H

#include "graphics.gl.h"

// ============================================================================
// UBO (Uniform Buffer Object) Management
// ============================================================================

struct GLDynamicUBO {
    GLuint handle;
    size_t size;
    GLuint binding_point;
    bool initialized;
};

// Initialize a dynamic UBO structure (does not allocate GPU resources yet)
inline void bbGLInitDynamicUBO(GLDynamicUBO *ubo, size_t size, GLuint bindingPoint) {
    ubo->handle = 0;
    ubo->size = size;
    ubo->binding_point = bindingPoint;
    ubo->initialized = false;
}

// Update UBO data - creates buffer on first call, updates on subsequent calls
inline void bbGLUpdateDynamicUBO(GLDynamicUBO *ubo, const void *data) {
    if (!ubo->initialized) {
        GL(glGenBuffers(1, &ubo->handle));
        GL(glBindBuffer(GL_UNIFORM_BUFFER, ubo->handle));
        GL(glBufferData(GL_UNIFORM_BUFFER, ubo->size, nullptr, GL_DYNAMIC_DRAW));
        GL(glBindBufferRange(GL_UNIFORM_BUFFER, ubo->binding_point, ubo->handle, 0, ubo->size));
        ubo->initialized = true;
    } else {
        GL(glBindBuffer(GL_UNIFORM_BUFFER, ubo->handle));
    }
    GL(glBufferSubData(GL_UNIFORM_BUFFER, 0, ubo->size, data));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));
}

// Delete UBO resources
inline void bbGLDeleteDynamicUBO(GLDynamicUBO *ubo) {
    if (ubo->handle) {
        glDeleteBuffers(1, &ubo->handle);
        ubo->handle = 0;
        ubo->initialized = false;
    }
}

// ============================================================================
// Texture Parameter Caching
// ============================================================================

struct GLTextureParamCache {
    GLuint bound_texture;
    GLint min_filter;
    GLint mag_filter;
    GLint wrap_s;
    GLint wrap_t;
};

// Initialize texture parameter cache
inline void bbGLInitTexParamCache(GLTextureParamCache *cache) {
    cache->bound_texture = 0;
    cache->min_filter = -1;
    cache->mag_filter = -1;
    cache->wrap_s = -1;
    cache->wrap_t = -1;
}

// Set texture parameters with caching - only calls glTexParameteri when values change
inline void bbGLSetTextureParams(GLTextureParamCache *cache, GLuint texture, GLenum target,
                                  GLint min_filter, GLint mag_filter,
                                  GLint wrap_s, GLint wrap_t) {
    // If texture changed, reset cache
    if (cache->bound_texture != texture) {
        cache->min_filter = -1;
        cache->mag_filter = -1;
        cache->wrap_s = -1;
        cache->wrap_t = -1;
        cache->bound_texture = texture;
    }

    if (cache->min_filter != min_filter) {
        GL(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter));
        cache->min_filter = min_filter;
    }
    if (cache->mag_filter != mag_filter) {
        GL(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter));
        cache->mag_filter = mag_filter;
    }
    if (cache->wrap_s != wrap_s) {
        GL(glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap_s));
        cache->wrap_s = wrap_s;
    }
    if (cache->wrap_t != wrap_t) {
        GL(glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap_t));
        cache->wrap_t = wrap_t;
    }
}

#endif // BB_GRAPHICS_UTIL_H
