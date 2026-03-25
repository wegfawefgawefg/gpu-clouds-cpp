# gpu-clouds-cpp

Standalone Vulkan volumetric cloud demo.

## Features

- Vulkan compute cloud renderer
- Real-time single-scatter cloud lighting
- SDL3 windowing and input
- SDL3_ttf HUD overlay
- Dear ImGui controls for resolution, cloud shape, wind, and lighting
- JSON preset load/save via `cloud-settings.json`
- Lightweight bloom and god-ray post controls

## Build

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/gpu-clouds-cpp
```

## VS Code

Open the `gpu-clouds-cpp` folder and press `F5`.

That runs the `build` task, then launches `build/debug/gpu-clouds-cpp` under `gdb`.

## Controls

- `Right Mouse`: capture mouse and look around
- `W A S D`: move
- `Space` / `Shift`: move up / down
- ImGui panel: resolution, cloud, light, and wind settings
