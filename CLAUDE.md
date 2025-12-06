# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Blitz3D "NG" (Next Generation) is a modernization of the classic Blitz3D game development language, adding cross-platform support (macOS, Linux, Windows) and 64-bit compilation via LLVM. The 32-bit Windows build retains the original Blitz code generation.

## Build Commands

### Prerequisites
- CMake 3.24+ (note: the Makefile says 3.16+ but LLVM 19 requires 3.20+, and root CMakeLists.txt requires 3.24)
- Ninja build system
- LLVM 19 toolchain (either build with `make llvm` or download pre-built from releases)

### Building
```bash
# Build LLVM toolchain (takes significant time)
make llvm

# Build release (default)
make ENV=release

# Build debug
make ENV=debug

# Build just the compiler
make compiler

# Cross-platform builds
make PLATFORM=ios
make PLATFORM=android
make PLATFORM=emscripten
```

The build output goes to `_release/` directory. Build artifacts are in `build/<arch>-<platform>-<env>/`.

### Cleaning
```bash
make clean
```

### Documentation
```bash
make help  # Requires Ruby 3.1.2
```

## Architecture

### Compiler (`src/tools/compiler/`)
The `blitzcc` compiler parses Blitz3D source (.bb files) and generates executables:
- **Tree parser** (`tree/`): Lexer/parser producing AST with decl, stmt, expr, and var nodes
- **Code generation**:
  - `codegen_x86/`: Original x86 32-bit code generator (Windows only)
  - `codegen_llvm/`: LLVM-based code generator for 64-bit and cross-platform
- **Linkers**:
  - `linker_x86/`: Original x86 linker
  - `linker_lld/`: LLD-based linker for LLVM builds
- **JIT**: `jit_orc/` provides LLVM ORC JIT support

### OOP Implementation (`src/tools/compiler/tree/`)
The compiler supports Object-Oriented Programming with these key files:
- **Class declarations** (`decl/class_decl.cpp/h`): Handles `Class`/`End Class` syntax
- **Method calls** (`expr/method_call.cpp/h`): Implements `obj\Method()` and `Super\Method()`
- **Object creation** (`expr/new.cpp/h`): Handles `New ClassName()` with constructor support
- **Parser** (`parser.cpp`): Parses OOP keywords (Class, Method, Self, Super, Extends, Static)
- **Toker** (`toker.cpp`): Tokenizes OOP keywords

#### OOP Features:
- `Class`/`End Class` - Define classes with fields and methods
- `Method`/`End Method` - Define instance methods (receive implicit `self` parameter)
- `Static Method` - Define class methods (no `self` parameter)
- `Self` - Reference to current object instance
- `Super\Method()` - Call parent class method
- `Extends` - Single inheritance
- Methods compiled as global functions: `ClassName_MethodName(self, args...)`

### Runtime Modules (`src/modules/bb/`)
Platform-agnostic APIs with platform-specific implementations:
- **Core**: `blitz` (runtime core), `math`, `string`, `bank` (memory buffers)
- **I/O**: `filesystem`, `stream`, `stdio`, `sockets`
- **Graphics**: `graphics` (2D), `graphics.gl` (OpenGL), `blitz3d` (3D engine), `blitz3d.gl` (OpenGL 3D), `pixmap`
- **Audio**: `audio` with backends (`audio.fmod`, `audio.openal`)
- **Input**: `input` with platform backends
- **Platform runtimes**: `runtime.sdl`, `runtime.html`, etc.

Platform-specific variants use dot notation (e.g., `filesystem.posix`, `filesystem.windows`).

### Runtime Libraries (`src/runtime/`)
- `base/`: Core runtime linked into executables
- `opengl/`: OpenGL renderer (primary graphics backend)
- `test/`: Test runtime

### Platform Configuration
The root `CMakeLists.txt` detects and sets:
- `BB_PLATFORM`: macos, linux, win32, win64, ios, android, emscripten, nx
- `BB_ARCH`: x86_64, arm64, i686, wasm32
- `BB_ENV`: debug, release, test
- `BB_TRIPLE`: Target triple (e.g., `x86_64-linux-gnu`)

## Testing

Test files are in `test/` directory as `.bb` Blitz3D source files:
- `test/all.bb`: Main test suite
- `test/language/`: Language feature tests
- `test/modules/`: Module-specific tests

OOP test samples in `_release/samples/oop/`:
- `test_super.bb`: Tests Super keyword
- `test_inheritance.bb`: Tests method inheritance
- `test_multi_inheritance.bb`: Tests multi-level inheritance

## Key Files

- `Makefile`: Primary build entry point
- `CMakeLists.txt`: CMake configuration
- `deps/`: Third-party dependencies (submodules)
- `_release/`: Output directory with built binaries and help files
- `CHANGES.md`: Document of changes from original Blitz3D

## Android Build

### Prerequisites
```bash
# Install Android SDK Command Line Tools, then:
sdkmanager "ndk;26.1.10909125" "build-tools;34.0.0" "cmake;3.22.1" "platforms;android-34" "platforms;android-24"
```

### Environment Variables (REQUIRED)
```bash
export ANDROID_HOME=/home/rufio72/Android/Sdk
export ANDROID_NDK_HOME=/home/rufio72/Android/Sdk/ndk/26.1.10909125
export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
```

### Build Android Runtime
```bash
cd /home/rufio72/b3d
cmake -G Ninja -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_PLATFORM=24 \
  -DANDROID_ABI=arm64-v8a \
  -DARCH=arm64 \
  -DBB_PLATFORM=android
ninja -C build-android
```

### Compile BB to APK
```bash
export blitzpath=/home/rufio72/b3d/_release
blitzcc -target arm64-android-24 -o game.apk game.bb
```

### Important Android Notes
- **Toolchain location**: `_release/bin/arm64-android-24/`
- **JAR files**: SDL.jar and Runtime.jar in `_release/bin/arm64-android-24/lib/`
- **minSdk**: 24 (Android 7.0), **targetSdk**: 34 (Android 14)
- **-mno-outline-atomics**: Added in CMakeLists.txt to fix `__aarch64_swp4_acq_rel` symbol errors on older Android devices

### Android Linker (linker_lld.cpp)
- Extracts API level from version string (e.g., "android-24" -> "24")
- Supports arch "arm64" and "arm64-v8a"
- Uses NDK clang 17 paths (not old 11.0.5 paths)

### Android Packager (package_apk.cpp)
- Uses build-tools 34.0.0
- Uses `d8` instead of deprecated `dx` for DEX
- Uses android-34/android.jar for Java compilation
- Default package: `com.blitz3dng.game`
- Default app name: `Blitz3D Game`
