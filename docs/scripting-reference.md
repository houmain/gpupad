# Scripting reference

Scripts share one persistent JavaScript state for the session.
Objects can be inspected with `console.log(object)`.

## Parameter Types

- `Item` is a session item exposed as mutable JavaScript object. The properties match
  the serialized session properties documented in the
  [session item reference](session-reference.md), and the `items` property
  contain the children.

- `ItemIdent` parameters accept:

  - An `Item` object.
  - A numeric item ID.
  - A `/`-separated item path relative to the origin, for example
    `"Program/cube.vs"`.
  - A predicate function accepting an `Item`. Predicate searches may include
    descendants according to the `subItems` argument; the single-argument
    overload searches the session recursively.

## App

- `frame: Number`
- `evaluation: "Paused" | "Automatic" | "Steady"` - Assign `"Reset"`,
  `"Manual"`, or `"Finish"` to control evaluation.
- `time: Number`
- `soundTime: Number`
- `timeDelta: Number`
- `date: [year, month, day, time]` - The current date.
- `keyboard: Keyboard`
- `mouse: Mouse`
- `session: Item`
- `selection: [Item]`
- `currentEditor: Editor?`
- `messages: [Message]`
- `palette: Object`
- `mediaEncoderFormats: EncoderFormats`
- `loadSession(fileName)`
- `clearSession()`
- `getItem(ItemIdent): Item`
- `findItem(ItemIdent, origin: ItemIdent?, subItems: Boolean?): Item?`
- `findItems(ItemIdent, origin: ItemIdent?, subItems: Boolean?): [Item]`
- `trackItems(ItemIdent, origin: ItemIdent?, subItems: Boolean?, Callback)`
- `getParentItem(ItemIdent): Item?`
- `insertItem(parent: ItemIdent?, Object): Item`
- `insertAfterItem(sibling: ItemIdent, Object): Item`
- `insertBeforeItem(sibling: ItemIdent, Object): Item`
- `getItemProperties(QJSValue itemIdent): [ItemProperty]`
- `deleteItem(ItemIdent)`
- `clearItems(ItemIdent)`
- `replaceItems(parent: ItemIdent, [Object])`
- `openEditor(fileName | ItemIdent): Editor?`
- `setBlockData(ItemIdent, [Number])`
- `setBufferData(ItemIdent, [Number])`
- `setScriptSource(ItemIdent, String)`
- `setShaderSource(ItemIdent, String)`
- `setTextureData(ItemIdent, [Number])`
- `processShader(fileName | ItemIdent, processType): String | Data`
- `getBufferHandle(ItemIdent): Number`
- `getTextureHandle(ItemIdent): Number`
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
