# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Version 1.24] - 2022-04-15

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

## [Version 1.23] - 2022-02-25

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


[version 1.24]: https://github.com/houmain/gpupad/compare/1.23...1.24
[version 1.23]: https://github.com/houmain/gpupad/compare/1.22...1.23
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

