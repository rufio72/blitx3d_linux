# OpenGL Optimizations Plan

This document outlines the planned optimizations for the OpenGL renderer in `src/modules/bb/blitz3d.gl/`.

---

## Phase 1: Completed Optimizations ✅

These optimizations have been implemented and tested with a **15% FPS improvement**.

### 1. Uniform Location Caching ✅

**File:** `blitz3d.gl.cpp`

**Problem:** `glGetUniformLocation()` was called every frame for projection, view, and world matrices.

**Solution:** Cache uniform locations once when the shader is compiled.

```cpp
struct {
    GLint proj_matrix = -1;
    GLint view_matrix = -1;
    GLint world_matrix = -1;
    GLint normal_matrix = -1;
} uniform_locs;
```

**Impact:** High - reduces CPU overhead per frame

---

### 2. UBO Update Optimization ✅

**File:** `blitz3d.gl.cpp`

**Problem:** Uniform Buffer Objects were updated with `glBufferData()` every frame, reallocating GPU memory.

**Solution:** Use `glBufferSubData()` instead - allocate once, update each frame.

**Impact:** Medium - reduces GPU memory churn

---

### 3. Index Buffer Re-upload Fix ✅

**File:** `blitz3d.gl.cpp`

**Problem:** `offsetIndices()` and `offsetArrays()` called `glBufferData()` on every render call.

**Solution:** Upload buffer data once in `unlock()`, use `glDrawElementsBaseVertex()` with offsets.

**Impact:** Very High - major performance improvement for complex scenes

---

### 4. Normal Matrix CPU Calculation ✅

**File:** `blitz3d.gl.cpp`, `default.glsl`

**Problem:** Matrix inversion (`transpose(inverse(...))`) was computed in the vertex shader for every vertex.

**Solution:** Precompute normal matrix on CPU in `setWorldMatrix()` and pass as uniform.

**Impact:** High - eliminates expensive `inverse()` per vertex

---

### 5. Texture State Caching ✅

**File:** `blitz3d.gl.cpp`

**Problem:** All 16 texture slots were unbound at the start of every `setRenderState()` call.

**Solution:** Track bound textures and only unbind when necessary.

**Impact:** Medium - reduces redundant GL state changes

---

## Phase 2: Planned Optimizations

### 6. Instanced Rendering for Identical Objects

**Files:** `blitz3d.gl.cpp`, `model.cpp`, `world.cpp`

**Problem:** Each object is rendered with a separate `glDrawElements` call, even when multiple objects share the same mesh geometry.

**Solution:** Implement `glDrawElementsInstanced` with a transformation matrix buffer for objects sharing geometry.

```cpp
// Instead of:
for each object:
    setWorldMatrix(object.transform)
    glDrawElements(...)

// Use:
// Upload instance transforms to a buffer
glBindBuffer(GL_ARRAY_BUFFER, instance_buffer);
glBufferSubData(..., instance_transforms, ...);
glDrawElementsInstanced(GL_TRIANGLES, tri_cnt*3, GL_UNSIGNED_INT, 0, instance_count);
```

**Impact:** Very High for scenes with many identical objects (trees, particles, etc.)
**Complexity:** High - requires tracking which objects share meshes

---

### 7. State Sorting / Batching

**File:** `model.cpp` (lines 122-129)

**Problem:** In `renderQueue()`, each `MeshQueue` calls `setRenderState()` even if the state is identical to the previous call.

```cpp
// Current - calls setRenderState every time
void Model::renderQueue( int type ){
    for( ;que->size();que->pop_back() ){
        MeshQueue *q=que->back();
        q->render();  // always calls setRenderState()
    }
}
```

**Solution:** Sort the queue by RenderState and skip redundant state changes:

```cpp
// Sort by brush/material before rendering
std::sort(queues[type].begin(), queues[type].end(),
    [](MeshQueue* a, MeshQueue* b) { return a->brush < b->brush; });

// Render with state caching
const BBScene::RenderState* last_state = nullptr;
for( auto& q : queues[type] ){
    const auto& state = q->brush.getRenderState();
    if( !last_state || memcmp(last_state, &state, sizeof(state)) != 0 ){
        bbScene->setRenderState(state);
        last_state = &state;
    }
    bbScene->render(q->mesh, ...);
}
```

**Impact:** High - dramatically reduces state changes
**Complexity:** Medium

---

### 8. Pre-calculate Light Direction on CPU

**File:** `blitz3d.gl.cpp` (setLights), `default.glsl` (vertex shader)

**Problem:** The vertex shader calculates `rotationMatrix()` for each light for every vertex:

```glsl
// Current - expensive per-vertex calculation
for( int i=0; i<LS.LightsUsed; i++ ){
    vec3 LightPos = normalize( mat3( bbViewMatrix * LS.Light[i].TForm *
                    rotationMatrix( vec3(1.0,0.0,0.0), 1.5708 ) ) * vec3(0.0,1.0,0.0) );
```

**Solution:** Pre-compute transformed light direction on CPU and pass via UBO:

```cpp
// CPU side - in setLights()
// Pre-transform light direction
vec3 dir = viewMatrix * lightTform * rotatedDir;
ls.data[i].direction[0] = dir.x;
ls.data[i].direction[1] = dir.y;
ls.data[i].direction[2] = dir.z;
```

```glsl
// Shader - use directly
struct BBLightData {
    mat4 TForm;
    vec4 Color;
    vec3 Direction;  // NEW: pre-computed
};

vec3 LightPos = normalize(LS.Light[i].Direction);
```

**Impact:** High - eliminates matrix construction per vertex per light
**Complexity:** Low

---

### 9. Blend State Caching

**File:** `blitz3d.gl.cpp` (setRenderState)

**Problem:** Blend state is set every `setRenderState()` call:

```cpp
switch( blend ){
case BLEND_REPLACE:
    GL( glDisable( GL_BLEND ) );
    break;
case BLEND_ALPHA:
    GL( glEnable( GL_BLEND ) );
    GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
    break;
// ...
}
```

**Solution:** Cache current blend state and only call GL functions when changed:

```cpp
struct {
    int current_blend = -1;
    bool blend_enabled = false;
} blend_cache;

if( blend != blend_cache.current_blend ){
    blend_cache.current_blend = blend;
    switch( blend ){
    case BLEND_REPLACE:
        if( blend_cache.blend_enabled ){
            GL( glDisable( GL_BLEND ) );
            blend_cache.blend_enabled = false;
        }
        break;
    // ...
    }
}
```

**Impact:** Medium
**Complexity:** Low

---

### 10. Texture Parameter Caching

**File:** `blitz3d.gl.cpp` (setRenderState, lines 602-605)

**Problem:** `glTexParameteri` is called for every texture every frame:

```cpp
GL( glTexParameteri( canvas->target, GL_TEXTURE_MAG_FILTER, ... ) );
GL( glTexParameteri( canvas->target, GL_TEXTURE_MIN_FILTER, ... ) );
GL( glTexParameteri( canvas->target, GL_TEXTURE_WRAP_S, ... ) );
GL( glTexParameteri( canvas->target, GL_TEXTURE_WRAP_T, ... ) );
```

**Solution:** Store texture parameters per texture and only call when changed:

```cpp
struct TextureParams {
    GLenum mag_filter = 0;
    GLenum min_filter = 0;
    GLenum wrap_s = 0;
    GLenum wrap_t = 0;
};
std::unordered_map<GLuint, TextureParams> tex_params_cache;

void setTextureParam(GLuint tex, GLenum target, GLenum pname, GLenum value){
    auto& params = tex_params_cache[tex];
    GLenum* cached = nullptr;
    switch(pname){
        case GL_TEXTURE_MAG_FILTER: cached = &params.mag_filter; break;
        // ...
    }
    if( *cached != value ){
        GL( glTexParameteri(target, pname, value) );
        *cached = value;
    }
}
```

**Impact:** Medium
**Complexity:** Low

---

### 11. Remove Unnecessary VAO Unbinding

**File:** `blitz3d.gl.cpp` (render function)

**Problem:** VAO is unbound after every draw call:

```cpp
GL( glBindVertexArray( mesh->vertex_array ) );
// ... draw ...
GL( glBindVertexArray( 0 ) );  // Unnecessary
```

**Solution:** Remove the unbind since the next render call will bind a new VAO anyway.

**Impact:** Low
**Complexity:** Very Low

---

### 12. Explicit Shader Binding Points

**File:** `default.glsl`, `blitz3d.gl.cpp`

**Problem:** Sampler uniform locations are set at runtime with loops and `snprintf`:

```cpp
for( int i=0; i<MAX_TEXTURES; i++ ){
    char sampler_name[20];
    snprintf( sampler_name, sizeof(sampler_name), "bbTexture[%i]", i );
    GLint texLocation = glGetUniformLocation( defaultProgram, sampler_name );
    glUniform1i( texLocation, i );
}
```

**Solution:** Use explicit binding points in GLSL:

```glsl
layout(binding = 0) uniform sampler2D bbTexture[8];
layout(binding = 8) uniform samplerCube bbTextureCube[8];
```

**Impact:** Low - startup optimization only
**Complexity:** Low

---

## Phase 3: Advanced Optimizations (Future)

### 13. Multi-Draw Indirect

**Problem:** Each mesh requires a separate draw call.

**Solution:** Use `glMultiDrawElementsIndirect` to batch multiple draw calls into one.

**Impact:** Very High
**Complexity:** Very High - requires significant restructuring

---

### 14. Persistent Mapped Buffers for Dynamic Meshes

**Problem:** Dynamic meshes (animations) require buffer re-uploads.

**Solution:** Use `GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT` with triple buffering.

**Impact:** High for animated content
**Complexity:** High

---

### 15. Shader LOD (Level of Detail)

**Problem:** All objects use the same complex shader regardless of distance.

**Solution:** Create simplified shader variants:
- **Simple:** No specular, no fog, single texture (for distant objects)
- **Medium:** Simplified specular
- **Full:** Current shader (for nearby objects)

**Impact:** Medium-High
**Complexity:** Medium

---

### 16. Frustum Culling with Bounding Spheres

**File:** `world.cpp`

**Problem:** Objects may be processed even when outside the view frustum.

**Solution:** Pre-compute bounding spheres for meshes and perform quick sphere-frustum tests before any rendering setup.

**Impact:** High for large scenes
**Complexity:** Medium

---

## Implementation Priority (Phase 2)

| Priority | Optimization | Impact | Complexity |
|----------|-------------|--------|------------|
| 1 | Pre-calculate Light Direction (#8) | High | Low |
| 2 | Blend State Caching (#9) | Medium | Low |
| 3 | State Sorting/Batching (#7) | High | Medium |
| 4 | Texture Parameter Caching (#10) | Medium | Low |
| 5 | Remove VAO Unbinding (#11) | Low | Very Low |
| 6 | Explicit Shader Bindings (#12) | Low | Low |
| 7 | Instanced Rendering (#6) | Very High | High |

---

## Benchmark Results

### Initial Benchmark (200 objects, 500 frames)

| Version | Total Time | Avg Frame Time | FPS |
|---------|------------|----------------|-----|
| Original | 45,956 ms | 91.9 ms | 10.88 |
| **Optimized (Phase 1)** | 39,981 ms | 79.9 ms | **12.51** |

**Improvement: +15% FPS**

---

## Testing Plan

1. Run `benchmark.bb` before/after each optimization
2. Test with `primitives.bb` for visual regression
3. Test with complex samples: `castle`, `driver`, `tron`
4. Profile with GPU tools if available (RenderDoc, NVIDIA Nsight)
5. Test on different hardware (integrated vs discrete GPU)

---

## Notes

- GLES builds still use the old `offsetIndices()`/`offsetArrays()` path since `glDrawElementsBaseVertex` is not available
- Point and spot light attenuation code remains commented out (feature incomplete, not performance)
- Consider WebGL/GLES compatibility when implementing advanced features
