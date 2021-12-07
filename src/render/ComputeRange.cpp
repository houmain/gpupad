#include "ComputeRange.h"
#include "GLContext.h"
#include "TextureData.h"
#include <QOpenGLBuffer>

namespace
{
static constexpr auto computeShaderSource = R"(

layout (local_size_x = 8, local_size_y = 8) in;

uniform SAMPLER uTexture;
uniform uvec2 uSize;
uniform int uLevel;
uniform int uLayer;
uniform int uFace;
uniform float uMin;
uniform float uMax;

layout (std430) buffer result_ssbo {
  uint global_min_value[4];
  uint global_max_value[4];
};

shared vec4 min_value[gl_WorkGroupSize.x][gl_WorkGroupSize.y]; 
shared vec4 max_value[gl_WorkGroupSize.x][gl_WorkGroupSize.y];

float linearToSrgb(float value) {
  if (value > 0.0031308)
    return 1.055 * pow(value, 1.0 / 2.4) - 0.055;
  return 12.92 * value;
}
vec3 linearToSrgb(vec3 value) {
  return vec3(
    linearToSrgb(value.r),
    linearToSrgb(value.g),
    linearToSrgb(value.b)
  );
}
vec3 getCubeTexCoord(vec2 tc, int face) {
  float rx = 0.5 - tc.x;
  float ry = 0.5 - tc.y;
  float rz = 0.5;
  return vec3[](
    vec3(-rz, -ry, rx),
    vec3( rz, -ry, rx),
    vec3( rx,  rz, ry),
    vec3( rx, -rz, ry),
    vec3( rx, -ry, rz),
    vec3(-rx, -ry, rz)
  )[face];
}

void main() {

  // first pass accumulate local
  {
    const uvec3 step_size = gl_WorkGroupSize * gl_NumWorkGroups;
    vec4 local_min = uMax.xxxx;
    vec4 local_max = uMin.xxxx;

    for (uint y = gl_GlobalInvocationID.y; y < uSize.y; y += step_size.y)
      for (uint x = gl_GlobalInvocationID.x; x < uSize.x; x += step_size.x) {
        vec4 color = vec4(SAMPLE);
        color = MAPPING;
        color = SWIZZLE;
        local_min = min(local_min, color);
        local_max = max(local_max, color);
      }
  
    min_value[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = local_min;
    max_value[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = local_max;
  }

  memoryBarrierShared();
  barrier();
  
  // second pass accumulate shared memory buffer
  if (gl_LocalInvocationID.y == 0) {
    vec4 local_min = min_value[gl_LocalInvocationID.x][0];
    vec4 local_max = max_value[gl_LocalInvocationID.x][0];
    for (uint y = 1; y < gl_WorkGroupSize.y; ++y) {
      local_min = min(local_min, min_value[gl_LocalInvocationID.x][y]);
      local_max = max(local_max, max_value[gl_LocalInvocationID.x][y]);
    }
    min_value[gl_LocalInvocationID.x][0] = local_min;
    max_value[gl_LocalInvocationID.x][0] = local_max;
  }
  memoryBarrierShared();
  barrier();
  
  // third pass atomic update of global min max values
  if (gl_LocalInvocationID.x == 0 && gl_LocalInvocationID.y == 0) {
    vec4 local_min = min_value[0][0];
    vec4 local_max = max_value[0][0];
    for (uint x = 1; x < gl_WorkGroupSize.x; ++x) {
      local_min = min(local_min, min_value[x][0]);
      local_max = max(local_max, max_value[x][0]);
    }
    
#define UpdateGlobal(global_value, local_value, op) \
  do {\
    uint gv_ui = global_value;\
    float gv = uintBitsToFloat(gv_ui);\
    while (local_value op gv) {\
      uint lv_ui = floatBitsToUint(local_value);\
      uint current_value = atomicCompSwap(global_value, gv_ui, lv_ui);\
      if (current_value == gv_ui)\
        break;\
      gv_ui = current_value;\
      gv = uintBitsToFloat(gv_ui);\
    }\
  } while (false)
  
    UpdateGlobal(global_min_value[0], local_min.x, <);
    UpdateGlobal(global_min_value[1], local_min.y, <);
    UpdateGlobal(global_min_value[2], local_min.z, <);
    UpdateGlobal(global_min_value[3], local_min.w, <);
    
    UpdateGlobal(global_max_value[0], local_max.x, >);
    UpdateGlobal(global_max_value[1], local_max.y, >);
    UpdateGlobal(global_max_value[2], local_max.z, >);
    UpdateGlobal(global_max_value[3], local_max.w, >);
  }
}
)";

    QString buildComputeShader(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format)
    {
        struct TargetVersion {
            QString sampler;
            QString sample;
        };
        struct DataTypeVersion {
            QString prefix;
            QString mapping;
        };
        static auto sTargetVersions = std::map<QOpenGLTexture::Target, TargetVersion>{
            { QOpenGLTexture::Target1D, { "sampler1D", "texelFetch(S, x, uLevel)" } },
            { QOpenGLTexture::Target1DArray, { "sampler1DArray", "texelFetch(S, ivec2(x, uLayer), uLevel)" } },
            { QOpenGLTexture::Target2D, { "sampler2D", "texelFetch(S, ivec2(x, y), uLevel)" } },
            { QOpenGLTexture::Target2DArray, { "sampler2DArray", "texelFetch(S, ivec3(x, y, uLayer), uLevel)" } },
            { QOpenGLTexture::Target3D, { "sampler3D", "texelFetch(S, ivec3(x, y, uLayer), uLevel)" } },
            { QOpenGLTexture::TargetCubeMap, { "samplerCube", "textureLod(S, getCubeTexCoord(vec2(x, y) / vec2(uSize - 1), uFace), uLevel)" } },
            { QOpenGLTexture::TargetCubeMapArray,  { "samplerCubeArray", "textureLod(S, vec4(getCubeTexCoord(vec2(x, y) / vec2(uSize - 1), uFace), uLayer), uLevel)" } },
            { QOpenGLTexture::Target2DMultisample, { "sampler2DMS", "texelFetch(S, ivec2(x, y), uLevel)" } },
            { QOpenGLTexture::Target2DMultisampleArray, { "sampler2DMSArray", "texelFetch(S, ivec3(x, y, uLayer), uLevel)" } },
        };
        static auto sDataTypeVersions = std::map<TextureDataType, DataTypeVersion>{
            { TextureDataType::Normalized, { "", "color" } },
            { TextureDataType::Normalized_sRGB, { "", "vec4(linearToSrgb(color.rgb), color.a)" } },
            { TextureDataType::Float, { "", "color" } },
            { TextureDataType::Uint8, { "u", "color / 255.0" } },
            { TextureDataType::Uint16, { "u", "color / 65535.0" } },
            { TextureDataType::Uint32, { "u", "color / 4294967295.0" } },
            { TextureDataType::Uint_10_10_10_2, { "u", "color / vec4(vec3(1023.0), 3.0)" } },
            { TextureDataType::Int8, { "i", "color / 127.0" } },
            { TextureDataType::Int16, { "i", "color / 32767.0" } },
            { TextureDataType::Int32, { "i", "color / 2147483647.0" } },
            { TextureDataType::Compressed, { "", "color" } },
        };
        static auto sComponetSwizzle = std::map<int, QString>{
            { 1, "vec4(vec3(color.r), 1)" },
            { 2, "vec4(vec3(color.rg, 0), 1)" },
            { 3, "vec4(color.rgb, 1)" },
            { 4, "color" },
        };
        const auto dataType = getTextureDataType(format);
        const auto targetVersion = sTargetVersions[target];
        const auto dataTypeVersion = sDataTypeVersions[dataType];
        const auto swizzle = sComponetSwizzle[getTextureComponentCount(format)];
        return "#version 430\n"
               "#define S uTexture\n"
               "#define SAMPLER " + dataTypeVersion.prefix + targetVersion.sampler + "\n"
               "#define SAMPLE " + targetVersion.sample + "\n" +
               "#define MAPPING " + dataTypeVersion.mapping + "\n" +
               "#define SWIZZLE " + swizzle + "\n" +
               computeShaderSource;
    }

    bool buildProgram(QOpenGLShaderProgram &program,
        QOpenGLTexture::Target target, QOpenGLTexture::TextureFormat format)
    {
        program.create();
        program.addShaderFromSourceCode(QOpenGLShader::Compute, buildComputeShader(target, format));
        return program.link();
    }
} // namespace

ComputeRange::ComputeRange(QObject *parent) : RenderTask(parent)
{
}

ComputeRange::~ComputeRange()
{
    releaseResources();
}

void ComputeRange::setImage(GLuint textureId, 
    const TextureData &texture, int level, int layer, int face) 
{
    mTarget = texture.target();
    mFormat = texture.format();
    mSize = { texture.width(), texture.height() };
    mLevel = level;
    mLayer = layer;
    mFace = face;
    mTextureId = textureId;
}

void ComputeRange::prepare(bool, EvaluationType)
{
}

void ComputeRange::render()
{
#if GL_VERSION_4_3
    auto &gl = GLContext::currentContext();
    if (!gl.v4_3)
        return;

    const auto min = std::numeric_limits<float>::lowest();
    const auto max = std::numeric_limits<float>::max();

    if (auto program = getProgram(mTarget, mFormat)) {
        program->bind();
        program->setUniformValue("uTexture", 0);
        gl.v4_3->glProgramUniform2ui(program->programId(), 
            program->uniformLocation("uSize"), mSize.width(), mSize.height());
        program->setUniformValue("uLevel", mLevel);
        program->setUniformValue("uLayer", mLayer);
        program->setUniformValue("uFace", mFace);
        program->setUniformValue("uMin", min);
        program->setUniformValue("uMax", max);

        struct Result {
            float min[4];
            float max[4];
        };
        if (!mBuffer.isCreated()) {
            mBuffer.setUsagePattern(QOpenGLBuffer::DynamicRead);
            mBuffer.create();
            mBuffer.bind();
            mBuffer.allocate(sizeof(Result));
        }

        auto result = Result{ max, max, max, max, min, min, min, min };
        mBuffer.bind();
        mBuffer.write(0, &result, sizeof(result));

        const auto bindingIndex = 0;
        gl.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 
            bindingIndex, mBuffer.bufferId());

        gl.glActiveTexture(GL_TEXTURE0);
        gl.glBindTexture(mTarget, mTextureId);
        gl.glTexParameteri(mTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(mTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(mTarget, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        gl.v4_3->glDispatchCompute(16, 16, 1);

        mBuffer.read(0, &result, sizeof(result));
        mRange = {
            qMin(qMin(result.min[0], result.min[1]), result.min[2]),
            qMax(qMax(result.max[0], result.max[1]), result.max[2])
        };
    }
#endif
}

void ComputeRange::finish()
{
    Q_EMIT rangeComputed(mRange);
}

void ComputeRange::release()
{
    mPrograms.clear();
}

QOpenGLShaderProgram *ComputeRange::getProgram(QOpenGLTexture::Target target,
    QOpenGLTexture::TextureFormat format)
{
    const auto key = std::make_tuple(target, format);
    auto &program = mPrograms[key];
    if (!program.isLinked())
        if (!buildProgram(program, target, format)) {
            mPrograms.erase(key);
            return nullptr;
        }
    return &program;
}
