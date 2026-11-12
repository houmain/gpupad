# Session item reference

This reference describes the session item properties exposed to JavaScript as `app.session` and
serialized in `.gpjs` files.

`Expression` is a JavaScript expression stored as a string. `Item-ID` and the
more specific `Texture-ID`, `Buffer-ID`, similar types are numeric item
IDs.

## Hierarchy

- `Session`, `Group`, and `Component` can contain `Group`, `Component`,
  `Buffer`, `Texture`, `Program`, `ShaderTable`, `Binding`, `Stream`, `Target`,
  `Call`, `Script`, `AccelerationStructure`, and `GeometrySet`.
- `Buffer` can contain `Block`; `Block` can contain `Field`.
- `Program` can contain `Shader`.
- `ShaderTable` can contain `Record`; `Record` can contain `Binding`.
- `Stream` can contain `Attribute`.
- `Target` can contain `Attachment`.
- `AccelerationStructure` can contain `Instance`; `Instance` can contain
  `Geometry`.
- `GeometrySet` can contain `Geometry`.

## Item types

### Item (common)

- `id: Item-ID`
- `type: ItemType`
- `name: String`
- `custom: Object`
- `sessionInterface: Interface`
- `componentInterface: Interface`
- `inputItemId: Item-ID`
- `items: [Item]`

ItemType: `"Session|Group|Component|Buffer|Block|Field|Texture|Program|Shader|ShaderTable|Record|Binding|Stream|Attribute|Target|Attachment|Call|Script|GeometrySet|Geometry|AccelerationStructure|Instance"`

Interface: `"Internal|Input|Output"`

### Session

- `renderer: Renderer`
- `apiVersion: Expression`
- `shaderLanguage: ShaderLanguage`
- `shaderCompiler: ShaderCompiler`
- `shaderCompilerSettings: Object`
  - `autoMapBindings: Boolean`
  - `autoMapLocations: Boolean`
  - `autoSampledTextures: Boolean`
  - `vulkanRulesRelaxed: Boolean`
  - `spirvVersion: Expression`
  - `shaderModel: Expression`
- `shaderPreamble: Expression`
- `shaderIncludePaths: Expression`
- `flipViewport: Boolean`
- `reverseCulling: Boolean`
- `audioBufferSize: Number`
- `audioChunkSize: Number`
- `audioSampleRate: Number`

`apiVersion` is used by Vulkan. `flipViewport` and `reverseCulling` are used by
Vulkan and Direct3D.

Renderer: `"OpenGL|Vulkan|Direct3D"`

ShaderLanguage: `"None|GLSL|HLSL|Slang"`

ShaderCompiler: `"Driver|glslang|D3DCompiler|DXC|Slang"`

### Group

- `iterations: Expression`
- `inlineScope: Boolean`
- `dynamic: Boolean`

### Component

- `source: Expression`

### Buffer

- `fileName: String`

### Block

- `offset: Expression`
- `rowCount: Expression`

### Field

- `dataType: DataType`
- `count: Number`
- `padding: Number`

DataType: `"Int8|Int16|Int32|Int64|Uint8|Uint16|Uint32|Uint64|Float|Double"`

### Texture

- `fileName: String`
- `sourceType: TextureSourceType`
- `audioVolume: Number` - Used by `AudioSpectrum` and `AudioSamples`.
- `target: TextureTarget`
- `format: TextureFormat`
- `width: Expression`
- `height: Expression` - Used by targets with two or more dimensions, except cube maps.
- `depth: Expression` - Used by three-dimensional targets.
- `layers: Expression` - Used by array targets.
- `samples: Number`
- `flipVertically: Boolean`

TextureSourceType: `"NoSource|Video|AudioSpectrum|AudioSamples"`

TextureTarget: `"Target1D|Target1DArray|Target2D|Target2DArray|Target3D|TargetCubeMap|TargetCubeMapArray|Target2DMultisample|Target2DMultisampleArray|TargetRectangle|TargetBuffer"`

TextureFormat:

- `"NoFormat|R8_UNorm|RG8_UNorm|RGB8_UNorm|RGBA8_UNorm|R16_UNorm|RG16_UNorm|RGB16_UNorm|RGBA16_UNorm"`
- `"R8_SNorm|RG8_SNorm|RGB8_SNorm|RGBA8_SNorm|R16_SNorm|RG16_SNorm|RGB16_SNorm|RGBA16_SNorm"`
- `"R8U|RG8U|RGB8U|RGBA8U|R16U|RG16U|RGB16U|RGBA16U|R32U|RG32U|RGB32U|RGBA32U"`
- `"R8I|RG8I|RGB8I|RGBA8I|R16I|RG16I|RGB16I|RGBA16I|R32I|RG32I|RGB32I|RGBA32I"`
- `"R16F|RG16F|RGB16F|RGBA16F|R32F|RG32F|RGB32F|RGBA32F"`
- `"RGB9E5|RG11B10F|RG3B2|R5G6B5|RGB5A1|RGBA4|RGB10A2"`
- `"D16|D24|D24S8|D32|D32F|D32FS8X24|S8"`
- `"RGB_DXT1|RGBA_DXT1|RGBA_DXT3|RGBA_DXT5|R_ATI1N_UNorm|R_ATI1N_SNorm|RG_ATI2N_UNorm|RG_ATI2N_SNorm|RGB_BP_UNSIGNED_FLOAT|RGB_BP_SIGNED_FLOAT|RGB_BP_UNorm"`
- `"R11_EAC_UNorm|R11_EAC_SNorm|RG11_EAC_UNorm|RG11_EAC_SNorm|RGB8_ETC2|SRGB8_ETC2|RGB8_PunchThrough_Alpha1_ETC2|SRGB8_PunchThrough_Alpha1_ETC2|RGBA8_ETC2_EAC|SRGB8_Alpha8_ETC2_EAC|RGB8_ETC1"`
- `"RGBA_ASTC_4x4|RGBA_ASTC_5x4|RGBA_ASTC_5x5|RGBA_ASTC_6x5|RGBA_ASTC_6x6|RGBA_ASTC_8x5|RGBA_ASTC_8x6|RGBA_ASTC_8x8|RGBA_ASTC_10x5|RGBA_ASTC_10x6|RGBA_ASTC_10x8|RGBA_ASTC_10x10|RGBA_ASTC_12x10|RGBA_ASTC_12x12"`
- `"SRGB8_Alpha8_ASTC_4x4|SRGB8_Alpha8_ASTC_5x4|SRGB8_Alpha8_ASTC_5x5|SRGB8_Alpha8_ASTC_6x5|SRGB8_Alpha8_ASTC_6x6|SRGB8_Alpha8_ASTC_8x5|SRGB8_Alpha8_ASTC_8x6|SRGB8_Alpha8_ASTC_8x8|SRGB8_Alpha8_ASTC_10x5|SRGB8_Alpha8_ASTC_10x6|SRGB8_Alpha8_ASTC_10x8|SRGB8_Alpha8_ASTC_10x10|SRGB8_Alpha8_ASTC_12x10|SRGB8_Alpha8_ASTC_12x12"`
- `"SRGB8|SRGB8_Alpha8|SRGB_DXT1|SRGB_Alpha_DXT1|SRGB_Alpha_DXT3|SRGB_Alpha_DXT5|SRGB_BP_UNorm"`

### Program

No additional properties.

### Shader

- `fileName: String`
- `shaderType: ShaderType`
- `entryPoint: Expression`
- `preamble: Expression`
- `includePaths: Expression`

ShaderType: `"Includable|Vertex|Fragment|Geometry|TessControl|TessEvaluation|Compute|Task|Mesh|RayGeneration|RayIntersection|RayAnyHit|RayClosestHit|RayMiss|RayCallable"`

### ShaderTable

- `programId: Program-ID`

### Record

- `recordType: RecordType`
- `shaderId: Shader-ID`
- `intersectionShaderId: Shader-ID`
- `anyHitShaderId: Shader-ID`
- `closestHitShaderId: Shader-ID`

RecordType:

- `"RayGeneration"` - Uses `shaderId`.
- `"Miss"` - Uses `shaderId`.
- `"Hit"` - Uses `intersectionShaderId`, `anyHitShaderId`, `closestHitShaderId`.

### Binding

- `bindingType: BindingType`
- `editor: BindingEditor`
- `values: [Expression]`
- `textureId: Texture-ID`
- `bufferId: Buffer-ID`
- `blockId: Block-ID`
- `accelerationStructureId: AccelerationStructure-ID`
- `level: Number`
- `layer: Number`
- `minFilter: TextureFilter`
- `magFilter: TextureFilter`
- `anisotropic: Boolean`
- `wrapModeX: TextureWrapMode`
- `wrapModeY: TextureWrapMode`
- `wrapModeZ: TextureWrapMode`
- `borderColor: Color`
- `comparisonFunc: ComparisonFunc`
- `subroutine: Expression`
- `imageFormat: ImageBindingFormat`

BindingType:

- `"Uniform"` - Uses `editor` and `values`.
- `"Sampler"` - Uses `textureId`, `minFilter`, `magFilter`, `anisotropic`, `wrapModeX`, `wrapModeY`, `wrapModeZ`, `borderColor`, `comparisonFunc`.
- `"Buffer"` - Uses `bufferId`.
- `"BufferBlock"` - Uses `blockId`.
- `"Image"` - Uses `textureId`, `level`, `layer`, `imageFormat`.
- `"TextureBuffer"` - Uses `bufferId` and `imageFormat`.
- `"Subroutine"` - Uses `subroutine`.
- `"AccelerationStructure"` - Uses `accelerationStructureId`.

BindingEditor: `"Expression|Expression2|Expression3|Expression4|Expression2x2|Expression2x3|Expression2x4|Expression3x2|Expression3x3|Expression3x4|Expression4x2|Expression4x3|Expression4x4|Color"`

TextureFilter: `"Nearest|Linear|NearestMipMapNearest|NearestMipMapLinear|LinearMipMapNearest|LinearMipMapLinear"`

TextureWrapMode: `"Repeat|MirroredRepeat|ClampToEdge|ClampToBorder"`

ComparisonFunc: `"NoComparisonFunc|LessEqual|GreaterEqual|Less|Greater|Equal|NotEqual|Always|Never"`

ImageBindingFormat:

- `"Internal|r8|r8ui|r8i|r16|r16_snorm|r16f|r16ui|r16i|rg8|rg8_snorm|rg8ui|rg8i"`
- `"rgb32f|rgb32i|rgb32ui|r32f|r32ui|r32i|rg16|rg16_snorm|rg16ui|rg16i|rg16f|rgba8|rgba8_snorm|rgba8ui|rgba8i|rgb10_a2|rgb10_a2ui|r11f_g11f_b10f"`
- `"rg32f|rg32ui|rg32i|rgba16|rgba16_snorm|rgba16f|rgba16ui|rgba16i|rgba32f|rgba32i|rgba32ui"`

### Stream

No additional properties.

### Attribute

- `fieldId: Field-ID`
- `normalize: Boolean`
- `divisor: Number`

### Target

- `frontFace: FrontFace`
- `cullMode: CullMode`
- `polygonMode: PolygonMode`
- `logicOperation: LogicOperation`
- `blendConstant: Color`
- `defaultWidth: Expression`
- `defaultHeight: Expression`
- `defaultLayers: Expression`
- `defaultSamples: Number`

The `default*` properties are used when the target has no attachments.

FrontFace: `"CCW|CW"`

CullMode: `"NoCulling|Back|Front|FrontAndBack"`

PolygonMode: `"Fill|Line|Point"`

LogicOperation: `"NoLogicOperation|Copy|Clear|Set|CopyInverted|NoOp|Invert|And|Nand|Or|Nor|Xor|Equiv|AndReverse|AndInverted|OrReverse|OrInverted"`

### Attachment

- `textureId: Texture-ID`
- `level: Number`
- `layer: Number` - Used by array textures.
- `blendColorEq: BlendEquation`
- `blendColorSource: BlendFactor`
- `blendColorDest: BlendFactor`
- `blendAlphaEq: BlendEquation`
- `blendAlphaSource: BlendFactor`
- `blendAlphaDest: BlendFactor`
- `colorWriteMask: Number`
- `depthComparisonFunc: ComparisonFunc`
- `depthOffsetSlope: Number`
- `depthOffsetConstant: Number`
- `depthClamp: Boolean`
- `depthWrite: Boolean`
- `stencilFrontComparisonFunc: ComparisonFunc`
- `stencilFrontReference: Number`
- `stencilFrontReadMask: Number`
- `stencilFrontFailOp: StencilOperation`
- `stencilFrontDepthFailOp: StencilOperation`
- `stencilFrontDepthPassOp: StencilOperation`
- `stencilFrontWriteMask: Number`
- `stencilBackComparisonFunc: ComparisonFunc`
- `stencilBackReference: Number`
- `stencilBackReadMask: Number`
- `stencilBackFailOp: StencilOperation`
- `stencilBackDepthFailOp: StencilOperation`
- `stencilBackDepthPassOp: StencilOperation`
- `stencilBackWriteMask: Number`

BlendEquation: `"Add|Min|Max|Subtract|ReverseSubtract"`

BlendFactor: `"Zero|One|SrcColor|OneMinusSrcColor|SrcAlpha|OneMinusSrcAlpha|DstAlpha|OneMinusDstAlpha|DstColor|OneMinusDstColor|SrcAlphaSaturate|ConstantColor|OneMinusConstantColor|ConstantAlpha|OneMinusConstantAlpha"`

ComparisonFunc: `"NoComparisonFunc|LessEqual|GreaterEqual|Less|Greater|Equal|NotEqual|Always|Never"`

StencilOperation: `"Keep|Zero|Replace|Increment|IncrementWrap|Decrement|DecrementWrap|Invert"`

### Call

- `checked: Boolean`
- `callType: CallType`
- `executeOn: ExecuteOn` - Used by every call except `ComputeSound`.
- `programId: Program-ID`
- `shaderTableId: ShaderTable-ID`
- `targetId: Target-ID`
- `vertexStreamId: Stream-ID`
- `primitiveType: PrimitiveType`
- `count: Expression`
- `first: Expression`
- `indexBufferBlockId: Block-ID`
- `baseVertex: Expression`
- `instanceCount: Expression`
- `baseInstance: Expression`
- `indirectBufferBlockId: Block-ID`
- `drawCount: Expression`
- `patchVertices: Expression` - Used when `primitiveType` is `Patches`.
- `workGroupsX: Expression`
- `workGroupsY: Expression`
- `workGroupsZ: Expression`
- `textureId: Texture-ID`
- `fromTextureId: Texture-ID`
- `clearColor: Color`
- `clearDepth: Number`
- `clearStencil: Number`
- `bufferId: Buffer-ID`
- `fromBufferId: Buffer-ID`

CallType:

- `"Draw"` - Uses `programId`, `targetId`, `vertexStreamId`, `primitiveType`, `count`, `first`, `instanceCount`, `baseInstance`.
- `"DrawIndexed"` - Uses the `Draw` properties plus `indexBufferBlockId` and `baseVertex`.
- `"DrawIndirect"` - Uses `programId`, `targetId`, `vertexStreamId`, `primitiveType`, `indirectBufferBlockId`, `drawCount`.
- `"DrawIndexedIndirect"` - Uses the `DrawIndirect` properties plus `indexBufferBlockId`.
- `"DrawMeshTasks"` - Uses `programId`, `targetId`, `workGroupsX`, `workGroupsY`, `workGroupsZ`.
- `"DrawMeshTasksIndirect"` - Uses `programId`, `targetId`, `indirectBufferBlockId`, `drawCount`.
- `"Compute"` - Uses `programId`, `workGroupsX`, `workGroupsY`, `workGroupsZ`.
- `"ComputeIndirect"` - Uses `programId` and `indirectBufferBlockId`.
- `"TraceRays"` - Uses `shaderTableId`, `workGroupsX`, `workGroupsY`, `workGroupsZ`.
- `"ClearTexture"` - Uses `textureId`, `clearColor`, `clearDepth`, `clearStencil`.
- `"CopyTexture"` - Uses `textureId` and `fromTextureId`.
- `"SwapTextures"` - Uses `textureId` and `fromTextureId`.
- `"ClearBuffer"` - Uses `bufferId`.
- `"CopyBuffer"` - Uses `bufferId` and `fromBufferId`.
- `"SwapBuffers"` - Uses `bufferId` and `fromBufferId`.
- `"ComputeSound"` - Uses `programId`.

ExecuteOn: `"ResetEvaluation|ManualEvaluation|EveryEvaluation"`

PrimitiveType: `"Points|LineStrip|Lines|LineStripAdjacency|LinesAdjacency|TriangleStrip|TriangleFan|Triangles|TriangleStripAdjacency|TrianglesAdjacency|Patches"`

### Script

- `fileName: String`
- `executeOn: ExecuteOn`

ExecuteOn: `"ResetEvaluation|ManualEvaluation|EveryEvaluation"`

### AccelerationStructure

No additional properties.

### GeometrySet

No additional properties.

### Instance

- `geometrySetId: GeometrySet-ID`
- `transform: Expression`
- `shaderInstanceId: Expression`
- `visibilityMask: Expression`
- `hitGroupRecordOffset: Expression`
- `triangleCulling: Boolean`
- `frontFace: FrontFace`
- `opacityMode: OpacityMode`

FrontFace: `"CCW|CW"`

OpacityMode: `"GeometryDefault|ForceOpaque|ForceNonOpaque"`

### Geometry

- `geometryType: GeometryType`
- `vertexBufferBlockId: Block-ID`
- `indexBufferBlockId: Block-ID`
- `transformBufferBlockId: Block-ID`
- `count: Expression`
- `offset: Expression`
- `opaque: Boolean`
- `noDuplicateAnyHitInvocation: Boolean`

GeometryType:

- `"AxisAlignedBoundingBoxes"` - Uses `vertexBufferBlockId`, `count`, `offset`.
- `"Triangles"` - Uses `vertexBufferBlockId`, `indexBufferBlockId`, `transformBufferBlockId`, `count`, `offset`.
