# Graphics Assignment 2 — OpenGL Scene

A C++17 / OpenGL 3.3 Core real-time scene featuring a third-person character controller, orbit-follow camera, terrain + texturing, HDR-based sky (day/night), animated water, a fire particle system, and an imported GLB model.

Module: **graphic**

---

## Features

### Interaction
- **W / A / S / D** — move the player (movement relative to camera heading)
- **Right Mouse Button (drag)** — orbit camera around the player
- **Mouse Wheel** — zoom in / out
- **ESC** — quit

### Rendering
- **Terrain**: generated mesh terrain with tiled diffuse textures
- **Lighting**: directional + ambient lighting, driven by time-of-day
- **Sky**: day/night HDR environment maps, converted to cubemap at runtime
- **Water**: animated surface (shader-driven waves)
- **Particles**: billboard fire particles (lifetime + blending)
- **Imported model**: `boat.glb` (GLB / glTF 2.0)

---

## Repository Layout

### Source
```
src/
  glad.c
  main.cpp
  stb_image.h
  stb_image_impl.cpp
  tiny_gltf.h
  json.hpp

  core/
    Input.h
    Input.cpp

  environment/
    Environment.h/.cpp
    Sky.h/.cpp
    Sun.h/.cpp
    TimeOfDay.h/.cpp
    Water.h/.cpp
    Particles.h/.cpp

  render/
    Shader.h/.cpp
    Mesh.h/.cpp
    Model.h/.cpp
    LightingSystem.h/.cpp
    TextureUtils.cpp

  scene/
    Camera.h/.cpp
    Player.h/.cpp
    Terrain.h/.cpp
    SceneNode.h
    Renderable.h
    Transform.h
```

### Assets
```
assets/
  models/
    boat.glb

  shaders/
    basic.vert / basic.frag
    terrain.vert / terrain.frag
    water.vert / water.frag
    particle.vert / particle.frag
    model.vert / model.frag
    skybox.vert / skybox.frag
    equirect2cube.vert / equirect2cube.frag

  textures/
    rocky/ rocky_terrain_02_diff_2k.jpg
    sand/  sandy_gravel_02_diff_2k.jpg
    sky/
      syferfontein_0d_clear_puresky_4k.hdr
      qwantani_night_puresky_4k.hdr
```

---

## Requirements

- Windows 10/11
- GPU + driver supporting **OpenGL 3.3 Core**
- **CMake ≥ 3.20**
- C++17 compiler (MSVC recommended)

### Third-party libraries
- GLFW (window/input)
- GLM (math)
- GLAD / stb_image / tinygltf / nlohmann json are included in-source

If you use vcpkg locally, it can be used to provide GLFW/GLM.

---

## Build

### CMake (Release)
From the repository root:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### (Optional) vcpkg toolchain
If your environment requires explicitly specifying the vcpkg toolchain:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=<VCPKG_ROOT>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

---

## Run

This project loads shaders/textures/models at runtime using relative paths under `assets/`.

- Ensure the **working directory** is set so the program can find:
    - `assets/shaders/`
    - `assets/textures/`
    - `assets/models/`

If launching from an IDE, set the Run/Debug working directory accordingly (typically the project root, or wherever `assets/` is located).

---

## Submission Packaging

Recommended layout for marker-friendly execution:

```
Submission/
  Graphic_Assignment2.exe
  assets/
    models/
    shaders/
    textures/
  README.md
```

Notes:
- Do not rely on absolute paths.
- Do not omit `.hdr`, `.jpg`, `.glb`, or any shader files.

Executable name for submission:
- **Graphic_Assignment2.exe**

---

## Asset Attribution

- **HDR skies + terrain textures**: Poly Haven  
  Files:
    - `assets/textures/sky/syferfontein_0d_clear_puresky_4k.hdr`
    - `assets/textures/sky/qwantani_night_puresky_4k.hdr`
    - `assets/textures/rocky/rocky_terrain_02_diff_2k.jpg`
    - `assets/textures/sand/sandy_gravel_02_diff_2k.jpg`

- **Boat model (Minecraft-style)**: Sketchfab  
  File:
    - `assets/models/boat.glb`

Source sites (for reference):
- https://polyhaven.com/
- https://sketchfab.com/

---

## Notes / Limitations

- Visual output (exposure/brightness) may vary slightly across GPUs/drivers due to HDR environment maps and tone mapping defaults.
# Graphic_Assignment2
