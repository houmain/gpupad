[![Build status](https://ci.appveyor.com/api/projects/status/d1frxd63iohaqcto/branch/master?svg=true)](https://ci.appveyor.com/project/houmaster/gpupad/branch/master)

GPUpad
======
Aims to be a lightweight editor for GLSL shaders of all kinds and a fully-featured IDE for developing GPU based algorithms.

Features
--------
* Cross platform and efficient.
* Decent source editor with automatic indentation, rectangular selection&hellip;
* GLSL and JavaScript syntax highlighting with basic auto completion (of built-in functions and constants).
* Continuous validation of standalone shader files (by compiling them using OpenGL).
* Possibility to evaluate shader programs with completely customizeable input and OpenGL state.
* Automatically defined printf function for printf-debugging.
* JavaScript expressions and scripts to define uniform input.
* Reading and writing of image files ([KTX](https://github.com/KhronosGroup/KTX-Software) and DDS for 3D/Array textures, block compressed textures, cube maps&hellip;).
* Streaming video files to textures.
* Editor for structured binary files.
* Sample sessions in the Help menu.

Screenshots
-----------
<a href="screenshot2.png"><img src="screenshot2.png" height="380"></a> &nbsp;
<a href="screenshot1.png"><img src="screenshot1.png" height="280"></a>

Session
-------
In order to try out the shaders, the session allows to define draw and compute calls, together with the pipeline state and data the programs should operate on.

It can be populated with items from the *Session* menu or the context menu. Undo/redo, copy/paste and drag/drop should work as expected (also between multiple instances).
It is even possible to drag the items to and from a text editor (they are serialized as JSON).

### Getting Started
To get started, you can open and play around with the sample sessions in the *Help* menu. They can also be used as templates - saving the session As... copies all the dependencies to the new location.

### Evaluation
The session can be evaluated manually *[F6]*, automatically whenever something relevant changes *[F7]* or steadily, for animations *[F8]*.
All items which contributed to the last evaluation are highlighted.

### Items
The items of a session pretty much correspond the concepts known from writing OpenGL applications:

- **Call** -
Most prominently are the draw and the compute calls. Whenever the session is evaluated, all active calls are evaluated in consecutive order. They can be de-/activated using the checkbox.
The elapsed time of each call is output to the *Message* window (measured using OpenGL timer queries).

- **Program** -
Consists of one or multiple shaders, which are linked together, so they can be used by draw or compute calls.

- **Texture** -
All kind of color, depth or stencil textures can be created. They serve as sample sources, image in- and outputs and target attachments. They can be backed by files. The texture's images can also be explicitly set (for custom mip levels, cube maps&hellip;).

- **Target** -
Specifies where draws calls should render to (it corresponds to an OpenGL *FBO*). Multiple images can be attached. Depending on the attached image's type, different OpenGL states can be configured.

- **Binding** -
Allows to bind data to a program's uniforms, samplers, images, buffers and to select shader subroutines. A binding affects all subsequent calls, until it is replaced by a binding with the same name, or the scope ends (see *Groups*). The name of a binding needs to match the name of a program's binding points.

- **Buffer** -
Buffer blocks define the structure of a region within a binary. It consists of rows with multiple fields of some data type. Buffers can be backed by a binary file.

- **Stream** -
Serves as the input for vertex shaders. A stream consists of multiple attributes, which get their data from the referenced buffer blocks.

- **Group** -
Allows to structure more complex sessions. They open a new scope unless *inline scope* is checked. Items within a scope are not visible for items outside the scope (they do not appear in the combo boxes).

- **Script** -
Allows to define JavaScript functions and variables in script files, which can subsequently be used in uniform binding expressions.
There is one JavaScript state for the whole session and the scripts are evaluated in consecutive order (*Group* scopes do not have an effect).

Building
--------
A C++17 conforming compiler and [Qt5](https://www.qt.io/) is required. A script for the
[CMake](https://cmake.org) build system is provided.

**Arch Linux** users can install it from the [AUR](https://aur.archlinux.org/packages/gpupad-git).

License
-------
It is released under the GNU GPLv3. Please see `LICENSE` for license details.
