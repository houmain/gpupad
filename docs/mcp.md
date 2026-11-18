# MCP bridge

MCP mode lets an external application inspect and modify a running GPUpad instance through the [scripting API](scripting-reference.md). The MCP server and the interactive application are two processes from the same GPUpad binary.

```text
MCP client ← standard input/output → gpupad --mcp ← local IPC → running GPUpad
```

## Starting MCP mode

Start the normal GPUpad application first and open the session to be automated. Configure the MCP client to launch the same binary with `--mcp` as its first argument:

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

The MCP process does not open or own a session. It translates MCP messages on standard input/output to local requests handled by the already running GPUpad application. Both processes must run as the same operating-system user. No network listener is opened.

## Tool

The server exposes one tool:

- `eval({code: String})` - Evaluates JavaScript in the persistent scripting state of the running GPUpad instance.

The code has access to `app`, the current Session, editors, files, shader processing, and the other objects described in the [scripting reference](scripting-reference.md). Session Item types and properties are described in the [session item reference](session-reference.md).

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

Changes to session Items and editor contents are applied in memory and affect the next evaluation without being saved. Saving is only needed to persist changes to disk and is normally left to the user. An MCP client should save only when explicitly requested.

The value of the final JavaScript expression is returned. `undefined` is reported explicitly. Boolean, number, string, null, array, and plain object results are supported. Functions, non-finite numbers, circular structures, and objects that cannot be serialized produce a tool error. Item objects should be converted to plain objects containing the required properties rather than returned directly.

Messages written with `console.log`, `console.warn`, or `console.error` during the evaluation are returned as tool content. A JavaScript exception returns an MCP tool error with its message, stack, source location, and console output.

## Resources

The MCP server publishes four Markdown resources:

- `gpupad://mcp` - MCP bridge behavior and usage.
- `gpupad://scripting-reference` - The scripting API.
- `gpupad://session-reference` - Session Item types, hierarchy, properties, and enum values.
- `gpupad://headless` - Headless execution and strict session validation.

The server's initialization instructions and `eval` tool description direct clients to these resources.

A client that can start local processes can use `gpupad --headless --strict` to validate a generated session file before loading it into the running application.

## Transport and limits

MCP messages use JSON-RPC over line-delimited standard input/output. The bridge uses a user-local connection to the interactive GPUpad process. Requests and responses are limited to 4 MiB. Connecting and writing use a one-second timeout, and an evaluation response uses a 30-second timeout.

Only one JSON value can be returned by an evaluation, but it can contain nested arrays and objects. Large binary data and device handles are better consumed inside the evaluated script or written through the scripting API rather than returned through MCP.

## Manual check

With GPUpad running, an MCP client's tool inspector should show the `eval` tool and all four resources. The protocol can also be checked directly by sending one JSON-RPC request to the MCP process. For example, PowerShell can list the tools with:

```powershell
'{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | & 'C:\path\to\gpupad.exe' --mcp
```

An `eval` call requires the running interactive application:

```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"eval","arguments":{"code":"app.session.name"}}}
```

## Security

The `eval` tool has the same authority as GPUpad's scripting API, including modifying the session and reading or writing files accessible to the process. Only trusted MCP clients should be configured to launch it. The local connection is restricted to the current operating-system user, but this does not limit what an authorized evaluation can do with that user's permissions.
