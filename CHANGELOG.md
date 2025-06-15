# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Version 2.7.0] - 2025-06-16

### Added

- Added session script object findItem and findItems.
- Added session script object openEditor.
- Added editor script object.
- Added custom action to compile all shaders to Spir-V.
- Allow to manually resize rows in message list.
- Improved JSON shader interface output.
- Logging device name and type Vulkan is initialized on.

### Changed

- Using glslang preprocessor to substitute #includes when extension is enabled.
- Replaced session script object getShaderInterface with processShader.
- Removed session script object item(), integrated in findItem().
- Session script object setBufferData/setBlockData also set block rowCount.
- Session script object replaceItems only reuses items without subitems.
- Center source editor on line which is jumped to.
- Improved shader type deduction on file drop.
- Hiding automap bindings/locations when using OpenGL renderer.
- Automatically enable zoom-to-fit when texture has same size as editor viewport
- Made Shadertoy sample resize to viewport size.
- Removed printfEnabled.

### Fixed

- Fixed swap textures call to only swap device resources.
- Fixed reset evaluation stopping video.
- Not reusing last working shader program on reset.
- Do not attempt to generate mipmaps from multisample target.
- Ensure buffer reference is not obtained before buffer is recreated.
- Evaluate each uniform binding only once.
- Improved error messages when #include is not found.
- Initialize source type of new script editors.
- Preserve mouse coordinates on reset.
- Preselect correct filter in save dialog.
- Prevent script messages from piling up.

## [Version 2.6.1] - 2025-05-07

### Added

- Allow to hide specific toolbar icons.
- Added fullscreen and collapse menu bar actions to context menu.
- Added optional parameter to AppScriptObject openEditor to set QML title.
- Writing glslang and spirv-cross version in about dialog.

### Fixed

- Fixed crash when Vulkan is not available.
- Ensure Vulkan is using same adapter as OpenGL.
- Validating that program has a shader.
- Fixed calling custom actions from render thread.
- Fixed some runtime warnings.
- Unloading caches when closing a session.

## [Version 2.6.0] - 2025-05-01

### Added

- Added session script object `getShaderInterface`.
- Added session script object `insertBeforeItem` / `insertAfterItem`.
- Added session script object `parentItem`.
- Added second shadertoy sample.
- Implemented automatic texture resizing on upload.

### Changed

- Moved `selection` from app to session script object.
- Keyboard script object `keys` array can be indexed by ASCII code.
- Synchronize shader type in session with when saving editor as.
- Setting default call item name to call type.

### Fixed

- Validating shader types for calls.

## [Version 2.5.0] - 2025-04-25

### Added

- Added `AccelerationStructure`, `Instance`, `Geometry` items for ray tracing call.
- Allow `loadLibrary` to load JavaScript libraries from "libs" directory.
- Added bindless textures support / `getTextureHandle` to session script object.
- Added buffer reference support / `getBufferHandle` to session script object.
- Added nonuniform indexing / variable length array support.
- Added `callAction` to app script object.
- Added `frameIndex` to App script object.
- Added "Ray Tracing In Vulkan" samples.
- Added "Custom Actions" sample.

### Fixed

- Validating Call's count does not exceed vertex/indexs count.
- Fixed offset/size of buffer block bindings in Vulkan.
- Improved performance of buffer block updates.
- Improved highlighting of items used by call.
- Prevent using Session object in binding scripts.
- Fixed printf in Vulkan renderer without enabled Auto Map Bindings.

## [Version 2.4.0] - 2025-03-22

### Changed

- Added `app` script object, made `Session/Mouse/Keyboard` properties of it.
- Started integrating custom actions manager in file browser.

### Added

- Added `loadLibrary` to app script object, for loading binaries.
- Added `openFileDialog` to app script object.
- Added "Generate Mesh" action.
- Added "Ray Tracing" and "Mesh Shader" samples.
- Added new shader file extensions to file dialogs.
- More work on "Trace Rays" call.

### Fixed

- Fixed buffer not set warning when no member is used.
- Validating index type of "DrawIndexed" call.
- Toolbar icon size explicitly set to 16x16.
- Fixed QML views from being completely black on Windows.
- Preserving untitled filenames when dragging/copying items.

## [Version 2.3.0] - 2025-02-12

### Added

- Added modern shader types.
- Added "Draw Mesh Tasks" call.
- Added "Trace Rays" call [WIP].
- Added "Order-independent transparency" sample.
- Allowing expressions in Target default width/height/layers.
- Obtaining filename and line number of script errors.
- Added options "Reverse Culling" and "Flip Viewport" to Vulkan renderer.
- Added setScriptSource and setShaderSource to Session script object.
- Added custom actions toolbar item.
- Added JSON Interface Output.
- Added delta to Mouse object.

### Changed

- Enforcing dimensions and format selected in texture item.
- Omitting reference errors when evaluating expressions with default script engine.
- Extracting uniform values from structured expression results.
- Custom actions are calling "apply" instead of "execute"
- Reset item name to type name when clearing filename.
- Looking for samples also in ../share folder.
- Updated Quad sample.

### Fixed

- Checking that all attributes are set.
- Checking that indirect/index buffer is set.
- Fixed glslang compiled shader SSBO binding.
- Fixed source type deduction of untitled editors.
- Improved alignment of editable cells in binary editor.
- Not automatically evaluating when editing Scripts which are executed on reset.
- Prevent deletion of file when changing case only of file item.
- Made Session available again in custom actions.
- Fixed paths in custom actions.
- Fixed timeout in custom actions.

## [Version 2.2.0] - 2024-12-22

### Added

- Allow to bind uniform buffer members separately.
- Added support for resource-array bindings.
- Added glslang HLSL support to OpenGL renderer.
- Added Auto Sampled Textures setting to session.
- Added push constant support also for compute.
- Added name input boxes to property editors.
- Added Uniforms test sample.
- Added definition of `GPUPAD_GLSLANG`.

### Changed

- Compiling all shader stages to SPIR-V at once.
- Updated GLSL syntax highlighting keywords to GLSL 4.6 specification.
- Source editor is centering on word when executing find.
- Parse line number from printf messages containing "in line".
- Only sorting #extensions of multi-file shaders to front.
- Added option for unity build in cmake (on by default).

### Fixed

- Verifying that Vulkan is available.
- Validating uniform buffer size.
- Fixed crash when using texture buffers.
- Fixed file cache invalidation when renaming file in session tree and copying session.
- Never keep previous shader program when shader compiler did change.
- Removed no longer needed font size workaround on Windows.
- Improved alignment of editable region in binary editor.
- Highlight usage of buffer block and groups.
- Fixed re-evaluation when Includable shader is changed.
- Restored OpenGL subroutine support.

## [Version 2.1.0] - 2024-11-23

### Added

- Showing session item in session editor.
- Added some shader compiler options to session item.
- Allow to select glslang compiler for OpenGL.

### Changed

- Generating output with builtin glslang.
- Moved session shader preamble/include to session properties, removed global.
- Moved Renderer selection to session item.
- Using session renderer for validating source.

### Fixed

- Synchronization of Line Wrapping menu and toolbar controls.

## [Version 2.0.1] - 2024-10-28

### Added

- Added definition of `GPUPAD_OPENGL` and `GPUPAD_VULKAN`.
- Added push constant support.
- Allow to select entry point for Vulkan/GLSL.

### Fixed

- Fixed RGB image loader stride.
- Fixed color picker on HiDPI displays.
- Prevent sampling attachment.
- Fixed setting script globals by uniform bindings.
- Fixed evaluation of empty expression for integer values.
- Validating that all attachments have the same sample count.
- Improved attachmentless rendering.

### Changed

- Moved assets to extra directory.
- Opening first texture item of sample sessions.
- Changed texture sample editor to a combobox.
- Removed "default" themes.

## [Version 2.0.0] - 2024-08-05

### Added

- Vulkan renderer can be used for evaluating sessions.
- Using OpenImageIO for reading/writing most image formats.

### Changed

- Removed Qt5 support.
- Remove Lua scripting support.

## [Version 1.35.1] - 2023-10-26

### Added

- Added EXR Writing.

### Fixed

- Fixed TIFF writing.
- Resetting item name to default when clearing filename.

## [Version 1.35.0] - 2023-10-01

### Added

- Added TIFF reading and writing.

### Changed

- Automatically defining #version 120 for shaders using gl_FragColor.
- Saving non-session filenames and evaluation mode in session state.

### Fixed

- Fixed 16 bit texture binding custom format filter.
- Supporting non-utf8 text files with system codepage again.

## [Version 1.34.0] - 2022-06-23

### Added

- Added Files dock.
- Allow dropping files on session window.

### Changed

- Set custom tool and dock window titlebars.
- Automatically moving #extension to front.
- Not automatically indenting lines which start with }.
- Eliding long paths in recent file list.
- Saving settings on Windows in Ini file.
- Raising process priority while bringing window to front.
- Rectangular selection does not create cursors left of rect.

### Fixed

- Fixed performance problem when file is reloaded very quickly.
- Only replacing untitled untouched source editors.
- Purging textures/binaries from cache when closing editor.
- Mark editor as modified when file is deleted.
- Fixed canonical path of included shaders.

## [Version 1.33.0] - 2022-03-09

### Fixed

- Disabled maximum image limit introduced in Qt6.
- Fixed Find/Replace bar close button icon.
- Fixed TextureEditor repaint under Qt6/Linux.
- Fixed video playback for Qt6, removed Qt5 support.
- Not replacing first attribute source when another is missing.

## [Version 1.32.0] - 2022-02-17

### Added

- Added [Base16](https://github.com/chriskempson/base16) theme support.

### Fixed

- Fixed deducing shader type from file extension.

## [Version 1.31.0] - 2022-01-26

### Added

- Allow to activate overwrite mode in source editors.

### Changed

- Not automatically zooming small images to fit.
- Made zoom control a combobox.

### Fixed

- Restored zoom under mouse cursor.

## [Version 1.30.0] - 2022-01-15

### Added

- Added zoom to texture editor toolbar.
- Added saving failed message.

### Changed

- Only updating untitled file items when Saving As.
- Building Windows binary releases with Qt 6.4.1.

## [Version 1.29.0] - 2022-01-05

### Changed

- Improved texture viewer performance.
- Improved source type deduction.
- No longer adding suffix when Saving As.

### Fixed

- Fixed crash when creating Texture without width.
- Not updating Item filename when Saving As fails.
- Fixed resizing file backed target texture.
- Not restoring maximized window when dropping file.

## [Version 1.28.0] - 2022-12-16

### Added

- Added toolbar button for toggling line wrapping.
- Setting default editor font.

### Changed

- Replaced icons with symbolic icons.
- Set custom dock close/restore icons.

### Fixed

- Fixed setting file dialog directory to current file's.
- Improved layout of shader/attachment property panel.
- Gamma correcting floating point formats in preview.
- Made font size of Windows consistent.
- Prevent opening menu on Alt-mouse action.

## [Version 1.27.0] - 2022-10-15

### Added

- Trying to link shader in standalone validation to get warnings.
- Standalone validation automatically selects common HLSL entry point.
- Added keyboard shortcut to standalone validation toolbar button.
- Completing extension when saving depending on source type.

### Changed

- No Syntax Highlighting for Plaintext, added source type Generic.
- Only flipping 2D textures in editor automatically.
- Only opening untitled textures automatically next to other editors.
- Reduced margin when zooming in texture editor.

### Fixed

- Prevent menu from popping up on Alt-key shortcut.
- Not focusing hidden session dock on Ctrl-Tab.

## [Version 1.26.0] - 2022-09-04

### Added

- Allow to disable channel in texture editor.
- Allow to copy texture editor content to clipboard.
- Implemented Attachment colorWriteMask.

### Changed

- Conditionally scrolling to end when reloading externally modified file.
- Sorting editors in dock tab context menu.

### Fixed

- Improved Find/Replace performance.

## [Version 1.25.0] - 2022-07-21

### Added

- Added shader preamble and include paths.
- Added toolbar buttons to show/hide dock windows.
- Allow to hide menu bar.
- Splitting single line of values pasted in binary editor.

### Changed

- Flipping textures from files in editor by default.
- Renamed attachment depth offset factor/units to slope/constant.
- Getting entry point for shader validation from session.
- Improved Atomic-Counter sample.
- Made find/replace button toggleable.
- Updated SPIRV-Cross to 2022-07-04 master.

### Fixed

- Fixed crash on invalid stream attribute.
- Fixed crash on stream attribute count change.
- Fixed potentially wrong preview texture format.
- Fixed restoring histogram state.

## [Version 1.24.0] - 2022-04-15

### Added

- Initial Lua scripting support.
- Added multi-cursor-editing.
- Added more advanced About dialog.
- Editor back/forward navigation.
- Allow pasting hex values in Binary Editor.
- Allow pasting clipboard in new editor.
- Allow to reduce histogram range using mouse.

### Changed

- Improved rectangular selection.
- Highlighting lines starting with # in plaintext files.
- Applying EXIF transformation on load.
- Highlighting current line in line number margin.
- Removing trailing space on backspace/delete.
- Selecting item when dropping in session.
- Resume previous search on F3.

### Fixed

- Improved performance of single instance check.
- Find/Replace only in selection.
- Fixed cache update on save.
- Fixed disabling syntax highlighting for big files.
- Improved syntax highlighting performance.

## [Version 1.23.0] - 2022-02-25

### Added

- Initial QML support.
- Automatically prepending default #version when missing.
- Outputting total evaluation time.
- Added Bitonic sort sample.

### Changed

- Moved script evaluation to render thread.
- Replacing untouched untitled editor when opening source file.
- Reloading text and binary files less often.
- Merged Z selector into layer selector in texture editor.
- Activating first item when not restoring session state.
- Generating output from generated shaders.

### Fixed

- Fixed line numbers when #including multiple files.
- Limiting matching brace highlighting.
- Improved cache invalidation on source editor open/close.
- Only invalidating raw texture on format change.
- Autocompleting extension only when saving.
- Fixed potential crash when deducing block row count.
- Fixed console.log script function.
- Showing initially downloaded empty buffer in editor.

## [Version 1.22] - 2021-12-21

### Added

- Context aware autocompletion.
- Allowing to select source lines by clicking in line number margin.
- Swap textures/buffers calls.
- PFM file reading.
- Added Paint sample.
- Added Cube HLSL sample.
- Added JavaScript library sample.

### Changed

- Replaced input with Mouse and Keyboard script objects.
- Opening sample sessions with active evaluation mode.

### Fixed

- Clearing script output on manual evaluation.
- Not checking remote files in recent file list for existence.
- Fixed auto range button.
- Fixed closing brace on non empty line.
- Fixed AltGr keys with rectangular selection.

## [Version 1.21] - 2021-11-12

### Added

- Added histogram in Find panel of texture editor.
- Initial HLSL support using glslangValidator/SPIRV-Cross.
- Added more actions to editor tab context menu.
- More work on color picker.

### Changed

- Setting file dialog directory to focused editor's.
- No longer applying sRGB conversion to float target preview.
- Deducing shader source type also from content.
- Cleaned up file dialog extension filter.
- Updating texture editor when video plays.
- Accepting dropped files on editors.
- Improved syntax highlighting.
- Increased maximum zoom level.

### Fixed

- Fixed wrong editor modified indicator.
- Improved removing messages of source validation.
- Fixed deleting rectangular selection.
- Fixed input of 0 in buffer editor.
- Expression editor does not simplify 0.00 to 0.
- Ignoring printf in standalone shader validation.
- Fixed handling of small files in gli and tinyexr.
- Fixed updating binary editor when buffer block layout changes.
- Fixed potential crash in JavaScript console redirection.
- Fixed crash when activating call without program.
- Invalidating file cache on editor save.

## [Version 1.20] - 2021-08-23

### Added

- Added editor tab bar context menu.
- Added color picker in Find panel of texture editor [WIP].
- Added EXR file reading.

### Changed

- Improved reloading of source editors.
- Reloading files in background.
- Opening new editor on tab bar double-click.
- Not undocking on dock window double-click.
- Increased number of recent files.

### Fixed

- Fixed potential crash on texture editor closing.
- Improved unicode handling.
- Fixed upload of integer textures.
- Fixed crash on missing buffer bindings.
- Correctly setting font of output window.
- Improved visibility of more checkboxes on dark theme.

## [Version 1.19] - 2021-05-10

### Added

- Added fullscreen option.
- Added saving/restoring session state.
- Added option to show whitespace.
- Highlighting matching brace.

### Changed

- Improved closing multiple modified files.
- Moved source validation/type to source editor toolbar.

### Fixed

- Fixed standalone shader validation.

## [Version 1.18] - 2021-04-16

### Added

- Added manual vertical texture flip control.
- Added support for integer/double stream attributes.
- Automatically adding suffix when Saving As.

### Changed

- Opening more editors on double click.
- Applying image format on combobox select.
- Using gli for loading/saving DDS.

### Fixed

- Better handling of failing save.
- Evaluating expressions in more contexts.
- Improved message updates.

## [Version 1.17] - 2021-03-29

### Added

- Added menu action for opening containing folder.
- Added support for the #include directive.
- Added GPUPAD preprocessor definition.

### Changed

- Simplified working with multi-file shaders.
- Showing top-left corner of texture on open.
- Increased update rate of externally modified files.
- Improved binary editor cell editing.
- Checking buffer for modifications after download.
- Improved display of file paths.
- Changed default indentation.
- Improved theme.

### Fixed

- Restoring unmaximized window geometry.
- Handling different directory separators in Save Session As.
- Fixed index buffer offset.

## [Version 1.16] - 2021-01-31

### Added

- Iteration count for groups.
- Buffer block bindings.
- Sessions can be reloaded.

### Changed

- Binding uniform sets global script variable.
- Removed Script expression.
- Keep rendering with previous program version when invalid.
- Increased shader validation responsiveness.
- Increased automatic evaluation responsiveness.
- Keeping instance when opening session file.

### Fixed

- Improved auto completion popup.
- Fixed error message line number parsing.
- Fixed opening of buffer block editor.
- Fixed crash when opening missing image.
- Fixed not disappearing messages.
- Improved video playback on Windows.

## [Version 1.15] - 2021-01-05

### Added

- Providing builds for Linux and macOS.

### Fixed

- Improved Qt and OpenGL version and platform support.

## [Version 1.14] - 2020-12-23

### Added

- Expressions for texture dimensions.
- Expressions for block offset and row count.
- Separating recent files and sessions.
- Script/Expression evaluation timeout.
- Made source compatible with Qt6.

### Fixed

- Fixed session modified indicator.
- Fixed buffer field invalidation.
- Fixed untitled buffer upload.
- Improved buffer editing performance.

## [Version 1.13] - 2020-11-21

### Added

- Added indirect compute call.
- Added polygon mode to target.
- DDS reading/writing.
- TGA reading/writing.
- RAW image data can be loaded as texture.
- Z-level of 3D-textures can be selected in preview.
- Added indirect sample.

### Changed

- Replaced buffer column with block and field.
- Showing full item name in reference comboboxes.
- Setting default extension of non-2D textures to .ktx.
- Updated volume sample.

### Fixed

- Fixed reading/writing of 3D textures.
- Improved renaming file items.
- Improved bringing first instance to front (under Windows).

## [Version 1.12] - 2020-09-10

### Added

- Copy/paste/cut for rectangular selection.
- Suggest filename when saving untitled.
- Added Instancing sample.

### Changed

- Improved pasting as session item child.
- Improved binary editor cell editing.
- Improved syntax highlighting.
- Improved find/replace (Ctrl-F3).
- Changed dark theme highlight to blue.

### Fixed

- Ignore unused item references of call.
- Allow opening missing files in new editor.
- Improved buffer binding point assignment.

## [Version 1.11] - 2020-08-04

### Added

- Supporting printf debugging.
- Added constant input.mouseFragCoord.
- Automatically moving #version to front.
- Added glslang AST dump.
- Tessellation sample.

### Changed

- Increased message update interval.

### Fixed

- Accepting file drops on whole window.
- Improved GLSL and Javascript highlighter.

## [Version 1.10] - 2020-07-16

### Added

- Texture Editor Toolbar.
- Uploading/downloading of multisample textures.

### Changed

- ZeroCopy preview no longer optional.
- Automatically assign result of script expression.
- Restore evaluation mode after opening sample.
- Scroll texture with wheel and hold modifier & focus.
- Updated samples.

## [Version 1.9] - 2020-06-03

### Added

- Framebuffer without attachment supported.
- Texture buffers and image binding format.
- Atomic counters support.

### Changed

- Automatic manual evaluation after steady evaluation.
- Active calls no longer bold.
- Updated dark theme.

### Fixed

- Improved utf8 detection.
- Explicitly set shortcut for Reload.
- Fixed handle leak in KTX lib.
- Fixed column width in BinaryEditor under Windows.
- Improved uniform array binding.
- Message window automatically resizing row height.
- Fixed automatic selection of new items.
- Fixed layer/layerd for 3D texture image binding.
- Improved Save session as.

## [Version 1.8] - 2020-04-27

### Fixed

- Fixed the MingW build.

## [Version 1.7] - 2020-04-02

### Added

- Added video file playback.

### Fixed

- Restore hidden editors dock.
- Prevent deletion of newlines of rectangular selection.
- Fixed problem with file item renaming.

## [Version 1.6] - 2020-03-26

### Added

- Rectangular selection in source editor.
- Synchronizing file items' names with file system.
- Tooltip for file items.
- Added Ctrl-Tab editor cycling.
- Optional anisotropic filtering.
- Added gltf sample viewer.
- Allow to open all shaders of program.
- Reset evaluation button.

### Changed

- Expand session groups on double click.
- Enabled HighDpi scaling.
- Ignoring file modifications on save.
- Ignoring dimensions of read only texture items.
- Not serializing name of file items.
- Reduced resolution of sample textures.
- Updated TextureEditor background.
- Updated samples.

### Fixed

- Improved file caching/reloading.
- Fixed texture editor unload.
- Not downloading resources without editor.

## [Version 1.5] - 2020-03-17

### Added

- Added KTX texture support.
- Preview of non 2D textures.
- Added compressed texture formats.
- Highlighting focused editor.
- Copy Buffer call.

### Fixed

- Smarter insertion of new items.
- Improved autocompletion popup.
- Improved occurences highlighting.
- Improved session/editor synchronization.
- Missing storage buffer binding warnings.
- Logging call errors.

### Removed

- Removed binary file extensions/deducing from filter.

## [Version 1.4] - 2019-12-23

### Added

- Allow to select subroutine by index.
- Automatically append number to new items' names.
- Mouse position variables.

## [Version 1.3] - 2019-12-15

### Changed

- Zero copy preview of texture made optional.

## [Version 1.2] - 2019-12-13

### Added

- Script expression.
- Copy texture call.

### Fixed

- Fixed scroll after find bug.
- Remove trailing spaces/auto indentation improved.
- Fixed missing file crash.

## [Version 1.1] - 2019-11-08

### Added

- Expressions for call arguments.
- Copy files on session SaveAs.
- Custom actions.
- Call "Execute On" property.
- Zero copy preview of texture.
- Synchronization to compositor.
- Output of preprocessed source.
- Output of Nvidia GPU program assembly.
- Report file write errors.
- SyncTest sample.

### Changed

- Evaluation revised.
- Use dedicated GPUs by default.
- Session tree double click no longer expands.
- Auto-open message window only once.
- Updated dark theme.

### Fixed

- Message window performance improved.
- Recent file list is updated on save.
- Call property limits increased.
- Fixed Save As / default extension.
- Improved uniform not set warnings.
- Source validation prepends headers.

[version 2.7.0]: https://github.com/houmain/gpupad/compare/2.6.1...2.7.0
[version 2.6.1]: https://github.com/houmain/gpupad/compare/2.6.0...2.6.1
[version 2.6.0]: https://github.com/houmain/gpupad/compare/2.5.0...2.6.0
[version 2.5.0]: https://github.com/houmain/gpupad/compare/2.4.0...2.5.0
[version 2.4.0]: https://github.com/houmain/gpupad/compare/2.3.0...2.4.0
[version 2.3.0]: https://github.com/houmain/gpupad/compare/2.2.0...2.3.0
[version 2.2.0]: https://github.com/houmain/gpupad/compare/2.1.0...2.2.0
[version 2.1.0]: https://github.com/houmain/gpupad/compare/2.0.1...2.1.0
[version 2.0.1]: https://github.com/houmain/gpupad/compare/2.0.0...2.0.1
[version 2.0.0]: https://github.com/houmain/gpupad/compare/1.35.1...2.0.0
[version 1.35.1]: https://github.com/houmain/gpupad/compare/1.35.0...1.35.1
[version 1.35.0]: https://github.com/houmain/gpupad/compare/1.34.0...1.35.0
[version 1.34.0]: https://github.com/houmain/gpupad/compare/1.33.0...1.34.0
[version 1.33.0]: https://github.com/houmain/gpupad/compare/1.32.0...1.33.0
[version 1.32.0]: https://github.com/houmain/gpupad/compare/1.31.0...1.32.0
[version 1.31.0]: https://github.com/houmain/gpupad/compare/1.30.0...1.31.0
[version 1.30.0]: https://github.com/houmain/gpupad/compare/1.29.0...1.30.0
[version 1.29.0]: https://github.com/houmain/gpupad/compare/1.28.0...1.29.0
[version 1.28.0]: https://github.com/houmain/gpupad/compare/1.27.0...1.28.0
[version 1.27.0]: https://github.com/houmain/gpupad/compare/1.26.0...1.27.0
[version 1.26.0]: https://github.com/houmain/gpupad/compare/1.25.0...1.26.0
[version 1.25.0]: https://github.com/houmain/gpupad/compare/1.24.0...1.25.0
[version 1.24.0]: https://github.com/houmain/gpupad/compare/1.23.0...1.24.0
[version 1.23.0]: https://github.com/houmain/gpupad/compare/1.22...1.23.0
[version 1.22]: https://github.com/houmain/gpupad/compare/1.21...1.22
[version 1.21]: https://github.com/houmain/gpupad/compare/1.20...1.21
[version 1.20]: https://github.com/houmain/gpupad/compare/1.19...1.20
[version 1.19]: https://github.com/houmain/gpupad/compare/1.18...1.19
[version 1.18]: https://github.com/houmain/gpupad/compare/1.17...1.18
[version 1.17]: https://github.com/houmain/gpupad/compare/1.16...1.17
[version 1.16]: https://github.com/houmain/gpupad/compare/1.15...1.16
[version 1.15]: https://github.com/houmain/gpupad/compare/1.14...1.15
[version 1.14]: https://github.com/houmain/gpupad/compare/1.13...1.14
[version 1.13]: https://github.com/houmain/gpupad/compare/1.12...1.13
[version 1.12]: https://github.com/houmain/gpupad/compare/1.11...1.12
[version 1.11]: https://github.com/houmain/gpupad/compare/1.10...1.11
[version 1.10]: https://github.com/houmain/gpupad/compare/1.9...1.10
[version 1.9]: https://github.com/houmain/gpupad/compare/1.8...1.9
[version 1.8]: https://github.com/houmain/gpupad/compare/1.7...1.8
[version 1.7]: https://github.com/houmain/gpupad/compare/1.6...1.7
[version 1.6]: https://github.com/houmain/gpupad/compare/1.5...1.6
[version 1.5]: https://github.com/houmain/gpupad/compare/1.4...1.5
[version 1.4]: https://github.com/houmain/gpupad/compare/1.3...1.4
[version 1.3]: https://github.com/houmain/gpupad/compare/1.2...1.3
[version 1.2]: https://github.com/houmain/gpupad/compare/1.1...1.2
[version 1.1]: https://github.com/houmain/gpupad/compare/1.0.0...1.1
