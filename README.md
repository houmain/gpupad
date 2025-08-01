
# GPUpad

<p>
<a href="https://github.com/houmain/gpupad/actions/workflows/build.yml">
<img alt="Build" src="https://github.com/houmain/gpupad/actions/workflows/build.yml/badge.svg"/></a>
<a href="https://github.com/houmain/gpupad/issues">
<img alt="Issues" src="https://img.shields.io/github/issues-raw/houmain/gpupad.svg"/></a>

<a href="#features">Features</a> |
<a href="#screenshots">Screenshots</a> |
<a href="#introduction">Introduction</a> |
<a href="#installation">Installation</a> |
<a href="#building">Building</a> |
<a href="https://github.com/houmain/gpupad/blob/main/CHANGELOG.md">Changelog</a>
</p>

A lightweight editor for GLSL and HLSL shaders and a fully-featured IDE for developing GPU based algorithms.

## Features

* OpenGL and Vulkan renderer.
* Decent source editor with automatic indentation, brace highlighting, rectangular selection&hellip;
* GLSL, HLSL and JavaScript syntax highlighting with basic auto completion.
* Possibility to evaluate shader programs with completely customizeable input and render state.
* Continuous validation of standalone shader and script files.
* Automatically defined printf function for printf-debugging.
* JavaScript expressions to define uniform input.
* Custom actions to extend the functionality.
* Dumping of preprocessed source, SPIR-V and glslang AST.
* Reading and writing of image files ([KTX](https://github.com/KhronosGroup/KTX-Software) and DDS for 3D/Array textures, block compressed textures, cube maps&hellip;).
* Streaming video files to textures (only when built with the optional dependency `Qt6Multimedia`).
* Editor for structured binary files.
* Advanced hot reloading of externally modified files.
* [Base16](https://github.com/chriskempson/base16-schemes-source) theme support.
* Sample sessions in the Help menu.

## Screenshots

<a href="screenshot1.png"><img  style="vertical-align: top" src="screenshot1.png" height="280"></a> &nbsp;
<a href="screenshot2.png"><img src="screenshot2.png" height="380"></a>

## Introduction

### Getting Started
To get started, you can open and play around with the sample sessions in the *Help* menu.

The sample sessions can also be used as templates - saving a session As... copies all the dependencies to the new location.

### Session
In order to try out the shaders, the session allows to define draw and compute calls, together with the pipeline state and data the programs should operate on.

It can be populated with items from the *Session* menu or the context menu. Undo/redo, copy/paste and drag/drop should work as expected (also between multiple instances).
It is even possible to drag the items to and from a text editor (they are serialized as JSON).

### Evaluation
The session can be evaluated manually *[F6]*, automatically whenever something relevant changes *[F7]* or steadily *[F8]*, for animations.
All items which contributed to the last evaluation are highlighted.

### Items
The items of a session pretty much correspond the concepts known from writing OpenGL or Vulkan applications:

- **Call** -
Most prominently are the draw, compute and ray trace calls. Whenever the session is evaluated, all active calls are evaluated in consecutive order. They can be de-/activated using the checkbox.
The elapsed time of each call is output to the *Message* window (measured using GPU timer queries).

- **Program** -
Consists of one or multiple shaders, which are linked together, so they can be used by draw or compute calls.

- **Texture** -
All kind of color, depth or stencil textures can be created. They serve as sample sources, image in- and outputs and target attachments. They can be backed by files.

- **Target** -
Specifies where draws calls should render to. Multiple images can be attached. Depending on the attached image's type, different render states can be configured.

- **Binding** -
Allows to bind data to a program's uniforms, samplers, images and buffers. A binding affects all subsequent calls, until it is replaced by a binding with the same name, or the scope ends (see *Groups*). The name of a binding needs to match the name of a program's binding point.

- **Buffer** -
Buffer blocks define the structure of a region within a binary. They consist of rows with multiple fields of some data type. Buffers can be backed by binary files.

- **Stream** -
Serves as the input for vertex shaders. A stream consists of multiple attributes, which get their data from the referenced buffer blocks.

- **Group** -
Allows to structure more complex sessions. They open a new scope unless *inline scope* is checked. Items within a scope are not visible for items outside the scope (they do not appear in the combo boxes).

- **Script** -
Allows to define JavaScript functions and variables in script files, which can subsequently be used in uniform binding expressions.
Scripts can also be used to dynamically populate the session and generate buffer and texture data.
There is one JavaScript state for the whole session and the scripts are evaluated in consecutive order (*Group* scopes do not have an effect).

- **Acceleration Structure** -
Allows to define the instances and geometry for ray tracing calls.

## Scripting

Initial documentation of the available script objects:

:warning: Please use the discussions section for requesting additional information or functionality.

### App

- `frameIndex: Number`
- `keyboard: Keyboard`
- `mouse: Mouse`
- `session: Session`
- `callAction(id, arguments...) -> result`
- `enumerateFiles(pattern) -> [filename]`
- `loadLibrary(filename) -> Library?`
- `openEditor(filename, title?) -> Editor?`
- `openFileDialog(pattern) -> filename: String?`
- `readTextFile(filename) -> String?`
- `writeTextFile(filename, String) -> Bool`
- `writeBinaryFile(filename, Data) -> Bool`

### Session
- `name: String`
- `items: [Item]`
- `selection: [Item]`
- `findItem(ItemIdent, origin: ItemIdent?, subItems: Bool?) -> Item?`
- `findItems(ItemIdent, origin: ItemIdent?, subItems: Bool?) -> [Item]`
- `getParentItem(ItemIdent) -> Item?`
- `insertItem(parent: ItemIdent?, object) -> Item`
- `insertItemAfter(sibling: ItemIdent, object) -> Item`
- `insertItemBefore(sibling: ItemIdent, object) -> Item`
- `deleteItem(ItemIdent)`
- `clearItems(ItemIdent)`
- `replaceItems(parent: ItemIdent, [Object])`
- `openEditor(ItemIdent) -> Editor?`
- `setBlockData(ItemIdent, Data)`
- `setBufferData(ItemIdent, Data)`
- `setScriptSource(ItemIdent, Data)`
- `setShaderSource(ItemIdent, Data)`
- `setTextureData(ItemIdent, Data)`
- `processShader(shader: ItemIdent, type: String) -> String/Data`
- `getBufferHandle(ItemIdent) -> Number`
- `getTextureHandle(ItemIdent) -> Number`

### Editor

- `viewportSize: [width, height]`

### Mouse

- `button: [State]` - The state of each mouse button (0 = Up, 1 = Down, 2 = Pressed, -1 = Released).
- `coord: [x, y]`
- `delta: [x, y]`
- `fragCoord: [x, y]`
- `prevCoord: [x, y]`
- `prevFragCoord: [x, y]`
- `editorSize: [width, height]`

### Keyboard

- `keys: [State]` - The state of each key (0 = Up, 1 = Down, 2 = Pressed, -1 = Released).

## Installation

### Arch Linux and derivatives

An up to date build can be installed from the [AUR](https://aur.archlinux.org/packages/gpupad-git/).

### Windows and other Linux distributions

A portable build can be downloaded from the [latest release](https://github.com/houmain/gpupad/releases/latest) page.

## Building

A C++20 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.
It depends on the following libraries, which can be installed using a package manager like [vcpkg](https://github.com/microsoft/vcpkg/) or by other means:

- [Qt6](https://doc.qt.io/qt-6/get-and-install-qt.html)
- [KDGpu](https://github.com/houmain/KDGpu) (automatically pulled as submodule)
- [glslang](https://github.com/KhronosGroup/glslang)
- [spirv-cross](https://github.com/KhronosGroup/SPIRV-Cross)
- [libktx](https://github.com/KhronosGroup/KTX-Software)
- [vulkan-memory-allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [spdlog](https://github.com/gabime/spdlog)
- [OpenImageIO](https://github.com/AcademySoftwareFoundation/OpenImageIO) (optional)

### Building on Debian Linux and derivatives:

```bash
# install dependencies
sudo apt install build-essential git cmake qtdeclarative6-dev libdrm-dev pkg-config libxcb*-dev libx11-dev libxrandr-dev

# check out source
git clone --recurse-submodules https://github.com/houmain/gpupad
cd gpupad

# install vcpkg
git clone --depth=1 https://github.com/microsoft/vcpkg.git
vcpkg/bootstrap-vcpkg.sh -disableMetrics

# install additional dependencies using vcpkg
vcpkg/vcpkg install vulkan "ktx[vulkan]" glslang spirv-cross vulkan-memory-allocator spdlog

# build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build -j4
```

### Building on Windows:

```bash
cmd

# install Qt6
# https://doc.qt.io/qt-6/get-and-install-qt.html

# check out source
git clone --recurse-submodules https://github.com/houmain/gpupad
cd gpupad

# install vcpkg
git clone --depth=1 https://github.com/microsoft/vcpkg.git
vcpkg\bootstrap-vcpkg -disableMetrics

# install dependencies using vcpkg
vcpkg\vcpkg install vulkan "ktx[vulkan]" glslang spirv-cross vulkan-memory-allocator spdlog

# generate Visual Studio solution
cmake -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.9.0\msvc2022_64 -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake

# copy all Qt dependencies to Debug directory
cmake --install build --config Debug --component Application --prefix %CD%\Debug

# open solution
build\gpupad.sln
```

## License

GPUpad is released under the GNU GPLv3. Please see `LICENSE` for license details.
