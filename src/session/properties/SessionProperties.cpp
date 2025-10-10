#include "SessionProperties.h"
#include "../SessionModel.h"
#include "PropertiesEditor.h"
#include "ui_SessionProperties.h"
#include <QDataWidgetMapper>

SessionProperties::SessionProperties(PropertiesEditor *propertiesEditor)
    : QWidget(propertiesEditor)
    , mPropertiesEditor(*propertiesEditor)
    , mUi(new Ui::SessionProperties)
{
    mUi->setupUi(this);

    fillComboBox<Session::Renderer>(mUi->renderer, true);
    fillComboBox<Session::ShaderLanguage>(mUi->shaderLanguage);
    removeComboBoxItem(mUi->shaderLanguage, "None");

    mUi->spirvVersion->addItem("1.0", 0);
    mUi->spirvVersion->addItem("1.1", 11);
    mUi->spirvVersion->addItem("1.2", 12);
    mUi->spirvVersion->addItem("1.3", 13);
    mUi->spirvVersion->addItem("1.4", 14);
    mUi->spirvVersion->addItem("1.5", 15);
    mUi->spirvVersion->addItem("1.6", 16);

    connect(mUi->renderer, &DataComboBox::currentDataChanged, this,
        &SessionProperties::updateShaderCompiler);
    connect(mUi->shaderLanguage, &DataComboBox::currentDataChanged, this,
        &SessionProperties::updateShaderCompiler);
    connect(mUi->shaderCompiler, &DataComboBox::currentDataChanged, this,
        &SessionProperties::updateWidgets);

    updateShaderCompiler();
    updateWidgets();
}

SessionProperties::~SessionProperties()
{
    delete mUi;
}

void SessionProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->name, SessionModel::Name);
    mapper.addMapping(mUi->renderer, SessionModel::SessionRenderer);
    mapper.addMapping(mUi->shaderLanguage, SessionModel::SessionShaderLanguage);
    mapper.addMapping(mUi->shaderCompiler, SessionModel::SessionShaderCompiler);
    mapper.addMapping(mUi->shaderPreamble, SessionModel::SessionShaderPreamble);
    mapper.addMapping(mUi->shaderIncludePaths,
        SessionModel::SessionShaderIncludePaths);

    mapper.addMapping(mUi->reverseCulling, SessionModel::SessionReverseCulling);
    mapper.addMapping(mUi->flipViewport, SessionModel::SessionFlipViewport);
}

void SessionProperties::updateShaderCompiler()
{
    const auto renderer =
        static_cast<Session::Renderer>(mUi->renderer->currentData().toInt());
    const auto language = static_cast<Session::ShaderLanguage>(
        mUi->shaderLanguage->currentData().toInt());

    const auto getShaderCompilers =
        [&]() -> std::vector<std::pair<const char *, Session::ShaderCompiler>> {
        if (language == Session::ShaderLanguage::Slang)
            return {
                { "Slang", Session::ShaderCompiler::Slang },
            };

        switch (renderer) {
        case Session::Renderer::OpenGL:
            if (language == Session::ShaderLanguage::GLSL)
                return {
                    { "Driver", Session::ShaderCompiler::Driver },
                    { "glslang", Session::ShaderCompiler::glslang },
                };
            return {
                { "glslang", Session::ShaderCompiler::glslang },
            };

        case Session::Renderer::Vulkan:
            if (language == Session::ShaderLanguage::GLSL)
                return {
                    { "glslang", Session::ShaderCompiler::glslang },
                };
            return {
                { "glslang", Session::ShaderCompiler::glslang },
                { "DXC", Session::ShaderCompiler::DXC },
            };

        case Session::Renderer::Direct3D:
            if (language == Session::ShaderLanguage::GLSL)
                return {
                    { "glslang / SPIRV-Cross",
                        Session::ShaderCompiler::glslang },
                };
            return {
                { "DXC", Session::ShaderCompiler::DXC },
                { "D3DCompiler", Session::ShaderCompiler::D3DCompiler },
            };
        }
        return {};
    };

    fillComboBox<Session::ShaderCompiler>(mUi->shaderCompiler,
        getShaderCompilers());

    updateWidgets();
}

void SessionProperties::updateWidgets()
{
    const auto renderer =
        static_cast<Session::Renderer>(mUi->renderer->currentData().toInt());
    const auto language = static_cast<Session::ShaderLanguage>(
        mUi->shaderLanguage->currentData().toInt());
    const auto shaderCompiler = static_cast<Session::ShaderCompiler>(
        mUi->shaderCompiler->currentData().toInt());

    const auto hasVulkanRenderer = (renderer == Session::Renderer::Vulkan);
    const auto hasOpenGLRenderer = (renderer == Session::Renderer::OpenGL);
    const auto hasShaderCompiler = (hasVulkanRenderer
        || shaderCompiler != Session::ShaderCompiler::Driver);

    setFormVisibility(mUi->formLayout, mUi->labelShaderCompiler,
        mUi->shaderCompiler, (language != Session::ShaderLanguage::Slang));

    mUi->rendererOptions->setVisible(!hasOpenGLRenderer);
    mUi->shaderCompilerOptions->setVisible(hasShaderCompiler);
    mUi->autoMapBindings->setVisible(!hasOpenGLRenderer);
    mUi->autoMapLocations->setVisible(!hasOpenGLRenderer);
    mUi->vulkanRulesRelaxed->setVisible(hasVulkanRenderer);
}
