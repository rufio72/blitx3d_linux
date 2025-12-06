# OpenGL Optimizations Plan

This document outlines the planned optimizations for the OpenGL renderer in `src/modules/bb/blitz3d.gl/`.

## Current Issues Analysis

After reviewing `blitz3d.gl.cpp` and `default.glsl`, the following performance issues were identified:

---

## 1. Uniform Location Caching

**File:** `blitz3d.gl.cpp` (lines 317, 381, 401)

**Problem:** `glGetUniformLocation()` is called every frame for projection, view, and world matrices.

```cpp
// Current (inefficient)
void setProj( const float matrix[16] ){
    GLint projLocation = glGetUniformLocation( defaultProgram, "bbProjMatrix" );
    glUniformMatrix4fv( projLocation, 1, GL_FALSE, matrix );
}
```

**Solution:** Cache uniform locations once when the shader is compiled.

```cpp
// Optimized
struct UniformLocations {
    GLint proj_matrix;
    GLint view_matrix;
    GLint world_matrix;
} uniform_locs;

// Cache once in begin() after shader compilation
uniform_locs.proj_matrix = glGetUniformLocation( defaultProgram, "bbProjMatrix" );
// ... etc

void setProj( const float matrix[16] ){
    glUniformMatrix4fv( uniform_locs.proj_matrix, 1, GL_FALSE, matrix );
}
```

**Impact:** High - reduces CPU overhead per frame

---

## 2. UBO Update Optimization

**File:** `blitz3d.gl.cpp` (lines 231-241, 546-556)

**Problem:** Uniform Buffer Objects are updated with `glBufferData()` every frame, which reallocates GPU memory.

```cpp
// Current (inefficient)
glBufferData( GL_UNIFORM_BUFFER, sizeof(ls), &ls, GL_DYNAMIC_DRAW );
```

**Solution:** Use `glBufferSubData()` or persistent mapped buffers.

```cpp
// Optimized
// Allocate once:
glBufferData( GL_UNIFORM_BUFFER, sizeof(ls), nullptr, GL_DYNAMIC_DRAW );
// Update each frame:
glBufferSubData( GL_UNIFORM_BUFFER, 0, sizeof(ls), &ls );
```

**Impact:** Medium - reduces GPU memory churn

---

## 3. Index Buffer Re-upload

**File:** `blitz3d.gl.cpp` (lines 113-121, 627)

**Problem:** `offsetIndices()` and `offsetArrays()` call `glBufferData()` on every render call, re-uploading entire buffers.

```cpp
// Current (inefficient)
void offsetIndices( int offset ){
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, index_buffer );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, (max_tris-offset)*3*sizeof(unsigned int), tris+offset*3, GL_STATIC_DRAW );
}
```

**Solution:** Upload buffer data once in `unlock()`, use `glDrawElementsBaseVertex()` with offsets (already done for non-GLES).

```cpp
// Optimized - upload once
void unlock(){
    glBindBuffer( GL_ARRAY_BUFFER, vertex_buffer );
    glBufferData( GL_ARRAY_BUFFER, max_verts*sizeof(GLVertex), verts, GL_STATIC_DRAW );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, index_buffer );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, max_tris*3*sizeof(unsigned int), tris, GL_STATIC_DRAW );
}

// render() uses offsets without re-upload
void render( BBMesh *m, int first_vert, int vert_cnt, int first_tri, int tri_cnt ){
    glDrawElementsBaseVertex( GL_TRIANGLES, tri_cnt*3, GL_UNSIGNED_INT,
                              (void*)(first_tri*3*sizeof(unsigned int)), first_vert );
}
```

**Impact:** Very High - major performance improvement for complex scenes

---

## 4. Normal Matrix Calculation

**File:** `default.glsl` (line 106)

**Problem:** Matrix inversion in vertex shader is expensive.

```glsl
// Current (inefficient)
mat3 bbNormalMatrix = transpose(inverse(mat3(bbModelViewMatrix)));
```

**Solution:** Precompute normal matrix on CPU and pass as uniform.

```glsl
// Optimized
uniform mat3 bbNormalMatrix;
// ... use directly
```

```cpp
// CPU side - compute once per object
mat3 normalMatrix = transpose(inverse(mat3(modelViewMatrix)));
glUniformMatrix3fv( normal_matrix_loc, 1, GL_FALSE, &normalMatrix[0][0] );
```

**Impact:** High - `inverse()` is expensive, especially for many vertices

---

## 5. Texture State Optimization

**File:** `blitz3d.gl.cpp` (lines 448-454)

**Problem:** All 8 texture slots are unbound at the start of every `setRenderState()` call, even when not needed.

```cpp
// Current (inefficient)
for( int i=0; i<MAX_TEXTURES; i++ ){
    glActiveTexture( GL_TEXTURE0+i );
    glBindTexture( GL_TEXTURE_2D, 0 );
    glActiveTexture( GL_TEXTURE0+i+8 );
    glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
}
```

**Solution:** Track bound textures and only unbind/rebind when changed.

```cpp
// Optimized
struct TextureCache {
    GLuint bound_2d[8] = {0};
    GLuint bound_cube[8] = {0};
} tex_cache;

void bindTexture2D( int slot, GLuint tex ){
    if( tex_cache.bound_2d[slot] != tex ){
        glActiveTexture( GL_TEXTURE0 + slot );
        glBindTexture( GL_TEXTURE_2D, tex );
        tex_cache.bound_2d[slot] = tex;
    }
}
```

**Impact:** Medium - reduces redundant GL state changes

---

## 6. Point/Spot Light Support (Incomplete)

**File:** `blitz3d.gl.cpp` (lines 212-228)

**Problem:** Code for point and spot light attenuation is commented out.

```cpp
// Currently commented out
// glLightfv( GL_LIGHT0+i, GL_CONSTANT_ATTENUATION, light_range );
// glLightfv( GL_LIGHT0+i, GL_LINEAR_ATTENUATION, range );
// glLightfv( GL_LIGHT0+i, GL_SPOT_DIRECTION, dir );
```

**Solution:** Implement proper light attenuation in the shader via UBO.

**Impact:** Feature completion, not performance

---

## 7. Shader Uniform Block Binding

**File:** `blitz3d.gl.cpp` (lines 577-595)

**Problem:** Sampler uniform locations are set every time the shader is rebuilt, using `snprintf` in a loop.

**Solution:** Use explicit binding points in GLSL (layout qualifiers) to avoid runtime setup.

```glsl
// In shader
layout(binding = 0) uniform sampler2D bbTexture[8];
layout(binding = 8) uniform samplerCube bbTextureCube[8];
```

**Impact:** Low - minor startup optimization

---

## Implementation Priority

| Priority | Optimization | Impact | Complexity |
|----------|-------------|--------|------------|
| 1 | Index Buffer Re-upload (#3) | Very High | Medium |
| 2 | Uniform Location Caching (#1) | High | Low |
| 3 | Normal Matrix Calculation (#4) | High | Medium |
| 4 | UBO Update Optimization (#2) | Medium | Low |
| 5 | Texture State Optimization (#5) | Medium | Medium |
| 6 | Shader Uniform Binding (#7) | Low | Low |
| 7 | Point/Spot Light Support (#6) | Feature | High |

---

## Testing Plan

1. Run `primitives.bb` sample before/after each optimization
2. Test with more complex samples: `castle`, `driver`, `tron`
3. Verify no visual regressions
4. Measure FPS improvement where possible
