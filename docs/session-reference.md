# Session reference

A GPUpad session is a JSON item tree describing GPU resources, shader programs, state, and commands. This reference is intended to be sufficient for writing a `.gpjs` session and its referenced files without first using the GPUpad editor.

## Session file format

A `.gpjs` file is strict JSON: property names and string values need double quotes, and comments and trailing commas are not allowed. Its root is one `Session` item. The smallest valid file is:

```json
{
  "type": "Session"
}
```

Properties at their default value may be omitted. Unknown properties are ignored when a session is loaded, so a misspelling does not produce an error. Property names, item types, and enum values are case-sensitive.

This complete session allocates a texture and clears it to opaque blue:

```json
{
  "type": "Session",
  "items": [
    {
      "type": "Texture",
      "id": 1,
      "name": "Result",
      "width": "512",
      "height": "512"
    },
    {
      "type": "Call",
      "callType": "ClearTexture",
      "textureId": 1,
      "clearColor": "#ff0000ff"
    }
  ]
}
```

## Items

Every object in the tree is an Item. An Item has the following common properties in addition to those of its concrete type:

- `id: Item-ID` - Session-wide reference ID. Positive IDs are unique across the Session, and `0` means no item. GPUpad assigns an ID when it is omitted. Referenced items consequently have an explicit ID in the file. The property cannot be changed after insertion.
- `type: ItemType` - Concrete item type. The root is a `Session`. On a child, an omitted type becomes the parent's standard child type (`Block` in a Buffer, `Field` in a Block, `Shader` in a Program, `Record` in a ShaderTable, `Binding` in a Record, `Attribute` in a Stream, `Attachment` in a Target, `Instance` in an AccelerationStructure, or `Geometry` in an Instance or GeometrySet); otherwise it becomes `Group`. The property cannot be changed after insertion.
- `name: String` - The name the item has in the session tree. Scripting functions can find items by name. `Binding` and `Attribute` items bind values to shader resources by this name. Defaults to the `type`.
- `custom: Object` - Allows scripts to set custom properties which are serialized in the session.
- `sessionInterface: Interface = Internal` - Marks the item as an imported Component input or output when this session is used as a Component source.
- `componentInterface: Interface = Internal` - Generated from `sessionInterface` during Component import rather than normally being authored in a source session.
- `inputItemId: Item-ID` - For a non-Binding Component input, references the visible same-typed item that replaces the imported input. `0` leaves the input unresolved. Binding inputs use their own editable properties instead.
- `items: [Item]` - Ordered child items. The allowed combinations are listed under [Hierarchy](#hierarchy).

**ItemType:** `"Session|Group|Component|Buffer|Block|Field|Texture|Program|Shader|ShaderTable|Record|Binding|Stream|Attribute|Target|Attachment|Call|Script|GeometrySet|Geometry|AccelerationStructure|Instance"`

**Interface:**

- `"Internal"` - Not part of a Component's public interface.
- `"Input"` - Supplied or edited by the importing session.
- `"Output"` - Exposed to following items in the importing session.

All properties have defaults unless stated otherwise: `Number` and ID properties default to `0`, `String` properties to `""`, `Boolean` properties to `false`, arrays to `[]`, objects to `{}`, and enums to their first listed value.

`Item-ID` and more specific types such as `Texture-ID` and `Buffer-ID` are numeric item IDs. `Expression` is JavaScript source stored as a string and evaluated by the script engine; an empty numeric Expression evaluates as zero. Dimensions, offsets, counts, and work-group sizes represent non-negative integers. `String` values are literal. `Color` is encoded as `"#aarrggbb"`. Enumeration values are listed in the section that declares them.

Shader, script, buffer, texture, and media contents live in separate files named by `fileName`. GPUpad resolves relative filenames against the Session directory and serializes them as relative paths with forward slashes. Absolute paths are accepted but are machine-dependent.

### Hierarchy

Only these parent-child combinations are valid:

- `Session`, `Group`, and `Component` contain scope items: `Group`, `Component`, `Buffer`, `Texture`, `Program`, `ShaderTable`, `Binding`, `Stream`, `Target`, `Call`, `Script`, `AccelerationStructure`, and `GeometrySet`.
- `Buffer` contains `Block`; `Block` contains `Field`.
- `Program` contains `Shader`.
- `ShaderTable` contains `Record`; `Record` contains `Binding`.
- `Stream` contains `Attribute`.
- `Target` contains `Attachment`.
- `AccelerationStructure` contains `Instance`; `Instance` contains `Geometry`.
- `GeometrySet` contains `Geometry`.

## Evaluation

Evaluation traverses the item tree from top to bottom and executes Calls in that order. Items that produce resources or data therefore precede the Calls that consume them.

Scripts and Uniform Binding Expressions are evaluated once per Session evaluation, in tree order, before the Call command sequence. They share persistent JavaScript state, so Scripts and Uniform Bindings can define globals used by later Expressions. A scalar property uses the first numeric result. Uniform Expressions may return scalars or nested arrays; arrays are recursively flattened before being passed to the shader.

`Component` and non-inline `Group` items open binding scopes. A Binding applies to subsequent Calls in the current scope, and a later Binding with the same name replaces it. Bindings inside a ShaderTable `Record` are local to that Record and do not change ordinary binding state. Item scopes are binding and resource scopes, not JavaScript scopes.

Resource references resolve to compatible items in enclosing scopes or earlier items in the current scope. Contents of a non-inline Group are hidden from the parent scope, while contents of an inline Group are visible to following items there. Later siblings are not visible. GPUpad inserts the required GPU resource transitions and memory barriers between Calls; the file does not describe barriers explicitly.

## Item types

### Session

The Session is the root scope. It selects the rendering backend and shader toolchain and contains the complete ordered item tree.

- `renderer: Renderer = OpenGL` - Selects the GPU API. `TraceRays` is available with Vulkan and Direct3D, not OpenGL; mesh and ray stages also require corresponding GPU support.
- `apiVersion: ApiVersion = "1.2"` - Requested Vulkan API version; ignored by other renderers.
- `shaderLanguage: ShaderLanguage = GLSL` - Language used by every Shader in the Session.
- `shaderCompiler: ShaderCompiler = Driver` - Compiler used for every Shader. It must be compatible with `renderer` and `shaderLanguage` as shown below.
- `shaderCompilerSettings: Object = {}` - Compiler-specific settings. An omitted key has no explicit value even though the property editor initially presents Boolean settings as enabled.
  - `autoMapBindings: Boolean = true` - Lets `glslang` assign missing resource bindings.
  - `autoMapLocations: Boolean = true` - Lets `glslang` assign missing input/output locations.
  - `autoSampledTextures: Boolean = true` - Lets `glslang` combine compatible texture and sampler declarations.
  - `vulkanRulesRelaxed: Boolean = true` - Uses relaxed Vulkan rules in `glslang` for Vulkan and Direct3D targets.
  - `spirvVersion: String` - SPIR-V version requested from `glslang`; `""` selects its default.
  - `shaderModel: String` - Shader model for `D3DCompiler` or `DXC`; `""` selects 5.1 or 6.5 respectively.
- `shaderPreamble: String` - Literal source prepended to every Shader before that Shader's own `preamble`.
- `shaderIncludePaths: String` - Newline-separated include directories applied to every Shader. Relative entries are resolved from each Shader file's directory.
- `flipViewport: Boolean = true` - Flips the viewport for Vulkan and Direct3D to match GPUpad's coordinate convention; ignored by OpenGL.
- `reverseCulling: Boolean` - Reverses front/back culling for Vulkan and Direct3D; ignored by OpenGL.
- `audioBufferSize: Number = 250` - Audio output buffering duration in milliseconds.
- `audioChunkSize: Number = 800` - Number of stereo frames produced per ComputeSound chunk.
- `audioSampleRate: Number = 48000` - Audio output sample rate in frames per second.

Renderer, language, and compiler must form a supported combination:

| Renderer | Shader language | Compiler |
| --- | --- | --- |
| `OpenGL` | `GLSL` | `Driver` or `glslang` |
| `OpenGL` | `HLSL` | `glslang` |
| `Vulkan` | `GLSL` | `glslang` |
| `Vulkan` | `HLSL` | `glslang` or `DXC` |
| `Direct3D` | `GLSL` | `glslang` |
| `Direct3D` | `HLSL` | `D3DCompiler` or `DXC` |

**Renderer:** `"OpenGL|Vulkan|Direct3D"`

**ApiVersion:** `"1.1|1.2|1.3|1.4"`

**ShaderLanguage:** `"GLSL|HLSL"`

**ShaderCompiler:**

- `"Driver"` - Sends GLSL directly to the OpenGL driver.
- `"glslang"` - Compiles GLSL or HLSL to SPIR-V; GPUpad translates it when the selected renderer requires another representation.
- `"D3DCompiler"` - Compiles HLSL shader models 4.0 through 5.1.
- `"DXC"` - Compiles HLSL shader model 6.x.

### Group

A Group orders related items and optionally gives them a private binding and resource scope.

- `iterations: Expression = "1"` - Evaluated once when the command sequence is built. `0` skips the Group; positive values repeat its Bindings and Calls, up to 1000 times. Scripts and Uniform expressions are not re-evaluated for each iteration.
- `inlineScope: Boolean` - When `false`, the Group opens a private binding/resource scope. When `true`, its children behave as if they were direct children of the parent, and resources they create are visible to following items there.
- `dynamic: Boolean` - When `true`, the Group is intended to be populated by a Script, commonly on reset. Generated children are runtime state and are not serialized.

### Component

A Component embeds a snapshot of another session's children behind a scope and can expose selected imported items as inputs and outputs.

- `source: String` - Literal filename of the session from which this Component was imported. It records provenance rather than a live link, so changes to the source file are not automatically re-imported.

An item marked `sessionInterface: Input` or `Output` in the source session is materialized as a direct child of the component and receives the corresponding `componentInterface`. Nested interface items are moved to that top level during import. The following item types may be exported as an interface:

- `Buffer`, `Texture`, `Program`, `ShaderTable`, `Stream`, `Target`, `GeometrySet`, and `AccelerationStructure`.
- A top-level `Group`, `Binding`, `Call`, or `Script`. These types cannot be exported from a nested scope because moving them would change execution behavior.

For a non-Binding input, `inputItemId` selects an item of the same type outside the component. All references to the imported input inside the component are replaced with that item during evaluation. Chained component inputs are resolved transitively; missing, differently typed, or cyclic input references are invalid. An input `Binding` is not replaced by ID; its binding properties form the editable component input. Component outputs are visible to items after the component. Other component contents are internal and hidden.

### Buffer

A Buffer is a byte-addressed GPU data resource. Blocks and Fields describe typed views into its storage; they do not add separate storage.

- `fileName: String` - Optional raw binary contents. With no file, data can be supplied by a Script or produced by a Call. The allocation size is the maximum `Block.offset + Block.rowCount * blockStride` over child Blocks, with a minimum of one byte.

### Block

A Block describes a rectangular sequence of equal-stride records inside its parent Buffer. Bindings, index/indirect Calls, and ray geometry refer to Blocks.

- `offset: Expression = "0"` - A byte offset from the start of the parent Buffer.
- `rowCount: Expression = "1"` - Number of consecutive rows. The Block's byte range is `offset` through `offset + rowCount * stride`; its stride is the sum of the sizes and padding of its Fields.

### Field

A Field describes one typed element within every row of a Block. Field order defines the row layout; GPUpad does not infer API-specific alignment or padding.

- `dataType: DataType = Float` - Scalar storage type. Its byte size is listed below.
- `count: Number = 1` - Number of consecutive components. A Field's data size is `count * dataTypeSize`.
- `padding: Number = 0` - Bytes inserted after this Field. Its total contribution to the Block stride is `count * dataTypeSize + padding`; its row offset is the sum of all preceding Field contributions.

**DataType:** `"Int8|Int16|Int32|Int64|Uint8|Uint16|Uint32|Uint64|Float|Double"` - Byte sizes are 1 for `Int8`/`Uint8`, 2 for `Int16`/`Uint16`, 4 for `Int32`/`Uint32`/`Float`, and 8 for `Int64`/`Uint64`/`Double`.

### Texture

A Texture is an image resource used as sampled data, storage, a render attachment, or a copy/clear destination. It can be loaded, streamed from media, filled by a Script, or produced by GPU Calls.

- `fileName: String` - Image/texture file for `NoSource`, or media file for Video and audio sources. Ordinary image files supply a 2D image; DDS and KTX additionally preserve mip levels, arrays, cube faces, 3D data, and compressed blocks. A Texture with no source or producer has allocated but unspecified contents.
- `sourceType: TextureSourceType = NoSource` - Optional live media source. A non-`NoSource` value determines `target` and `format` and resets depth, layers, and samples to one.
- `audioVolume: Number = 50` - Audio playback level from `0` to `100` for AudioSpectrum and AudioSamples sources.
- `target: TextureTarget = Target2D` - Dimensionality and topology. Media `sourceType` values override it.
- `format: TextureFormat = RGBA8_UNorm` - Component order follows the name (`R`, `RG`, `RGB`, or `RGBA`). Unsigned-normalized formats sample in `[0, 1]`, signed-normalized in `[-1, 1]`, integer formats as integers, floating formats as floats, and sRGB formats apply the sRGB transfer function to color components. Packed, depth/stencil, and compressed formats have binary layouts supplied by compatible texture files or GPU operations.
- `width: Expression = "256"` - For a cube map, it is used for both face dimensions.
- `height: Expression = "256"` - Used by targets with two or more dimensions, except cube maps.
- `depth: Expression = "1"` - Used by three-dimensional targets.
- `layers: Expression = "1"` - Used by array targets. A cube-map array contains six faces per array element.
- `samples: Number = 1` - A value greater than one selects multisampling for supported two-dimensional targets.
- `flipVertically: Boolean` - Vertically flips file-loaded image data.

**TextureSourceType:**

- `"NoSource"` - Uses `fileName` or the explicitly declared dimensions, target, and format.
- `"Video"` - Streams video frames into a 2D `RGBA8_UNorm` texture.
- `"AudioSpectrum"` - Streams frequency-domain audio into a 1D `R32F` texture; uses `audioVolume`.
- `"AudioSamples"` - Streams stereo samples into a 1D `RG32F` texture; uses `audioVolume`.

**TextureTarget:**

- `"Target1D"` - Uses `width`.
- `"Target1DArray"` - Uses `width` and `layers`.
- `"Target2D"` - Uses `width`, `height`, and optionally `samples`.
- `"Target2DArray"` - Uses `width`, `height`, `layers`, and optionally `samples`.
- `"Target3D"` - Uses `width`, `height`, and `depth`.
- `"TargetCubeMap"` - Six square faces; uses `width` for both face dimensions.
- `"TargetCubeMapArray"` - `layers` cube maps of six square faces each; uses `width` for both face dimensions.

**TextureFormat:**
- `"R8_UNorm|RG8_UNorm|RGB8_UNorm|RGBA8_UNorm|R16_UNorm|RG16_UNorm|RGB16_UNorm|RGBA16_UNorm"` - Unsigned normalized formats.
- `"R8_SNorm|RG8_SNorm|RGB8_SNorm|RGBA8_SNorm|R16_SNorm|RG16_SNorm|RGB16_SNorm|RGBA16_SNorm"` - Signed normalized formats.
- `"R8U|RG8U|RGB8U|RGBA8U|R16U|RG16U|RGB16U|RGBA16U|R32U|RG32U|RGB32U|RGBA32U"` - Unsigned integer formats.
- `"R8I|RG8I|RGB8I|RGBA8I|R16I|RG16I|RGB16I|RGBA16I|R32I|RG32I|RGB32I|RGBA32I"` - Signed integer formats.
- `"R16F|RG16F|RGB16F|RGBA16F|R32F|RG32F|RGB32F|RGBA32F"` - Floating point formats.
- `"RGB9E5|RG11B10F|RG3B2|R5G6B5|RGB5A1|RGBA4|RGB10A2"` - Packed formats.
- `"SRGB8|SRGB8_Alpha8"` - sRGB color formats.
- `"D16|D24|D24S8|D32|D32F|D32FS8X24"` - Depth and depth/stencil texture formats.
- `"S8"` - Stencil texture formats.
- `"RGB_DXT1|RGBA_DXT1|RGBA_DXT3|RGBA_DXT5|R_ATI1N_UNorm|R_ATI1N_SNorm|RG_ATI2N_UNorm|RG_ATI2N_SNorm|RGB_BP_UNSIGNED_FLOAT|RGB_BP_SIGNED_FLOAT|RGB_BP_UNorm|R11_EAC_UNorm|R11_EAC_SNorm|RG11_EAC_UNorm|RG11_EAC_SNorm|RGB8_ETC2|SRGB8_ETC2|RGB8_PunchThrough_Alpha1_ETC2|SRGB8_PunchThrough_Alpha1_ETC2|RGBA8_ETC2_EAC|SRGB8_Alpha8_ETC2_EAC|RGB8_ETC1|RGBA_ASTC_4x4|RGBA_ASTC_5x4|RGBA_ASTC_5x5|RGBA_ASTC_6x5|RGBA_ASTC_6x6|RGBA_ASTC_8x5|RGBA_ASTC_8x6|RGBA_ASTC_8x8|RGBA_ASTC_10x5|RGBA_ASTC_10x6|RGBA_ASTC_10x8|RGBA_ASTC_10x10|RGBA_ASTC_12x10|RGBA_ASTC_12x12|SRGB8_Alpha8_ASTC_4x4|SRGB8_Alpha8_ASTC_5x4|SRGB8_Alpha8_ASTC_5x5|SRGB8_Alpha8_ASTC_6x5|SRGB8_Alpha8_ASTC_6x6|SRGB8_Alpha8_ASTC_8x5|SRGB8_Alpha8_ASTC_8x6|SRGB8_Alpha8_ASTC_8x8|SRGB8_Alpha8_ASTC_10x5|SRGB8_Alpha8_ASTC_10x6|SRGB8_Alpha8_ASTC_10x8|SRGB8_Alpha8_ASTC_10x10|SRGB8_Alpha8_ASTC_12x10|SRGB8_Alpha8_ASTC_12x12|SRGB_DXT1|SRGB_Alpha_DXT1|SRGB_Alpha_DXT3|SRGB_Alpha_DXT5|SRGB_BP_UNorm"` - Formats with block compression.

### Program

A Program groups the Shader stages that GPUpad links into one graphics, compute, mesh, or ray-tracing pipeline. It has no additional properties.

### Shader

A Shader loads one source file and assigns it a stage in its parent Program.

- `fileName: String` - Shader source file. All Shaders use the Session's `shaderLanguage`.
- `shaderType: ShaderType = Vertex` - Pipeline stage or includable source role.
- `entryPoint: String` - Literal entry-point name for HLSL, and other compiled sources with named entry points. GLSL normally uses `main`.
- `preamble: String` - Literal source prepended to this Shader after the Session preamble.
- `includePaths: String` - Newline-separated include directories appended to the Session include paths. Relative entries are resolved from this Shader file's directory.

**ShaderType:**

- `"Includable"` - Contributes source code for includes but is not a pipeline stage.
- `"Vertex|TessControl|TessEvaluation|Geometry|Fragment"` - Conventional graphics stages used by direct, indexed, and indirect Draw Calls. Vertex is the usual first stage and Fragment is the usual final raster stage; tessellation requires both tessellation stages.
- `"Task|Mesh|Fragment"` - Mesh graphics stages used by DrawMeshTasks Calls. Task is optional; Mesh is required; Fragment is optional when rasterization does not need it.
- `"Compute"` - Compute stage used by Compute, ComputeIndirect, and ComputeSound.
- `"RayGeneration|RayIntersection|RayAnyHit|RayClosestHit|RayMiss|RayCallable"` - Ray-tracing stages referenced through a ShaderTable. Ray generation is required; the other stages are selected by Records as needed.

### ShaderTable

A ShaderTable selects ray-tracing entry points and their local bindings. A TraceRays Call refers to the table rather than directly to a Program.

- `programId: Program-ID` - Required. References the visible Program containing every Shader used by the table's Records.

A ShaderTable contains exactly one RayGeneration Record. Miss and Hit Records each form a separate zero-based sequence in their relative tree order. Interleaving the three Record types does not change these category-local indices: the first Miss Record has miss index `0`, and the first Hit Record has hit-group index `0`.

### Record

A Record is one shader-table entry. Its child Bindings are local resources for that entry and do not change the surrounding binding scope.

- `recordType: RecordType = RayGeneration` - Selects the entry kind and therefore which Shader IDs apply.
- `shaderId: Shader-ID` - Required for `RayGeneration` and `Miss`; references a Shader of the corresponding stage in the ShaderTable's Program.
- `intersectionShaderId: Shader-ID` - Optional `RayIntersection` Shader for a `Hit` Record; `0` omits it.
- `anyHitShaderId: Shader-ID` - Optional `RayAnyHit` Shader for a `Hit` Record; `0` omits it.
- `closestHitShaderId: Shader-ID` - Optional `RayClosestHit` Shader for a `Hit` Record; `0` omits it.

**RecordType:**
- `"RayGeneration"` - Uses `shaderId`.
- `"Miss"` - Uses `shaderId`.
- `"Hit"` - Uses any applicable combination of `intersectionShaderId`, `anyHitShaderId`, and `closestHitShaderId`.

### Binding

A Binding assigns a value or resource to a shader name. Before each Call, GPUpad combines the Bindings active at that tree position; a later Binding with the same name replaces an earlier one in the same scope.

- `name: String` - Must exactly match the reflected shader uniform, resource, or subroutine name.
- `bindingType: BindingType = Uniform`
- `editor: BindingEditor = Expression` - Describes the expected scalar, vector, matrix, or color shape of a Uniform. Each element in `values` is evaluated and any returned arrays are flattened; the resulting component sequence is matched to the reflected uniform type.
- `values: [Expression] = ["0"]` - Used by Uniform. The evaluated value is also stored as a JavaScript global named by `name`, so later scripts and expressions can use it. Record-local Bindings do not create or replace this global.
- `textureId: Texture-ID` - Required by Sampler and Image; references the Texture to bind.
- `bufferId: Buffer-ID` - Required by Buffer and TextureBuffer; references the Buffer to bind.
- `blockId: Block-ID` - Required by BufferBlock; binds only that Block's byte range.
- `accelerationStructureId: AccelerationStructure-ID` - Required by AccelerationStructure.
- `level: Number` - Mip level used by Image.
- `layer: Number = -1` - Array/cube layer or 3D slice used by Image; `-1` binds all layers or slices.
- `minFilter: TextureFilter = Nearest` - Sampler filter used when the sampled footprint is larger than one texel; mipmap variants also select/interpolate mip levels.
- `magFilter: TextureFilter = Nearest` - Sampler filter used when magnifying. Mipmap parts of a selected enum value have no effect on magnification.
- `anisotropic: Boolean` - Enables anisotropic Sampler filtering when supported.
- `wrapModeX: TextureWrapMode = Repeat` - Sampler addressing outside the X coordinate range.
- `wrapModeY: TextureWrapMode = Repeat` - Sampler addressing outside the Y coordinate range for 2D/3D targets.
- `wrapModeZ: TextureWrapMode = Repeat` - Sampler addressing outside the Z coordinate range for 3D targets.
- `borderColor: Color = "#ff000000"` - Sampler border returned by `ClampToBorder`.
- `comparisonFunc: ComparisonFunc = NoComparisonFunc` - For a depth Sampler, enables comparison sampling and selects the comparison; `NoComparisonFunc` returns depth values normally.
- `subroutine: String` - Literal shader subroutine implementation name used by Subroutine.
- `imageFormat: ImageBindingFormat = Internal` - Storage-image element format. Image may use `Internal` to take the Texture's format. TextureBuffer must select an explicit format whose byte size equals the first Block's stride.

**BindingType:**
- `"Uniform"` - Uses `editor` and `values`.
- `"Sampler"` - Uses `textureId`, `minFilter`, `magFilter`, `anisotropic`, `wrapModeX`, `wrapModeY`, `wrapModeZ`, `borderColor`, `comparisonFunc`.
- `"Buffer"` - Binds the whole Buffer `bufferId`.
- `"BufferBlock"` - Binds the byte range described by the Block `blockId`.
- `"Image"` - Binds one texture mip level `level` and either one layer or all layers when `layer` is `-1`. Uses `imageFormat`.
- `"TextureBuffer"` - Exposes Buffer `bufferId` as a one-dimensional typed texture. Uses the first child Block's stride and requires a matching explicit `imageFormat`.
- `"Subroutine"` - Uses `subroutine`.
- `"AccelerationStructure"` - Uses `accelerationStructureId`.

**BindingEditor:** `"Expression|Expression2|Expression3|Expression4|Expression2x2|Expression2x3|Expression2x4|Expression3x2|Expression3x3|Expression3x4|Expression4x2|Expression4x3|Expression4x4|Color"` - `ExpressionN` presents an N-component vector, `ExpressionRxC` an R-row/C-column matrix, and `Color` four color components. It controls editing shape; the reflected Uniform type determines the actual upload.

**TextureFilter:** `"Nearest|Linear|NearestMipMapNearest|NearestMipMapLinear|LinearMipMapNearest|LinearMipMapLinear"`

**TextureWrapMode:** `"Repeat|MirroredRepeat|ClampToEdge|ClampToBorder"`

**ComparisonFunc:** `"NoComparisonFunc|LessEqual|GreaterEqual|Less|Greater|Equal|NotEqual|Always|Never"`

**ImageBindingFormat:**
- `"Internal"` - Uses the Texture's internal format for an Image Binding; not valid for TextureBuffer.
- `"r8|r8ui|r8i|r16|r16_snorm|r16f|r16ui|r16i|rg8|rg8_snorm|rg8ui|rg8i"`
- `"rgb32f|rgb32i|rgb32ui|r32f|r32ui|r32i|rg16|rg16_snorm|rg16ui|rg16i|rg16f|rgba8|rgba8_snorm|rgba8ui|rgba8i|rgb10_a2|rgb10_a2ui|r11f_g11f_b10f"`
- `"rg32f|rg32ui|rg32i|rgba16|rgba16_snorm|rgba16f|rgba16ui|rgba16i|rgba32f|rgba32i|rgba32ui"`

### Stream

A Stream maps Buffer Fields to the vertex inputs of a graphics Program. A Draw Call refers to the Stream. It has no additional properties; its Attribute children define the mapping.

### Attribute

An Attribute maps one Field to one reflected vertex input.

- `name: String` - Must exactly match the reflected vertex input name or semantic. 
- `fieldId: Field-ID` - Required. The referenced visible Field must have one to four components and supplies the input's scalar type, component count, Buffer, Block stride, and byte offset.
- `normalize: Boolean` - Converts integer data to normalized floating-point values when enabled.
- `divisor: Number` - `0` advances per vertex; a positive divisor advances per that many instances. 

### Target

A Target defines rasterization state and the color, depth, and stencil destinations for graphics Calls. Every graphics Draw Call requires one.

- `frontFace: FrontFace = CCW` - Winding considered front-facing before any Session viewport/culling adjustment.
- `cullMode: CullMode = NoCulling` - Which triangle faces to discard.
- `polygonMode: PolygonMode = Fill` - How polygons are rasterized.
- `logicOperation: LogicOperation = NoLogicOperation` - Optional per-component integer logic operation applied instead of ordinary color output behavior.
- `blendConstant: Color = "#ffffffff"` - Constant color used by Attachment blend factors `ConstantColor`, `OneMinusConstantColor`, `ConstantAlpha`, and `OneMinusConstantAlpha`.
- `defaultWidth: Expression = "1"` - Render width used only when the Target has no Attachments.
- `defaultHeight: Expression = "1"` - Render height used only when the Target has no Attachments.
- `defaultLayers: Expression = "1"` - Render layer count used only when the Target has no Attachments.
- `defaultSamples: Number = 1` - Sample count used only when the Target has no Attachments.

Attachment order determines color attachment indices. All attached mip levels and layers must have compatible dimensions and sample counts. A Target without Attachments uses the `default*` dimensions and does not implicitly create or reference a Texture.

**FrontFace:** `"CCW|CW"`

**CullMode:** `"NoCulling|Back|Front|FrontAndBack"`

**PolygonMode:** `"Fill|Line|Point"`

**LogicOperation:** `"NoLogicOperation|Copy|Clear|Set|CopyInverted|NoOp|Invert|And|Nand|Or|Nor|Xor|Equiv|AndReverse|AndInverted|OrReverse|OrInverted"`

### Attachment

An Attachment connects one Texture subresource to a Target. The Texture format decides whether its color/blend, depth, or stencil properties apply.

- `textureId: Texture-ID` - Required visible Texture. Its format determines the Attachment role.
- `level: Number` - Mip level to attach.
- `layer: Number = -1` - Array/cube layer or 3D slice; `-1` attaches all layers or slices.
- `blendColorEq: BlendEquation = Add` - Color blend equation; used only for a color Texture.
- `blendColorSource: BlendFactor = One` - Source color factor; used only for a color Texture.
- `blendColorDest: BlendFactor = Zero` - Destination color factor; used only for a color Texture.
- `blendAlphaEq: BlendEquation = Add` - Alpha blend equation; used only for a color Texture.
- `blendAlphaSource: BlendFactor = One` - Source alpha factor; used only for a color Texture.
- `blendAlphaDest: BlendFactor = Zero` - Destination alpha factor; used only for a color Texture.
- `colorWriteMask: Number = 15` - Color-channel bit mask: red `1`, green `2`, blue `4`, alpha `8`; used only for a color Texture.
- `depthComparisonFunc: ComparisonFunc = Less` - Depth test for a depth Texture.
- `depthOffsetSlope: Number` - Slope-scaled depth bias for a depth Texture.
- `depthOffsetConstant: Number` - Constant depth bias for a depth Texture.
- `depthClamp: Boolean` - Clamps rather than clips depth outside the viewport range when supported.
- `depthWrite: Boolean = true` - Enables writes to a depth Texture.
- `stencilFrontComparisonFunc: ComparisonFunc = Always` - Front-face stencil test.
- `stencilFrontReference: Number` - Front-face stencil reference value.
- `stencilFrontReadMask: Number = 255` - Front-face mask applied before comparison.
- `stencilFrontFailOp: StencilOperation = Keep` - Front-face operation when the stencil test fails.
- `stencilFrontDepthFailOp: StencilOperation = Keep` - Front-face operation when stencil passes and depth fails.
- `stencilFrontDepthPassOp: StencilOperation = Keep` - Front-face operation when both tests pass.
- `stencilFrontWriteMask: Number = 255` - Front-face mask controlling which stencil bits may be written.
- `stencilBackComparisonFunc: ComparisonFunc = Always` - Back-face stencil test.
- `stencilBackReference: Number` - Back-face stencil reference value.
- `stencilBackReadMask: Number = 255` - Back-face mask applied before comparison.
- `stencilBackFailOp: StencilOperation = Keep` - Back-face operation when the stencil test fails.
- `stencilBackDepthFailOp: StencilOperation = Keep` - Back-face operation when stencil passes and depth fails.
- `stencilBackDepthPassOp: StencilOperation = Keep` - Back-face operation when both tests pass.
- `stencilBackWriteMask: Number = 255` - Back-face mask controlling which stencil bits may be written.

**BlendEquation:** `"Add|Min|Max|Subtract|ReverseSubtract"`

**BlendFactor:** `"Zero|One|SrcColor|OneMinusSrcColor|SrcAlpha|OneMinusSrcAlpha|DstAlpha|OneMinusDstAlpha|DstColor|OneMinusDstColor|SrcAlphaSaturate|ConstantColor|OneMinusConstantColor|ConstantAlpha|OneMinusConstantAlpha"`

**ComparisonFunc:** `"NoComparisonFunc|LessEqual|GreaterEqual|Less|Greater|Equal|NotEqual|Always|Never"`

**StencilOperation:** `"Keep|Zero|Replace|Increment|IncrementWrap|Decrement|DecrementWrap|Invert"`

### Call

A Call records one ordered GPU command: a draw, dispatch, trace, clear, copy, or swap. Only the properties named by its `callType` are used.

- `checked: Boolean = true` - When `false`, the Call is disabled and does not execute.
- `callType: CallType = Draw` - Command to execute. Every value and its required properties are described below.
- `executeOn: ExecuteOn = EveryEvaluation` - Evaluation classes in which the Call runs. Ignored by ComputeSound.
  - `"ResetEvaluation"` - Reset only.
  - `"ManualEvaluation"` - Reset and manual evaluation.
  - `"EveryEvaluation"` - Reset, manual, automatic, and steady evaluation.
- `programId: Program-ID` - Required by graphics, mesh, Compute, ComputeIndirect, and ComputeSound Calls; the Program must contain stages compatible with the Call.
- `shaderTableId: ShaderTable-ID` - Required by TraceRays; the table supplies its Program and record-local Bindings.
- `targetId: Target-ID` - Required by every graphics or mesh draw.
- `vertexStreamId: Stream-ID` - Vertex input Stream for conventional draws. It may be `0` only when the shaders use no user vertex attributes, such as a full-screen draw using a built-in vertex ID.
- `primitiveType: PrimitiveType = Triangles` - Topology assembled from vertices or indices by conventional draws.
- `count: Expression` - Number of vertices for Draw or indices for DrawIndexed. When empty, a direct Draw uses the smallest `rowCount` among Stream Attributes with `divisor: 0`, and a direct DrawIndexed uses all available indices, in both cases after `first`.
- `first: Expression = "0"` - First vertex for Draw or first index for DrawIndexed.
- `indexBufferBlockId: Block-ID` - Required by indexed draws. The Block must contain unsigned index elements of total width 1, 2, or 4 bytes; use 2 or 4 bytes for portability.
- `baseVertex: Expression = "0"` - Signed value added to each fetched index in an indexed draw.
- `instanceCount: Expression = "1"` - Number of instances for direct conventional draws.
- `baseInstance: Expression = "0"` - First instance ID for direct conventional draws.
- `indirectBufferBlockId: Block-ID` - Required by indirect Calls. The Block offset locates the first command, and its stride must match the command layout stated for the selected CallType.
- `drawCount: Expression = "1"` - Number of consecutive commands for an indirect graphics or mesh Call.
- `patchVertices: Expression = "3"` - Vertices per patch when `primitiveType` is `Patches`.
- `workGroupsX: Expression = "1"` - X dispatch size for Compute, DrawMeshTasks, or TraceRays.
- `workGroupsY: Expression = "1"` - Y dispatch size for Compute, DrawMeshTasks, or TraceRays.
- `workGroupsZ: Expression = "1"` - Z dispatch size for Compute, DrawMeshTasks, or TraceRays.
- `textureId: Texture-ID` - Required destination for ClearTexture, CopyTexture, and SwapTextures.
- `fromTextureId: Texture-ID` - Required source for CopyTexture and second Texture for SwapTextures.
- `clearColor: Color = "#ff000000"` - Value used by ClearTexture when the destination has color components.
- `clearDepth: Number = 1` - Value used by ClearTexture when the destination has a depth component.
- `clearStencil: Number` - Value used by ClearTexture when the destination has a stencil component.
- `bufferId: Buffer-ID` - Required destination for ClearBuffer, CopyBuffer, and SwapBuffers.
- `fromBufferId: Buffer-ID` - Required source for CopyBuffer and second Buffer for SwapBuffers.

**CallType:**

- `"Draw"` - Direct non-indexed conventional graphics draw. Uses `programId`, `targetId`, optional `vertexStreamId`, `primitiveType`, `count`, `first`, `instanceCount`, and `baseInstance`.
- `"DrawIndexed"` - Direct indexed conventional graphics draw. Uses the Draw properties plus required `indexBufferBlockId` and `baseVertex`.
- `"DrawIndirect"` - Conventional non-indexed draw(s) whose arguments come from `indirectBufferBlockId`. Uses `programId`, `targetId`, optional `vertexStreamId`, `primitiveType`, and `drawCount`. Each 16-byte command row is `Uint32 count, instanceCount, first, baseInstance`.
- `"DrawIndexedIndirect"` - Indexed draw(s) whose arguments come from `indirectBufferBlockId`; also requires `indexBufferBlockId`. Each 20-byte command row is `Uint32 count, instanceCount, firstIndex`, `Int32 baseVertex`, `Uint32 baseInstance`.
- `"DrawMeshTasks"` - Direct mesh-shader draw. Uses `programId`, `targetId`, and `workGroupsX/Y/Z`.
- `"DrawMeshTasksIndirect"` - Mesh-shader draw(s) read from `indirectBufferBlockId`; uses `programId`, `targetId`, and `drawCount`. Each 12-byte command row is `Uint32 groupCountX, groupCountY, groupCountZ`.
- `"Compute"` - Direct compute dispatch. Uses a compute `programId` and `workGroupsX/Y/Z`.
- `"ComputeIndirect"` - Compute dispatch read from `indirectBufferBlockId`. Uses a compute `programId`; each 12-byte command row is `Uint32 x, y, z`.
- `"TraceRays"` - Ray-tracing dispatch. Uses `shaderTableId` and `workGroupsX/Y/Z`; available with Vulkan and Direct3D only.
- `"ClearTexture"` - Clears every applicable component of `textureId` using `clearColor`, `clearDepth`, and `clearStencil`.
- `"CopyTexture"` - Copies all compatible data from `fromTextureId` to `textureId`; source and destination topology, dimensions, and format must be copy-compatible.
- `"SwapTextures"` - Exchanges the backing resources of compatible `textureId` and `fromTextureId` without copying their contents.
- `"ClearBuffer"` - Clears all bytes of `bufferId` to zero.
- `"CopyBuffer"` - Copies all compatible data from `fromBufferId` to `bufferId`.
- `"SwapBuffers"` - Exchanges the backing resources of compatible `bufferId` and `fromBufferId` without copying their contents.
- `"ComputeSound"` - Audio-driven compute dispatch using a compute `programId`; it ignores `executeOn` and runs when output needs another chunk. GPUpad binds an implicit storage buffer named `soundBuffer` and dispatches groups of 256 invocations. Its first 16 bytes are `Uint32 sampleBaseLow, sampleBaseHigh, sampleRate, frameCount`, followed by `frameCount` stereo frames as `Float32 left, right`. Invocations whose index is at least `frameCount` are outside the output range.

Indirect command data must be produced before the Call. Its Block offset must satisfy the selected GPU API's indirect-command alignment requirement.

**PrimitiveType:** `"Points|LineStrip|Lines|LineStripAdjacency|LinesAdjacency|TriangleStrip|TriangleFan|Triangles|TriangleStripAdjacency|TrianglesAdjacency|Patches"` - `Patches` additionally uses `patchVertices` and requires tessellation Shader stages.

### Script

A Script executes a JavaScript file in the persistent session state. Scripts run in tree order before GPU Calls and can define globals for later Scripts and Expressions, populate dynamic Groups, and upload Buffer or Texture data. Binding/resource scopes do not create JavaScript scopes.

- `fileName: String` - JavaScript source file.
- `executeOn: ExecuteOn = ManualEvaluation` - Evaluation classes in which the Script runs.
  - `"ResetEvaluation"` - Reset only.
  - `"ManualEvaluation"` - Reset and manual evaluation.
  - `"EveryEvaluation"` - Reset, manual, automatic, and steady evaluation.

### AccelerationStructure

An AccelerationStructure is the top-level ray-tracing scene structure. It contains Instances and is exposed to a ray-tracing Program by an AccelerationStructure Binding. It has no additional properties.

### GeometrySet

A GeometrySet is a reusable collection of Geometry items. An Instance can reference it instead of containing its own Geometry. It has no additional properties.

### Instance

An Instance places one inline or shared geometry collection in an AccelerationStructure and supplies its per-instance ray-tracing metadata.

- `geometrySetId: GeometrySet-ID` - Shared GeometrySet used when the Instance has no Geometry children. Inline children take precedence; `0` is valid only when inline Geometry is present.
- `transform: Expression` - Row-major 3x4 affine matrix as 12 values, or a 4x4 matrix as 16 values whose first three rows are used. An empty result is the identity matrix.
- `shaderInstanceId: Expression` - Value visible to ray shaders. When empty, GPUpad uses this Instance's zero-based position in the AccelerationStructure.
- `visibilityMask: Expression = "255"` - Eight-bit instance mask tested against the ray mask during traversal.
- `hitGroupRecordOffset: Expression = "0"` - Instance contribution added when selecting a Hit Record from the ShaderTable.
- `triangleCulling: Boolean` - Enables front/back triangle culling during ray traversal.
- `frontFace: FrontFace = CCW` - Winding considered front-facing for ray-tracing triangle culling.
- `opacityMode: OpacityMode = GeometryDefault` - Keeps each Geometry's opacity or forces all Geometry in the Instance opaque/non-opaque.

**FrontFace:** `"CCW|CW"`

**OpacityMode:**

- `"GeometryDefault"` - Uses each Geometry's `opaque` property.
- `"ForceOpaque"` - Treats all Geometry in the Instance as opaque.
- `"ForceNonOpaque"` - Treats all Geometry in the Instance as non-opaque.

### Geometry

A Geometry describes one triangle or axis-aligned-bounding-box primitive set in a ray-tracing acceleration structure.

- `geometryType: GeometryType = AxisAlignedBoundingBoxes` - Primitive representation and required Blocks; see the values below.
- `vertexBufferBlockId: Block-ID` - Required primitive-data Block. For Triangles, each vertex row begins with three `Float32` position components. For AxisAlignedBoundingBoxes, each row is exactly 24 bytes: three `Float32` minimum components followed by three maximum components.
- `indexBufferBlockId: Block-ID` - Optional unsigned index Block for Triangles. Without it, each consecutive group of three vertices forms a triangle.
- `transformBufferBlockId: Block-ID` - Optional Triangle transform Block containing a row-major 3x4 matrix of `Float32` values.
- `count: Expression` - Number of primitives, not vertices or indices. When empty, GPUpad uses all complete primitives available in the referenced Block(s).
- `offset: Expression = "0"` - Additional byte offset into the primitive data.
- `opaque: Boolean = true` - Declares the Geometry opaque unless the Instance `opacityMode` overrides it; opaque Geometry can skip any-hit processing.
- `noDuplicateAnyHitInvocation: Boolean` - Requests at most one any-hit invocation for a primitive/ray intersection.

**GeometryType:**

- `"AxisAlignedBoundingBoxes"` - Procedural primitives using `vertexBufferBlockId`, `count`, and `offset`.
- `"Triangles"` - Triangle primitives using `vertexBufferBlockId`, optional `indexBufferBlockId`, optional `transformBufferBlockId`, `count`, and `offset`.
