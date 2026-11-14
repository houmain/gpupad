# Scripting reference

Scripts share one persistent JavaScript state for the session.
Objects can be inspected with `console.log(object)`.

## App

- `frame: Number`
- `evaluation: "Paused|Automatic|Steady"` - Additionally `"Reset|Manual|Finish"` can be set to control evaluation.
- `time: Number`
- `soundTime: Number`
- `timeDelta: Number`
- `date: [year, month, day, time]` - An array containing the current date.
- `keyboard: Keyboard`
- `mouse: Mouse`
- `session: Item`
- `selection: [Item]`
- `currentEditor: Editor?`
- `messages: [Message]`
- `palette: Object`
- `mediaEncoderFormats: EncoderFormats`
- `loadSession(fileName)`
- `openEditor(fileName): Editor?`
- `processShader(fileName, processType): String | Data`
- `isUntitled(fileName): Boolean`
- `getFileTitle(fileName): String`
- `callAction(id, arguments...): Result`
- `evaluateScript(fileName): Result`
- `makeDirectory(fileName): Boolean`
- `enumerateFiles(pattern): [fileName]`
- `enumerateDirs(pattern): [dirName]`
- `loadLibrary(fileName): Library?`
- `saveEditor(fileName): Boolean`
- `openFileDialog(pattern): String?`
- `saveFileDialog(pattern): String?`
- `readTextFile(fileName): String?`
- `writeTextFile(fileName, String): Boolean`
- `appendTextFile(fileName, String): Boolean`
- `writeBinaryFile(fileName, Data): Boolean`
- `appendBinaryFile(fileName, Data): Boolean`

## Item

`Item` is a session item exposed as mutable JavaScript object. The properties match the serialized session properties documented in the [session item reference](session-reference.md).

- `properties(): [Object]` - Returns the `name` and `type` of each property.
- `parent(): Item?`
- `findItem(ItemIdent, subItems: Boolean = true): Item?`
- `findItems(ItemIdent, subItems: Boolean = true): [Item]`
- `getItem(ItemIdent): Item` - Like findItem but requires the item to exist.
- `trackItems(ItemIdent, subItems: Boolean?, Function): TrackingIndex` - Registers a function taking an Item, which should be called whenever the tracked items are modified.
- `untrackItems(TrackingIndex)` - Takes a tracking index and stopps tracking.
- `insert(Object): Item` - Inserts a child item.
- `insertBefore(Object): Item` - Inserts a sibling before the item.
- `insertAfter(Object): Item` - Inserts a sibling after the item.
- `replaceItems([Object])`
- `clear()` - Clears the item's children.
- `remove()`
- `openEditor(): Editor?` 
- `setData([Number])` - Available on Buffer, Block, and Texture items.
- `setSource(String)` - Available on Script and Shader items.
- `processShader(processType): String | Data`
- `getDeviceHandle(): Number` - Available on Buffer and Texture items.

`ItemIdent` parameters accept:
  - A `/`-separated item path relative to the origin, for example
    `"Program/cube.vs"`.
  - A numeric item ID.
  - An Item object.
  - A predicate function taking an Item.
    
## Editor

- `type: String`
- `fileName: String`
- `title: String`
- `viewportSize: [width, height]`

## Mouse

- `buttons: {Qt.MouseButton: State}` - States indexed by Qt button values, for
  example `app.mouse.buttons[Qt.LeftButton]` (0 = Up, 1 = Down, 2 = Pressed,
  -1 = Released).
- `pos: [x, y]`
- `prevPos: [x, y]`
- `delta: [x, y]`
- `coord: [x, y]`
- `prevCoord: [x, y]`
- `fragCoord: [x, y]`
- `prevFragCoord: [x, y]`
- `editorSize: [width, height]`

## Keyboard

- `keys: {Qt.Key: State}` - States indexed by Qt key values, for example
  `app.keyboard.keys[Qt.Key_Left]` (1 = Down, 2 = Pressed, -1 = Released;
  absent keys are Up).
- `keysByKeyCode: [State]` - 256-element browser key-code array (0 = Up,
  1 = Down, 2 = Pressed, -1 = Released).
