
# Changelog
All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

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

## [Version 1.0] - 2019-04-14

[Unreleased]: https://github.com/houmaster/gpupad/compare/1.14...HEAD
[Version 1.14]: https://github.com/houmaster/gpupad/compare/1.13...1.14
[Version 1.13]: https://github.com/houmaster/gpupad/compare/1.12...1.13
[Version 1.12]: https://github.com/houmaster/gpupad/compare/1.11...1.12
[Version 1.11]: https://github.com/houmaster/gpupad/compare/1.10...1.11
[Version 1.10]: https://github.com/houmaster/gpupad/compare/1.9...1.10
[Version 1.9]: https://github.com/houmaster/gpupad/compare/1.8...1.9
[Version 1.8]: https://github.com/houmaster/gpupad/compare/1.7...1.8
[Version 1.7]: https://github.com/houmaster/gpupad/compare/1.6...1.7
[Version 1.6]: https://github.com/houmaster/gpupad/compare/1.5...1.6
[Version 1.5]: https://github.com/houmaster/gpupad/compare/1.4...1.5
[Version 1.4]: https://github.com/houmaster/gpupad/compare/1.3...1.4
[Version 1.3]: https://github.com/houmaster/gpupad/compare/1.2...1.3
[Version 1.2]: https://github.com/houmaster/gpupad/compare/1.1...1.2
[Version 1.1]: https://github.com/houmaster/gpupad/compare/1.0.0...1.1
[Version 1.0]: https://github.com/houmaster/gpupad/releases/tag/1.0.0
