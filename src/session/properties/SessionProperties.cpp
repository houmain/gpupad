#include "SessionProperties.h"
#include "../SessionModel.h"
#include "PropertiesEditor.h"
#include "ui_SessionProperties.h"
#include "Singletons.h"
#if defined(OPENGL_ENABLED)
#  include "render/opengl/GLWindow.h"
#endif
#if defined(VULKAN_ENABLED)
#  include "render/vulkan/VKWindow.h"
#endif
#include <QDataWidgetMapper>
#include <QSignalBlocker>
#include <QStringListModel>
#include <algorithm>

namespace {
    bool isNull(const AdapterIdentity::UUID &uuid)
    {
        return std::ranges::all_of(uuid, [](auto byte) { return byte == 0; });
    }

    bool isNull(const AdapterIdentity::LUID &luid)
    {
        return std::ranges::all_of(luid, [](auto byte) { return byte == 0; });
    }

    bool isSameAdapter(const AdapterIdentity &a, const AdapterIdentity &b)
    {
        if (a == b)
            return true;

        if (!isNull(a.deviceLUID) && a.deviceLUID == b.deviceLUID)
            return true;

        if (!isNull(a.driverUUID) && a.driverUUID == b.driverUUID)
            for (const auto &aDeviceUUID : a.deviceUUIDs)
                if (!isNull(aDeviceUUID))
                    for (const auto &bDeviceUUID : b.deviceUUIDs)
                        if (aDeviceUUID == bDeviceUUID)
                            return true;

        return false;
    }

    QString getAdapterName(const AdapterIdentity &adapter, int index)
    {
        if (!adapter.name.isEmpty())
            return adapter.name;
        return QString("Adapter %1").arg(index + 1);
    }
} // namespace

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
    using enum Session::ShaderCompilerSetting;
    auto setting = static_cast<Session::ShaderCompilerSetting>(column);
    switch (setting) {
    case autoMapBindings:
    case autoMapLocations:
    case autoSampledTextures:
    case vulkanRulesRelaxed:  return true;
    case spirvVersion:        return {};
    case COUNT:               break;
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

    for (auto i = 1; i <= 4; ++i) {
        auto version = QString("1.%1").arg(i);
        mUi->apiVersion->addItem(version, version);
    }

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

    for (auto i = 0; i <= 6; ++i) {
        auto version = QString("1.%1").arg(i);
        mUi->spirvVersion->addItem(version, version);
    }

    connect(mUi->renderer, &DataComboBox::currentDataChanged, this,
        &SessionProperties::updateShaderCompiler);
    connect(mUi->apiVersion, &DataComboBox::currentDataChanged, this,
        &SessionProperties::updateAdapters);
    connect(mUi->adapter, &DataComboBox::currentDataChanged, this,
        &SessionProperties::selectAdapter);
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
    mapper.addMapping(mUi->apiVersion, SessionModel::SessionApiVersion);
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

    mPropertiesEditor.model().setField(mPropertiesEditor.currentModelIndex(),
        SessionModel::SessionShaderCompilerSettings,
        mShaderCompilerSettingsModel->variantMap());
}

void SessionProperties::updateAdapters()
{
    const auto renderer =
        static_cast<Session::Renderer>(mUi->renderer->currentData().toInt());

    mAdapters.clear();
    switch (renderer) {
    case Session::Renderer::OpenGL:
#if defined(OPENGL_ENABLED)
        mAdapters.append(GLWindow::getAdapterIdentity());
#endif
        break;

    case Session::Renderer::Vulkan:
    case Session::Renderer::Direct3D:
#if defined(VULKAN_ENABLED)
        mAdapters = VKWindow::getAdapterIdentities();
#endif
        break;
    }
    if (mAdapters.isEmpty())
        mAdapters.push_back(AdapterIdentity{ "Default" });

    {
        const auto signalBlocker = QSignalBlocker(mUi->adapter);
        mUi->adapter->clear();
        for (auto i = 0; i < mAdapters.size(); ++i)
            mUi->adapter->addItem(getAdapterName(mAdapters[i], i), i);

        for (auto i = 0; i < mAdapters.size(); ++i)
            if (isSameAdapter(mAdapters[i], Singletons::selectedAdapter())) {
                mUi->adapter->setCurrentIndex(i);
                break;
            }
    }
    selectAdapter(mUi->adapter->currentData());
}

void SessionProperties::selectAdapter(QVariant data)
{
    const auto apiVersion = mUi->apiVersion->currentData().toString();
    const auto adapterIndex = data.toInt();
    if (adapterIndex < mAdapters.size())
        Singletons::selectAdapter(mAdapters[adapterIndex], apiVersion);
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
#if defined(DXC_ENABLED)
                { "DXC", Session::ShaderCompiler::DXC },
#endif
                { "D3DCompiler", Session::ShaderCompiler::D3DCompiler },
#if !defined(DXC_ENABLED)
                { "DXC", Session::ShaderCompiler::DXC },
#endif
            };
        }
        return {};
    };

    fillComboBox<Session::ShaderCompiler>(mUi->shaderCompiler,
        getShaderCompilers());

    mShaderCompilerSettingsModel->setVariantMap(mPropertiesEditor.model()
            .data(mPropertiesEditor.currentModelIndex(),
                SessionModel::SessionShaderCompilerSettings)
            .toMap());
    mShaderCompilerSettingsMapper->revert();

    updateAdapters();
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
        (shaderCompiler == Session::ShaderCompiler::glslang);

    setFormVisibility(mUi->formLayout, mUi->labelApiVersion, mUi->apiVersion,
        (renderer == Session::Renderer::Vulkan));
    setFormVisibility(mUi->formLayout, mUi->labelAdapter, mUi->adapter, true);
    setFormVisibility(mUi->formLayout, mUi->labelShaderLanguage,
        mUi->shaderLanguage, true);
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
