# Scripting reference

Session Scripts and Binding Expressions share one persistent JavaScript state. Scripts run in session-tree order before the GPU Call sequence, so globals created by one Script are available to later Scripts and Expressions. The global objects are `app` and `console`.

File paths returned by GPUpad are absolute native paths. Relative paths used by `openEditor`, file enumeration, and `loadLibrary` are resolved against the script engine's base directory; for a session Script this is normally the session directory.

`console.log`, `console.warn`, and `console.error` join their arguments with spaces and create GPUpad info, warning, and error messages. Plain JavaScript objects are formatted as JSON.

## App

`app` exposes the current Session, evaluation state, editors, files and actions.

### Session

- `session: Item` - Root Session Item.
- `loadSession(fileName: String): void` - Pauses evaluation, closes editors, clears the current Session, and loads `fileName`.
- `closeSession(): void` - Clears the current Session and invalidates existing Item trackers and wrappers.
- `messages: [Message]` - Snapshot of application messages. Each Message has `severity: "INFO|WARNING|ERROR"` and `text: String`, with optional `fileName: String`, `line: Number`, and `itemId: Number`.

### Evaluation and time

- `evaluation: String` - Read/write evaluation mode. Can be `"Paused|Automatic|Steady"`.
- `evaluate(reset: Boolean = false): void` - Triggers a manual evaluation, or a reset evaluation when `reset` is `true`.
- `frame: Number` - Read/write input frame index.
- `time: Number` - Read/write input time in seconds.
- `soundTime: Number` - Audio-generation time in seconds when sound is being generated, otherwise `time`.
- `timeDelta: Number` - Difference between the current and previous `time` values.
- `date: [year, month, day, secondsSinceMidnight]` - Current local wall-clock date and time, refreshed when read.

### Scripts and actions

- `selection: [Item]` - Session-tree selection supplied to a custom action.
- `evaluateScript(fileName: String): void` - Evaluates a JavaScript file in the current state.
- `callAction(id: String, arguments: any = {}): any` - Executes a custom action. Its global `arguments` receives the supplied value, and the action's global `result` becomes the return value. Failure to find or apply the action throws.

### Editors and input

- `currentEditor: Editor?` - Current editor, or `undefined` when none is active.
- `openEditor(fileName: String): Editor?` - Opens or retrieves an editor. The Item equivalent is `item.openEditor()`.
- `mouse: Mouse` - Mouse state associated with the active editor.
- `keyboard: Keyboard` - Keyboard state associated with the active editor.
- `palette: Palette` - Current application colors as `#rrggbb` strings: `alternateBase`, `base`, `text`, `window`, `windowText`, `button`, and `buttonText`.

### Files and dialogs

- `isUntitled(fileName: String): Boolean` - Whether `fileName` is GPUpad's untitled-file name.
- `getFileTitle(fileName: String): String` - Display title derived from a path.
- `makeDirectory(path: String): Boolean` - Creates the directory and any missing parents.
- `enumerateFiles(pattern: String): [String]` - Absolute files matching the name-filter pattern. A directory component named `**` enables recursive traversal, for example `"shaders/**/*.glsl"`.
- `enumerateDirs(pattern: String): [String]` - Directory equivalent of `enumerateFiles`.
- `openFileDialog(pattern: String = ""): String?` - Opens a file picker and returns the selected absolute filename or `undefined` when canceled.
- `saveFileDialog(pattern: String = ""): String?` - Opens a save picker and returns the selected absolute filename or `undefined` when canceled.
- `readTextFile(fileName: String): String?` - UTF-8 text, or `undefined` when the file cannot be read.
- `writeTextFile(fileName: String, text: String): Boolean` - Atomically writes UTF-8 text.
- `appendTextFile(fileName: String, text: String): Boolean` - Appends UTF-8 text.
- `writeBinaryFile(fileName: String, data: BinaryData): Boolean` - Atomically writes bytes such as the result of `processShader(..., "spirvBinary")`.
- `appendBinaryFile(fileName: String, data: BinaryData): Boolean` - Appends bytes.
- `processShader(fileName: String, processType: ShaderProcessType): String | BinaryData | undefined` - Processes the source type associated with an open source editor. An unavailable operation or empty output returns `undefined`. A Shader Item also provides `shader.processShader(processType)` without requiring an open editor.

**ShaderProcessType:**

- `"preprocess"` - Preprocessed shader source without generated `#line` directives.
- `"glsl"` - Generated GLSL source.
- `"hlsl"` - Generated HLSL source.
- `"spirv"` - SPIR-V disassembly.
- `"spirvBinary"` - SPIR-V bytes as `BinaryData`.
- `"ast"` - glslang abstract syntax tree text.
- `"json"` - Reflection data as a JSON string.
- `"programBinary"` - OpenGL program binary representation; available only where an OpenGL program can be linked.

### Native libraries

- `loadLibrary(fileName: String): Library` - Loads each library once. A `.js` library is evaluated in the persistent state and currently returns an empty object; a native reflected library returns an object whose properties are callable exported functions. Search starts in the script base directory and application library directories. Loading failure throws.

A reflected native Library exposes each exported function as a JavaScript function. Primitive values convert to Boolean or Number, character arrays convert to String, and numeric arrays accept JavaScript arrays and return a `LibraryArray` with `length` and `toArray()`. A `LibraryArray` can be passed directly to `Item.setData`. Opaque results can only be passed to parameters of the matching native type.

<details>
<summary>Support functionality for advanced actions</summary>

### Cameras

Camera input is provided by a Texture with `sourceType: "Video"` and a `.camera` file as its `fileName`. An empty `.camera` file selects the default camera and its first supported format. A JSON object can select a device and format using values returned by `app.enumerateCameras()`:

```json
{
  "id": "device ID",
  "pixelFormat": "pixel format",
  "resolutionX": 1920,
  "resolutionY": 1080
}
```

The device ID is matched first. If it is absent or unavailable, the `.camera` filename without its extension is matched against a device name, followed by the default device and then the first available device. The format fields are optional; the first supported format matching the supplied values is selected. If none matches, the device's default format remains active.

- `enumerateCameras(): [Camera]` - Available video inputs, or an empty array when multimedia support is absent.

Each object returned by `app.enumerateCameras()` has:

- `name: String` - Device description.
- `id: String` - Persistent device identifier where provided by the platform.
- `isDefault: Boolean?` - Present and true for the default device.
- `formats: [CameraFormat]` - Supported capture modes.

Each CameraFormat has `pixelFormat: String`, `resolutionX: Number`, `resolutionY: Number`, `minFrameRate: Number`, and `maxFrameRate: Number`.

### Offline rendering and encoding

`SessionRenderer` evaluates the Session for one requested time and frame, captures the selected Texture and/or generated audio, and emits the result as a `MediaFrame`. The frame is passed to `MediaEncoder.writeFrame()`. After `frameWritten`, the next frame can be requested. A final request with `isFinish: true` produces the final frame, after which the encoder finalizes the output and emits `finished`. Signals use `signal.connect(callback)`.

- `mediaEncoderFormats: EncoderFormats` - Media containers and codec combinations available on the current system. It is empty when media encoding is unavailable.
- `createSessionRenderer(options: SessionRendererOptions): SessionRenderer` - Creates an isolated renderer for deterministic video/audio frame production.
- `createMediaEncoder(options: MediaEncoderOptions): MediaEncoder` - Creates an asynchronous media encoder.

**SessionRenderer:**

`app.createSessionRenderer(options)` pauses ordinary evaluation while it owns the rendering state and restores the previous evaluation mode, time, and frame when closed. Invalid options or unavailable rendering are reported by the first `frameReady` as an error MediaFrame.

- `requestFrame(request: FrameRequest): void` - Starts one asynchronous request. A second request while one is pending emits an error MediaFrame. Completion emits `frameReady(frame)`.
- `close(): void` - Cancels pending work, releases the renderer, and restores ordinary evaluation state.
- `frameReady(MediaFrame)` - Signal emitted with a rendered frame, completion frame, or error frame.

**SessionRendererOptions:**

- `textureId: Texture-ID = 0` - Texture captured as video; `0` disables video.
- `audio: Boolean = true` - Whether rendered frames contain session audio.

At least one of video or audio must be enabled. `textureId` must identify an existing Texture.

**FrameRequest:**

- `time: Number` - Session time in seconds.
- `frameIndex: Number` - Session frame index.
- `soundSampleBase: Number` - Absolute first audio-frame index.
- `soundFrameCount: Number` - Number of stereo audio frames.
- `videoStartTime: Number` - Video presentation start in microseconds.
- `videoEndTime: Number` - Video presentation end in microseconds; it must not precede `videoStartTime`.
- `audioStartTime: Number` - Audio presentation start in microseconds.
- `isFinish: Boolean` - Marks the final frame.

**MediaFrame:**

- `image: Image` - Captured video image, or a null image for audio-only output.
- `audioSamples: BinaryData` - Interleaved stereo `Float32` samples.
- `videoStartTime: Number`
- `videoEndTime: Number`
- `audioStartTime: Number`
- `audioSampleRate: Number`
- `errorString: String` - Nonempty on failure.
- `isFinish: Boolean`

**MediaEncoder:**

- `writeFrame(frame: MediaFrame): void` - Asynchronously writes one frame. The next frame can be submitted after `frameWritten`; a finish frame finalizes the file and eventually emits `finished`.
- `waitForFinished(timeout: Number): Boolean` - Processes events until encoding finishes or the millisecond timeout expires.
- `close(): void` - Stops and releases the encoder without completing pending output.
- `frameWritten()` - Signal emitted when another non-final frame can be accepted.
- `finished(errorString: String)` - Signal emitted after finalization; an empty string means success.

**MediaEncoderOptions:**

- `outputFile: String` - Destination filename.
- `fileFormat: String` - Container key from `app.mediaEncoderFormats`.
- `videoCodec: String` - Optional video codec key from the selected format.
- `audioCodec: String` - Optional audio codec key from the selected format or selected video codec.
- `videoFrameRate: Number = 60`
- `videoBitRate: Number = 12000000` - Bits per second.
- `audioBitRate: Number = 192000` - Bits per second.

At least one codec is required, and the selected combination must appear in `app.mediaEncoderFormats`. Invalid options or unavailable encoding are reported by `finished(errorString)` when the first frame is submitted.

**EncoderFormats:**

`EncoderFormats` is an object keyed by media-format enum names. Each format has `name`, `suffix`, optional `default`, and either `videoCodecs` or `audioCodecs`. Codec maps are keyed by codec enum names; each codec has `name`, optional `default`, and a video codec additionally has compatible `audioCodecs`.

</details>

## Item

An Item is a mutable live view of one session item. Its data properties match [session-reference.md](session-reference.md), including `items: [Item]`. Assigning a writable property updates the Session. `id` and `type` identify the existing Item rather than mutable session properties. Structural changes use the methods below. Items inside a Component cannot be structurally inserted, removed, replaced, or cleared.

### Lookup

- `properties(): [PropertyInfo]` - Metadata for every property supported by this Item type. Each PropertyInfo has `name: String` and `type: "boolean|integer|number|string|color|array|object|reference|enum"`. Integer and reference properties additionally have `unsigned: Boolean`; enum properties have `values: [String]`.
- `parent(): Item?` - Parent Item, or `undefined` for the Session root.
- `findItem(itemIdent: ItemIdent, searchSubItems: Boolean = true): Item?` - First matching Item relative to this Item, or `undefined`.
- `findItems(itemIdent: ItemIdent, searchSubItems: Boolean = true): [Item]` - All predicate matches. A path, ID, or Item identifies at most one result.
- `getItem(itemIdent: ItemIdent): Item` - Equivalent to `findItem` but throws if no Item matches.
- `trackItems(itemIdent: ItemIdent, searchSubItems: Boolean = true, callback: Function): TrackingIndex` - Registers `callback(item, action)` where action is `"added"`, `"modified"`, or `"removed"`. Existing matches immediately produce `"added"`. The returned index remains active until passed to `untrackItems`.
- `untrackItems(trackingIndex: TrackingIndex): void` - Stops a tracker.

`ItemIdent` accepts a `/`-separated path of Item `name` values relative to the origin, a numeric session-wide Item ID, an Item object, or a predicate `(item) => Boolean`. `searchSubItems: false` restricts predicate matching to direct children; IDs and Item objects are session-wide identities. Path segments match `name`, not `fileName`.

### Structure and files

- `insert(object: Object): Item` - Appends a child and returns it. A missing `id` is assigned and written back into `object`.
- `insertBefore(object: Object): Item` - Inserts a sibling before this Item and returns it.
- `insertAfter(object: Object): Item` - Inserts a sibling after this Item and returns it.
- `replaceItems(objects: [Object]): void` - Makes the child list match `objects`. Existing IDs are retained; entries without IDs may reuse an unused child of the same type when that child has no children. Assigned IDs are written back into the input objects.
- `clear(): void` - Removes this Item's children.
- `remove(): void` - Removes this Item from its parent.
- `openEditor(): Editor?` - Opens the backing file of a Buffer, Texture, Shader, or Script. An untitled backing file is created when needed.
- `setSource(source: String): void` - Replaces the cached source of a Shader or Script and creates an untitled backing file when needed.
- `processShader(processType: ShaderProcessType): String | BinaryData | undefined` - Processes a Shader using its Session language, stage, compiler, and Program context.
- `getDeviceHandle(): Number` - Raw backend handle for a Buffer or Texture during rendered session evaluation, or `0` when unavailable.

### Data upload

- `setData(data: [Number] | LibraryArray): void` - Uploads data to a Buffer, Block, or Texture according to its declared layout.

For a Buffer, the first Block's Fields define conversion and row layout. The entire backing file is replaced; `rowCount` is updated when the Buffer has exactly one Block. For a Block, its own Fields define the layout, its `rowCount` is updated, and the resulting bytes replace the range beginning at its evaluated `offset`. Field padding bytes are zero. Input is a flat component array; only complete rows are written. JavaScript values for `Int64` and `Uint64` Fields are decimal strings to avoid Number precision loss.

For a Texture, input is a flat, interleaved component array in format order (`R`, `RG`, `RGB`, or `RGBA`) for the base mip level. Supported uploads are 8- and 16-bit UNorm/SNorm, 8-/16-/32-bit integer, and 16-/32-bit floating-point R/RG/RGB/RGBA formats. UNorm values use `[0, 1]`, SNorm values use `[-1, 1]`, and integer/float formats use their direct numeric values.

## Editor

- `type: EditorType` - `"Text|Shader|Script|Binary|Texture|QmlView"`.
- `fileName: String` - Absolute backing filename.
- `title: String` - Display title. Assignment changes the window title only for a QML view.
- `viewportSize: [width, height]` - Current editor viewport size. Reading it marks the Script as dependent on viewport-size changes.
- `save(): Boolean` - Saves the editor to its `fileName`. Returns `false` for an untitled editor or when saving fails.
- `exportTo(fileName: String): Boolean` - Writes the editor contents to another file without changing its backing filename. The filename extension selects the output format.
- `reload(): Boolean` - Reloads the backing file, discarding unsaved editor contents. Returns whether reloading succeeded; untitled editors cannot be reloaded.
- `close(): Boolean` - Closes this editor without prompting.

## Mouse

Reading mouse state marks the Script as dependent on mouse changes.

- `buttons: {Qt.MouseButton: InputState}` - States indexed by [Qt.MouseButton](https://doc.qt.io/qt-6/qt.html#MouseButton-enum), for example `app.mouse.buttons[Qt.LeftButton]`.
- `pos: [x, y]` - Pixel position with the editor's top-left origin.
- `prevPos: [x, y]` - Previous pixel position.
- `delta: [dx, dy]` - Pixel movement divided by editor width and height.
- `coord: [x, y]` - Pixel-center position mapped to `[-1, 1]`; Y follows the renderer's coordinate orientation.
- `prevCoord: [x, y]` - Previous normalized position.
- `fragCoord: [x, y]` - Pixel-center fragment coordinate; Y follows the renderer's coordinate orientation.
- `prevFragCoord: [x, y]` - Previous fragment coordinate.
- `editorSize: [width, height]` - Active editor size in pixels.

**InputState:** `-1` released this update, `0` up, `1` down, `2` pressed this update.

## Keyboard

Reading keyboard state marks the Script as dependent on keyboard changes.

- `keys: {Qt.Key: InputState}` - Sparse states indexed by [Qt.Key](https://doc.qt.io/qt-6/qt.html#Key-enum), for example `app.keyboard.keys[Qt.Key_Left]`; absent keys are up.
- `keysByKeyCode: [InputState]` - Dense 256-element array using legacy browser key codes.

## Known defects

- `app.selection` is populated for custom actions but is not currently exposed as an App property.
- `console.log(item)` and `console.log(app.session)` currently attempt JSON serialization of an Item and can produce a serialization error instead of an object dump.
- Assigning `Item.id` updates the wrapper value and can update another Item or insert a new Item with that ID instead of rejecting the assignment. Assigning `Item.type` can leave the wrapper showing the assigned type even though the Session retains the original type.
- A path string passed to `Item.trackItems` finds the initial Item but is not matched correctly on later changes; numeric IDs, Item objects, and predicates continue to track correctly.
- `Texture.setData` converts `width`, `height`, `depth`, and `layers` as literal integers rather than evaluating their Expressions. It writes only layer 0 / cube face 0, leaving other array layers or faces unpopulated.
- `Texture.setData` converts all 8-bit SNorm formats through an unsigned 8-bit path, so negative normalized values are incorrect.
- `Item.getDeviceHandle()` exposes a 64-bit handle as a JavaScript Number, which can lose integer precision above `2^53 - 1`.
- Native LibraryArray conversion narrows 64-bit integer elements to 32 bits, and scalar 64-bit results outside the 32-bit range fail conversion.
