# Blitz3D

This compiler is a fork of the Blitz3D NG project, which itself was a fork of the original Blitz3D by the late Mark Sibly.
My goal is to modernize Blitz3D and make it compile on Linux, Windows, Mac and Android. Android compilation is really important to me. The other platforms I see more as a dress rehearsal.
Blitz3D will have object-oriented syntax and use GCC as its compiler. You'll be able to easily download ready-to-use versions for Linux and Windows.
Over time I'd like to introduce an alternative C-like syntax (for those who, like me, prefer it).
Yes, I know — reviving a compiler from 2000 with goto and gosub might seem a bit old-fashioned. But I think now that AI is learning to write code, this compiler could actually become a better solution than all those engines where you have to learn complex environments, study strategies and whatnot.
Anyway, even if nobody else ends up caring about this, I can tell you that working on this project just makes me feel good.

## Highlights of this fork

### Object-Oriented Programming

A full OOP extension of the classic language — see the [OOP guide](docs/OOP.md) and [CHANGES.md](CHANGES.md) for details and examples:

- **Classes and methods**: `Class … End Class`, `Method … End Method`; methods compile to plain functions (`ClassName_MethodName`), no hidden runtime.
- **Constructors and destructors**: `Method constructor()` runs on `New`, `Method destructor()` on `Delete` — both inherited through the class chain.
- **Single inheritance** with `Extends`, method inheritance included.
- **`Self` / `This` and `Super`**: current instance and parent-class calls, with dot syntax (`Self.field`, `Self.Method()`) as an alternative to the classic backslash.
- **`Static Method`**: class-level functions without an instance.
- **Access control that actually holds**: `Private` / `Protected` / `Public` are enforced against the class that owns the member — on field access, on method calls, on `Super`, and even on direct mangled calls.
- **Contextual keywords**: classic sources that use `class`, `method`, `static`, … as variable or function names keep compiling. The OOP words are only reserved where OOP syntax can appear.
- Method resolution is **static** (no vtable) — documented, deliberate, and fast.

### Compiler

- **Two modern backends**: a C/GCC backend (the default — Blitz3D programs are compiled through generated C, so they build wherever GCC does) and an **LLVM 19** backend with LLD linking and ORC JIT for 64-bit targets. The original x86 code generator is kept for legacy 32-bit Windows builds.
- **True 64-bit integers**: literals, constants and constant folding work at 64 bits (`Local x = 5000000000` just works); out-of-range literals are a compile error instead of silent truncation.
- **Debug runtime checks** (`-d`): calling a method on a `Null` or deleted object reports *"Object does not exist"* instead of crashing.
- **Robust command handling**: paths with spaces and shell metacharacters work throughout the build pipeline.
- Compile-time existence checks for `LoadImage`/`LoadMesh`/… arguments, case-insensitive `Include` and asset paths (Windows-style sources run on Linux unchanged).
- The whole compiler went through a **22-finding code review in October 2026**: every critical, important and minor finding was fixed and the fixes are guarded by the test suite (**503 passing tests**).

### Platforms

- **Linux, Windows, macOS** — one codebase, CMake + Ninja build.
- **Android**: `blitzcc -target arm64-android-24 -o game.apk game.bb` produces an installable APK (SDK 34, NDK 26); assets are bundled automatically from an `assets/` folder or by media-type whitelist.
- iOS, Emscripten and Switch build targets are scaffolded in the build system.

### Runtime and graphics

The renderer was rebuilt on **OpenGL + SDL2** (DirectX and DirectPlay removed):

- **Per-pixel lighting** in the fragment shader, **tangent-space normal mapping** and **parallax-occlusion mapping** (height in the normal-map alpha).
- **Dynamic shadow maps**: perspective shadow maps for spot/point lights and omnidirectional (cube) shadow maps for point lights.
- **Cubemap reflections** with a correct per-face render pipeline.
- **GPU skinning** in the vertex shader, with per-frame pose caching.
- **Model import via assimp**: glTF/GLB, OBJ, DAE, FBX — including skeletal animations and embedded textures.
- **4x MSAA** on the default framebuffer (with graceful fallback), DPI-aware windows (`DPIScaleX/Y`), `ScreenWidth`/`ScreenHeight`.
- Serious performance work: redundant-state elimination, draw-call coalescing, deferred mesh uploads, cheap normal matrices, `TrisRendered()`/`DrawsRendered()` counters — the benchmark scene runs in ~0.2 ms/frame on a mid-range GPU.
- **OpenAL audio** backend with working channel controls; FreeImage 3.17; pixel-perfect image collisions; `ExecFile` implemented on Linux/macOS; memory-safety hardening across banks, streams and strings.

### IDE and tools

- Cross-platform IDE with error highlighting, two-phase compile output (Blitz3D + GCC tabs), DPI scaling and a 64-bit build.
- Debugger with runtime error reporting (programs are compiled with `-d` from the IDE).
- `docgen` documentation generator (no Ruby dependency) — the full classic help plus the new guides ship in [`_release/help`](_release/help).

## Current status

**21-10-2026** — The compiler passed a full code review: all 22 findings (3 critical, 9 important, 10 minor) are fixed, verified and covered by the test suite, which now stands at 503 passing tests. Both the GCC and the LLVM backends build and run the OOP test programs.

**23-12-2025** — All the examples work. There might be something I haven't noticed, but overall they work.
There will probably be some bugs in texture handling, or something I haven't noticed, that slipped past me, but overall it seems like the first part of the work is done.
Then I'll try to get the tank game working. I consider that a test for the compiler's reliability.
When I manage to get that part working, I'll start considering the compiler ready.
Then I'll move on to creating the distributable version for Linux.

## Documentation

The original Blitz3D help is available in the [\_release/help](_release/help)
directory in HTML form, together with the new OOP guide.
Documentation is generated from Markdown files using the built-in `docgen`
tool. The full list of changes from the original Blitz3D lives in
[CHANGES.md](CHANGES.md).

## Roadmap

- Distributable, ready-to-use packages for Linux and Windows.
- The tank game sample running end to end (the reliability test).
- An alternative C-like syntax for the language.
- Remaining known issues are tracked at the bottom of the code review document
  (field null-checks in debug on the C backend, OOP on the legacy 32-bit x86
  code generator).

## License

```
The zlib/libpng License

Copyright (c) 2013 Blitz Research Ltd

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
```
