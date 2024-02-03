
#include "spirvCross.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "spirv_glsl.hpp"
#include "spirv_reflect.hpp"

#if defined(interface)
#  undef interface
#endif

namespace spirvCross
{

namespace {
    spv::ExecutionModel getExecutionModel(Shader::ShaderType shaderType)
    {
        switch (shaderType) {
            case Shader::ShaderType::Vertex: return spv::ExecutionModelVertex;
            case Shader::ShaderType::Fragment: return spv::ExecutionModelFragment;
            case Shader::ShaderType::Geometry: return spv::ExecutionModelGeometry;
            case Shader::ShaderType::TessellationControl: return spv::ExecutionModelTessellationControl;
            case Shader::ShaderType::TessellationEvaluation: return spv::ExecutionModelTessellationEvaluation;
            case Shader::ShaderType::Compute: return spv::ExecutionModelGLCompute;
            case Shader::ShaderType::Includable: break;
        }
        return { };
    }

    QString patchGLSL(QString glsl) 
    {
        // WORKAROUND - is there no simpler way?
        // turn:
        //   layout(binding = 0, std140) uniform _Global
        //   {
        //       layout(row_major) mat4 viewProj;
        //   } _35;
        //   ...
        //   _35.viewProj
        //
        // into:
        //   uniform mat4 viewProj;
        //   ...
        //   viewProj
        //
        auto i = glsl.indexOf("uniform _Global");
        if (i >= 0) {
            auto lineBegin = glsl.lastIndexOf('\n', i) + 1;
            glsl.insert(lineBegin, "//");
            for (auto level = 0; i < glsl.size(); ++i) {
                if (glsl[i] == '{') {
                    if (level == 0) {
                        glsl.insert(i, "//");
                        i += 2;
                    }
                    ++level;
                }
                else if (glsl[i] == 'l' && glsl.indexOf("layout(", i) == i) {
                    lineBegin = i;
                    glsl.remove(i, glsl.indexOf(')', i) - i + 1);
                }
                else if (glsl[i] == '\n') {
                    lineBegin = i + 1;
                }
                else if (glsl[i] == ';') {
                    glsl.insert(lineBegin, "uniform ");
                    i += 8;
                }
                else if (glsl[i] == '}') {
                    --level;
                    if (level == 0) {
                        glsl.insert(i, "//");
                        i += 4;
                        const auto var = glsl.mid(i, glsl.indexOf(';', i) - i);
                        glsl.remove(var + ".");
                        break;
                    }
                }
            }
        }
        return glsl;
    }
} // namespace

Interface getInterface(const std::vector<uint32_t> &spirv)
{
    auto reflection = spirv_cross::CompilerReflection(spirv);
    const auto json = reflection.compile();
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(json)).object();
    auto interface = Interface{ };

    struct Type
    {
        QString name;
        QList<Interface::Field> members;
    };
    auto types = QMap<QString, Type>();
    for (const auto &key : document.keys()) {
        if (key == "types") {
            auto items = document[key].toObject();
            for (auto id : items.keys()) {
                auto item = items[id].toObject();
                auto type = Type{ };
                type.name = item["name"].toString();
                for (auto member : item["members"].toArray()) {
                    auto object = member.toObject();
                    type.members.append(Interface::Field{
                        object["type"].toString(),
                        object["name"].toString(),
                        object["offset"].toInt(),
                        object["matrix_stride"].toInt(),
                    });
                }
                types[id] = type;
            }            
        }
    }

    for (const auto &key : document.keys()) {
        if (key == "entryPoints") {
            for (auto item : document[key].toArray()) {
                auto object = item.toObject();
                interface.entryPoints.append({
                    object["mode"].toString(),
                    object["name"].toString(),
                });
            }
        }
        else if (key == "inputs") {
            for (auto item : document[key].toArray()) {
                auto object = item.toObject();
                interface.inputs.append({
                    object["type"].toString(),
                    object["name"].toString(),
                    static_cast<uint32_t>(object["location"].toInt()),
                });
            }
        }
        else if (key == "outputs") {
            for (auto item : document[key].toArray()) {
                auto object = item.toObject();
                interface.outputs.append({
                    object["type"].toString(),
                    object["name"].toString(),
                    static_cast<uint32_t>(object["location"].toInt()),
                });
            }
        }
        else if (key == "textures") {
            for (auto item : document[key].toArray()) {
                auto object = item.toObject();
                interface.textures.append({
                    object["type"].toString(),
                    object["name"].toString(),
                    static_cast<uint32_t>(object["set"].toInt()),
                    static_cast<uint32_t>(object["binding"].toInt()),
                });
            }
        }
        else if (key == "images") {
            for (auto item : document[key].toArray()) {
                auto object = item.toObject();
                interface.images.append({
                    object["type"].toString(),
                    object["name"].toString(),
                    static_cast<uint32_t>(object["set"].toInt()),
                    static_cast<uint32_t>(object["binding"].toInt()),
                    object["format"].toString(),
                });
            }
        }
        else if (key == "ubos") {
            for (auto item : document[key].toArray()) {
                auto object = item.toObject();
                const auto &type = types[object["type"].toString()];
                interface.buffers.append({
                    type.name,
                    type.members,
                    object["name"].toString(),
                    object["blockSize"].toInt(),
                    static_cast<uint32_t>(object["set"].toInt()),
                    static_cast<uint32_t>(object["binding"].toInt()),
                });
            }
        }
    }
    return interface;
}

QString generateGLSL(std::vector<uint32_t> spirv, const QString &fileName, MessagePtrSet &messages)
{
    try {
        auto compiler = spirv_cross::CompilerGLSL(std::move(spirv));
        auto options = spirv_cross::CompilerGLSL::Options{ };
        options.emit_line_directives = true;
        compiler.set_common_options(options);

        compiler.build_combined_image_samplers();
        for (auto &remap : compiler.get_combined_image_samplers())
            compiler.set_name(remap.combined_id,
              compiler.get_name(remap.image_id) + "_" +
                    compiler.get_name(remap.sampler_id));

        return patchGLSL(QString::fromStdString(compiler.compile()));
    }
    catch (const std::exception &ex) {
        messages += MessageList::insert(fileName, -1,
            MessageType::ShaderError, ex.what());
    }
    return { };
}

} // namespace
