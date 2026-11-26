# MCP bridge

MCP bridge lets an external application launch and list GPUpad instances and control them through the [scripting API](scripting-reference.md). The MCP bridge and each controlled application are separate processes from the same GPUpad binary and must run as the same operating-system user. No network listener is opened.

```text
MCP client ← standard input/output → gpupad --mcp ← local IPC → GPUpad instances
```

## Starting MCP bridge

Configure the MCP client to launch GPUpad with `--mcp` as its first argument:

```json
{
  "mcpServers": {
    "gpupad": {
      "command": "/path/to/gpupad",
      "args": ["--mcp"]
    }
  }
}
```

The MCP process is a broker. It discovers running GPUpad instances, can launch new ones, and routes evaluations to the selected instance. 

## Tools

The bridge exposes these tools:

- `list({})` - Lists running instances and reports which one is selected.
- `status({instanceId?: String})` - Reports an instance's process, mode, readiness, and loaded session. When omitted, `instanceId` refers to the selected instance.
- `select({instanceId: String})` - Selects the target used by subsequent evaluations.
- `launch({arguments?: String[]})` - Launches and instance with the arguments and automatically selects the new instance.
- `close({instanceId?: String})` - Closes an instance, or the selected instance when omitted. Closing a windowed instance uses the normal unsaved-changes prompt; canceling that prompt leaves the instance running and selected.
- `eval({code: String})` - Evaluates JavaScript in the selected instance's persistent scripting state on the main thread.

When no instance has been selected, `eval` and `status` automatically select the only running instance. They return an ambiguity error when multiple instances are available.

`launch` generates an instance ID and automatically prepends `--instance <id>` to the supplied arguments. The call returns after the new instance has registered and is ready; failed launches preserve the previous selection. Headless instances keep running when launched by the `--instance` argument is passed.

`close` requests a clean shutdown through the instance connection. It does not terminate the process by PID. Successfully closing the selected instance clears the selection.

For example, this launches a persistent windowless instance with a session already loaded:

```json
{
  "arguments": ["--headless", "C:/project/sample.gpjs"]
}
```

`eval` code has access to `app`, the current Session, editors, files, shader processing, and the other objects described in the [scripting reference](scripting-reference.md). Session Item types and properties are described in the [session item reference](session-reference.md).

An evaluation can inspect state:

```javascript
({
  renderer: app.session.renderer,
  itemCount: app.session.findItems(() => true).length
})
```

It can also modify the live session:

```javascript
const call = app.session.getItem("Draw")
call.checked = false
({ id: call.id, checked: call.checked })
```

The value of the final JavaScript expression is returned. `undefined` is reported explicitly. Boolean, number, string, null, array, and plain object results are supported. Functions, non-finite numbers, circular structures, and objects that cannot be serialized produce a tool error.

Messages written with `console.log`, `console.warn`, or `console.error` during the evaluation are returned as tool content. A JavaScript exception returns an MCP tool error with its message, stack, source location, and console output.

Changes to session Items and editor contents are applied in memory and affect the next evaluation without being saved. Saving is only needed to persist changes to disk and is normally left to the user. An MCP client should save only when explicitly requested.

The following example replaces the current session with a complete in-memory graphics session. IDs returned by inserted Items can immediately be used for references. `setSource()` creates untitled backing files without writing anything to disk. The function scope also allows the example to be run repeatedly in the persistent scripting state. An output Texture's editor must be open during evaluation to receive the evaluated data. `app.evaluate()` triggers manual evaluation. An explicitly requested export can directly follow. The destination extension selects the image or texture format:

```javascript
(() => {
  app.closeSession()

  const session = app.session
  session.renderer = "OpenGL"
  session.shaderLanguage = "GLSL"
  session.shaderCompiler = "Driver"

  const texture = session.insert({
    type: "Texture",
    name: "Result",
    width: "512",
    height: "512"
  })

  const target = session.insert({ type: "Target", name: "Target" })
  target.insert({ type: "Attachment", textureId: texture.id })

  const program = session.insert({ type: "Program", name: "Program" })
  const vertex = program.insert({ type: "Shader", shaderType: "Vertex" })
  vertex.setSource(`#version 330
const vec2 positions[3] = vec2[3](
    vec2(-0.8, -0.8),
    vec2(0.8, -0.8),
    vec2(0.0, 0.8));
void main()
{
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}`)

  const fragment = program.insert({ type: "Shader", shaderType: "Fragment" })
  fragment.setSource(`#version 330
layout(location = 0) out vec4 color;
void main()
{
    color = vec4(0.2, 0.6, 1.0, 1.0);
}`)

  const call = session.insert({
    type: "Call",
    callType: "Draw",
    programId: program.id,
    targetId: target.id,
    count: "3"
  })

  const editor = texture.openEditor()
  app.evaluate()

  app.makeDirectory("renders")
  if (!editor.exportTo("renders/result.png"))
    throw new Error("Export failed")

  return { textureId: texture.id, targetId: target.id, programId: program.id, callId: call.id }
})()
```

## Resources

The MCP server publishes four Markdown resources:

- `gpupad://mcp` - MCP bridge behavior and usage.
- `gpupad://scripting-reference` - The scripting API.
- `gpupad://session-reference` - Session Item types, hierarchy, properties, and enum values.
- `gpupad://headless` - Headless execution and strict session validation.

The server's initialization instructions and tool descriptions direct clients to these resources.

A client that can start local processes can use `gpupad --headless --strict` to validate a generated session file before loading it into the running application.

## Transport and limits

MCP messages use JSON-RPC over line-delimited standard input/output. The bridge uses user-local connections to registered GPUpad instances. Requests and responses are limited to 4 MiB. Connecting and writing use a one-second timeout, and an evaluation response uses a 30-second timeout.

Only one JSON value can be returned by an evaluation, but it can contain nested arrays and objects. Large binary data and device handles are better consumed inside the evaluated script or written through the scripting API rather than returned through MCP.

## Manual check

An MCP client's tool inspector should show all five tools and all four resources. The protocol can also be checked directly by sending one JSON-RPC request to the MCP process. For example, PowerShell can list the tools with:

```powershell
'{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | & 'C:\path\to\gpupad.exe' --mcp
```

An `eval` call requires a selected instance. A single running instance is selected automatically:

```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"eval","arguments":{"code":"app.session.name"}}}
```

## Security

The `eval` tool has the same authority as GPUpad's scripting API, including modifying the session and reading or writing files accessible to the process. Only trusted MCP clients should be configured to launch it. The local connection is restricted to the current operating-system user, but this does not limit what an authorized evaluation can do with that user's permissions.
