# Return Line Engine Spike

Minimal native runtime spike for the PC version. This is our own software pixel renderer and fixed-step game loop; SDL3 provides the window, input events, controller detection and final texture presentation.

## Build

Requirements: C++20 compiler, CMake 3.24+, and the SDL3 development package with its CMake config files.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/SDL3
cmake --build build --config Release
```

On Windows, pass the folder that contains SDL3's `lib/cmake/SDL3` directory as `CMAKE_PREFIX_PATH`. A Windows build has not yet been run from this workspace.

Run the game window:

```sh
./build/return_line_engine
```

Press **Enter** to start. Use **A/D** or **Left/Right** to move; **E** collects supplies or repairs the generator; reach the gate and press **N** to begin the night; **Space** attacks, and **Q** builds a barricade at the gate. A connected gamepad's left stick moves the character. Press **Escape** to quit and **R** to restart after the result.

The short playable loop is: collect two scrap caches and fuel, let Marta finish one visible salvage job, repair the generator, build a barricade, then defend the gate until dawn. This is deliberately one small vertical slice; the tilemap loader and final editor-authored art are still future work.

The headless startup/render check used in CI is:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/return_line_engine --smoke-frame
```

This opens the SDL dummy display, creates the runtime, draws one frame into the custom 640×360 pixel buffer, presents it through SDL and exits. The native Windows build is not yet tested in this workspace.

## Web playtest

The same C++ source can be compiled to WebAssembly with Emscripten and SDL3. GitHub Pages serves that build for link-based playtests; the browser shell supplies touch buttons, while the simulation and renderer remain in C++.
