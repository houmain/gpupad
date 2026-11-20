# Headless mode

Headless mode loads and evaluates sessions without opening the main window. It is writing messages on standard output.

On Windows, `gpupad.exe` is a GUI-subsystem executable, so launching it directly from an interactive console may return control before output is displayed. Use the installed `gpupad-headless.cmd` wrapper, or redirect standard output and standard error when invoking `gpupad.exe --headless` from a script.

## Starting headless mode

Pass `--headless` as the first argument:

```text
gpupad --headless [options] [files...]
```

The same mode is selected when the executable is named `gpupad-headless`, which is useful for a symbolic link or renamed copy:

```text
gpupad-headless [options] [files...]
```

## Processing

Arguments are processed from left to right. A session file is loaded when encountered and evaluated before the next session is loaded or the process exits. JavaScript files are evaluated immediately in the shared scripting state. Other supported files are opened in an editor so they can participate in later processing.

The final loaded session is always evaluated. Headless evaluation performs the same script, shader, resource, and GPU work as an evaluation in the application, using the graphics backend configured by the session.

Relative input filenames are resolved from the directory in which GPUpad was started. Session loading may temporarily change the process directory, but `--output` paths remain relative to the original working directory.

## Options

- `--strict` - Rejects a session when an item contains an unknown property, a property that does not belong to that item type, or an unrecognized item type.
- `--verbose` - Includes informational messages. Warnings and errors are always printed.
- `--set <ident> <value>` - Defines a global scripting value before subsequent scripts and sessions are evaluated. The value is passed as a string.
- `--output <item-ident> <filename>` - Writes the evaluated data of a file-backed Item to `filename`. `item-ident` is a numeric Item ID or an Item path. The option belongs after the session file whose Item it references.

Options and files can be combined. For example:

```text
gpupad --headless --strict --verbose --set sampleCount 64 scene.gpjs --output Result result.ktx
```

Multiple sessions can be evaluated in one invocation:

```text
gpupad --headless --strict first.gpjs second.gpjs
```

## Exit status and messages

The process exits with status `0` when all requested files load, every session evaluation finishes without an error message, and all requested outputs are saved. It exits with status `1` for invalid command-line arguments, loading or saving failures, script errors, shader or rendering errors, and other messages classified as errors. Warnings do not change the exit status.

Without `--verbose`, informational messages such as call timings are omitted. Each printed diagnostic includes its severity and, when available, the source filename, line, and Item name.
