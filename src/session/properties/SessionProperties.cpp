#include "SessionProperties.h"
#include "../SessionModel.h"
#include "PropertiesEditor.h"
#include "ui_SessionProperties.h"
#include <QDataWidgetMapper>
#include <QStringListModel>

VariantMapModel::VariantMapModel(QObject *parent) : QAbstractItemModel(parent)
{
}

QModelIndex VariantMapModel::index(int row, int column,
    const QModelIndex &parent) const
{
    return createIndex(row, column, nullptr);
}

QModelIndex VariantMapModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}

int VariantMapModel::rowCount(const QModelIndex &parent) const
{
    return 1;
}

QVariant VariantMapModel::data(const QModelIndex &index, int role) const
{
    if (index.row() != 0 || (role != Qt::EditRole && role != Qt::DisplayRole))
        return {};

    const auto it = mVariantMap.find(getColumnKey(index.column()));
    if (it == mVariantMap.end())
        return getColumnDefaultValue(index.column());

    return it.value();
}

bool VariantMapModel::setData(const QModelIndex &index, const QVariant &value,
    int role)
{
    if (index.row() != 0 || role != Qt::EditRole)
        return {};

    const auto key = getColumnKey(index.column());
    if (key.isEmpty())
        return false;

    mVariantMap[key] = value;
    return true;
}

//-------------------------------------------------------------------------

int ShaderCompilerSettingsModel::columnCount(const QModelIndex &parent) const
{
    return Session::ShaderCompilerSetting::COUNT;
}

QString ShaderCompilerSettingsModel::getColumnKey(int column) const
{
    const auto metaEnum = QMetaEnum::fromType<Session::ShaderCompilerSetting>();
    if (auto key = metaEnum.key(column))
        return key;
    return {};
}

QVariant ShaderCompilerSettingsModel::getColumnDefaultValue(int column) const
{
    auto setting = static_cast<Session::ShaderCompilerSetting>(column);
    switch (setting) {
    case Session::ShaderCompilerSetting::autoMapBindings:
    case Session::ShaderCompilerSetting::autoMapLocations:
    case Session::ShaderCompilerSetting::autoSampledTextures:
    case Session::ShaderCompilerSetting::vulkanRulesRelaxed:  return true;
    case Session::ShaderCompilerSetting::spirvVersion:        return {};
    }
    return {};
}

//-------------------------------------------------------------------------

SessionProperties::SessionProperties(PropertiesEditor *propertiesEditor)
    : QWidget(propertiesEditor)
    , mPropertiesEditor(*propertiesEditor)
    , mUi(new Ui::SessionProperties)
    , mShaderCompilerSettingsModel(new ShaderCompilerSettingsModel(this))
    , mShaderCompilerSettingsMapper(new QDataWidgetMapper(this))
{
    mUi->setupUi(this);

    fillComboBox<Session::Renderer>(mUi->renderer, true);
    fillComboBox<Session::ShaderLanguage>(mUi->shaderLanguage);
    removeComboBoxItem(mUi->shaderLanguage, "None");
    // TODO: implement Slang
    removeComboBoxItem(mUi->shaderLanguage, "Slang");

    mShaderCompilerSettingsMapper->setModel(mShaderCompilerSettingsModel);
    mShaderCompilerSettingsMapper->setCurrentModelIndex(
        mShaderCompilerSettingsModel->index(0, 0));

    mShaderCompilerSettingsMapper->addMapping(mUi->spirvVersion,
        Session::ShaderCompilerSetting::spirvVersion);
    mShaderCompilerSettingsMapper->addMapping(mUi->autoMapBindings,
        Session::ShaderCompilerSetting::autoMapBindings);
    mShaderCompilerSettingsMapper->addMapping(mUi->autoMapLocations,
        Session::ShaderCompilerSetting::autoMapLocations);
    mShaderCompilerSettingsMapper->addMapping(mUi->autoSampledTextures,
        Session::ShaderCompilerSetting::autoSampledTextures);
    mShaderCompilerSettingsMapper->addMapping(mUi->vulkanRulesRelaxed,
        Session::ShaderCompilerSetting::vulkanRulesRelaxed);

    mUi->spirvVersion->addItem("1.0", 10);
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

void SessionProperties::submitShaderCompilerSettings()
{
    mShaderCompilerSettingsMapper->submit();

    mPropertiesEditor.model().setData(
        mPropertiesEditor.currentModelIndex(
            SessionModel::SessionShaderCompilerSettings),
        mShaderCompilerSettingsModel->variantMap());
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
                // TODO: implement DXC -> SpirV
                //{ "DXC", Session::ShaderCompiler::DXC },
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

    const auto currentIndex = mPropertiesEditor.currentModelIndex(
        SessionModel::SessionShaderCompilerSettings);
    mShaderCompilerSettingsModel->setVariantMap(
        mPropertiesEditor.model().data(currentIndex).toMap());
    mShaderCompilerSettingsMapper->revert();

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

    const auto hasOpenGLRenderer = (renderer == Session::Renderer::OpenGL);
    const auto hasShaderCompiler =
        (shaderCompiler != Session::ShaderCompiler::Driver);

    setFormVisibility(mUi->formLayout, mUi->labelShaderCompiler,
        mUi->shaderCompiler, (language != Session::ShaderLanguage::Slang));

    mUi->rendererOptions->setVisible(!hasOpenGLRenderer);
    mUi->shaderCompilerSettings->setVisible(hasShaderCompiler);

    using SCS = Session::ShaderCompilerSetting;
    const auto hasSetting = [&](SCS setting) {
        return shaderCompilerHasSetting(shaderCompiler, renderer, setting);
    };
    setFormVisibility(mUi->shaderCompilerSettingsLayout, mUi->labelSprivVersion,
        mUi->spirvVersion, hasSetting(SCS::spirvVersion));
    mUi->autoMapBindings->setVisible(hasSetting(SCS::autoMapBindings));
    mUi->autoMapLocations->setVisible(hasSetting(SCS::autoMapLocations));
    mUi->autoSampledTextures->setVisible(hasSetting(SCS::autoSampledTextures));
    mUi->vulkanRulesRelaxed->setVisible(hasSetting(SCS::vulkanRulesRelaxed));
}
