# Blitz3D - TODO and Improvements

This document lists potential optimizations and improvements identified in the codebase.

---

## HIGH PRIORITY

### 1. Empty/Stub Source Files
Files that are empty or contain minimal code but are still in the build system:

- `src/tools/compiler/compiler.cpp` (empty)
- `src/tools/compiler/compiler.h` (empty)
- `src/tools/compiler/std.cpp` (2 lines only)
- `src/modules/bb/blitz3d/ms3drep.cpp` (empty)
- `src/modules/bb/blitz3d/loader.h` (empty)
- `src/modules/bb/audio.fmod/audio.fmod.h` (empty)

**Action:** Remove from build system or delete if not required.

---

### 2. Obsolete Files to Remove
Legacy backup files that should be cleaned up:

- `src/tools/compiler/block.h_old`
- `src/tools/compiler/tree/semant.old`
- `src/tools/compiler/tree/decl.old`
- `src/modules/bb/blitz3d/md2model_bak.cpp`

**Action:** Delete these files.

---

### 3. Unsafe String Operations
Legacy unsafe string functions used in the codebase:

**Files affected:**
- `src/modules/bb/system/system.cpp` - uses `strcpy()`
- `src/modules/bb/blitz3d/surface.cpp`
- `src/modules/bb/blitz3d/sprite.cpp`
- `src/modules/bb/blitz3d/terrainrep.cpp`
- `src/modules/bb/blitz3d.gl/blitz3d.gl.cpp`

**Issue:** `strcpy`, `strdup`, `sprintf` are buffer overflow risks.

**Action:** Replace with `strncpy`, `snprintf`, or C++ `std::string`.

---

## MEDIUM PRIORITY

### 4. Memory Management in Graphics Rendering
No object pooling for frequently allocated objects.

**Files:**
- `src/modules/bb/blitz3d.gl/blitz3d.gl.cpp` (lines 100-102)
- `src/modules/bb/graphics.gl/canvas.cpp` (multiple instances)

**Code example:**
```cpp
// Line 100: TODO comment exists
// "TODO: this should probably come from a pool"
if( !verts ) verts=new GLVertex[max_verts];
if( !tris ) tris=new unsigned int[max_tris*3];
```

**Impact:** Frame-by-frame allocations, potential memory fragmentation.

**Action:** Implement vertex/index buffer pooling.

---

### 5. Legacy IDE Components (Windows Only)
Code that only builds with MSVC and is not used on Linux/macOS:

- `src/tools/legacy/blitzide/` (~2738 lines)
- `src/tools/legacy/debugger/` (~41 source files)

**Current CMakeLists.txt:**
```cmake
IF(BB_MSVC AND NOT BB_CROSSCOMPILE)
  add_subdirectory(legacy/blitzide)
  add_subdirectory(legacy/debugger)
ENDIF()
```

**Action:** Consider removing or archiving separately.

---

### 6. Incomplete VR/OVR Module
**File:** `src/modules/bb/runtime.vrapi/runtime.vrapi.cpp`

Multiple unimplemented functions (lines 557-571):
- `numGraphicsDrivers()` - returns 0, TODO
- `graphicsDriverInfo()` - TODO
- `numGraphicsModes()` - returns 0, TODO
- `graphicsModeInfo()` - TODO
- `windowedModeInfo()` - TODO

**Action:** Complete implementation or document as unsupported.

---

### 7. Graphics Module Code Duplication
Similar code patterns duplicated between:
- `src/modules/bb/graphics.gl/` (2D graphics)
- `src/modules/bb/blitz3d.gl/` (3D graphics)

Duplicated patterns:
- Shader compilation and error handling
- Texture state caching logic
- Matrix handling code

**Action:** Extract common infrastructure into shared utility module.

---

### 8. Build System Hard-coded Paths
**File:** `CMakeLists.txt` line 153

```cmake
# TODO: should find a way to derive this...
/usr/lib/x86_64-linux-gnu
```

Hard-coded library paths don't work across all Linux distributions.

**Action:** Use `find_package()` or `pkg-config` for proper path detection.

---

### 9. TODO/FIXME Comments in Codebase
Files with unresolved TODO comments (23 total):

**Critical files:**
- `linker_lld/linker_lld.cpp` - 8 TODOs (platform-specific linker issues)
- `main.cpp` - architecture directory detection
- `codegen_llvm.cpp` - code generation workarounds

**Action:** Review and prioritize each TODO.

---

## LOW PRIORITY

### 10. Incomplete Module Documentation
**Statistics:**
- Total modules: 42
- Modules with docs: 7 (16.7%)
- Missing documentation: 35 modules (83.3%)

**Modules needing docs:**
- `graphics.gl`, `blitz3d.gl` (rendering APIs)
- `audio.fmod`, `audio.openal` (audio backends)
- `sockets`, `stream` (I/O APIs)
- `pixmap`, `bank` (data structures)

**Action:** Add API documentation to critical modules.

---

### 11. Custom Allocator Inconsistency
Mixed use of `d_new`/`d_delete` macros and standard `new`/`delete`:

**Files:**
- `src/modules/bb/graphics.gl/canvas.cpp`
- `src/modules/bb/graphics.gl/graphics.gl.cpp`
- `src/modules/bb/blitz3d.gl/blitz3d.gl.cpp`

**Action:** Standardize on one allocation pattern and document.

---

### 12. Code Style/Density Issues
**File:** `src/tools/compiler/main.cpp`
- 719 lines total
- Mixes platform-specific code (WIN32, POSIX, MSVC, GCC)
- Could be split into smaller modules

**Action:** Refactor into separate platform-specific files.

---

## QUICK WINS (Easy to Fix)

1. **Remove empty source files from build** - 30 minutes
2. **Delete `.old`, `.bak` files** - 15 minutes
3. **Add compiler warning for unsafe string operations** - 30 minutes
4. **Verify legacy IDE is disabled on non-MSVC** - 15 minutes

---

## SUMMARY TABLE

| Issue | Severity | Effort | Impact |
|-------|----------|--------|--------|
| Empty files | HIGH | Low | Build clutter |
| Obsolete files | HIGH | Low | Repository clutter |
| Unsafe strings | HIGH | Medium | Security |
| Memory pooling | MEDIUM | Medium | Performance |
| Legacy IDE | MEDIUM | Low | Maintenance |
| VR module | MEDIUM | High | Feature gap |
| Code duplication | MEDIUM | Medium | Maintainability |
| Build paths | MEDIUM | Medium | Portability |
| TODOs | MEDIUM | Low | Clarity |
| Documentation | LOW | Medium | Developer experience |
| Allocators | LOW | Low | Consistency |
| Code style | LOW | Medium | Readability |

---

*Last updated: December 2024*
