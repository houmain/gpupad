#include "SessionProperties.h"
#include "SessionModel.h"
#include "PropertiesEditor.h"
#include "ui_SessionProperties.h"
#include "render/Renderer.h"
#include <QDataWidgetMapper>

SessionProperties::SessionProperties(PropertiesEditor *propertiesEditor)
    : QWidget(propertiesEditor)
    , mPropertiesEditor(*propertiesEditor)
    , mUi(new Ui::SessionProperties)
{
    mUi->setupUi(this);

    mUi->renderer->addItem("OpenGL", "OpenGL");
    mUi->renderer->addItem("Vulkan", "Vulkan");

    connect(mUi->renderer, &DataComboBox::currentDataChanged, this,
        &SessionProperties::updateWidgets);

    updateWidgets();
}

SessionProperties::~SessionProperties()
{
    delete mUi;
}

void SessionProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->renderer, SessionModel::SessionRenderer);
    mapper.addMapping(mUi->shaderPreamble, SessionModel::SessionShaderPreamble);
    mapper.addMapping(mUi->shaderIncludePaths,
        SessionModel::SessionShaderIncludePaths);

    mapper.addMapping(mUi->autoMapBindings,
        SessionModel::SessionAutoMapBindings);
    mapper.addMapping(mUi->autoMapLocations,
        SessionModel::SessionAutoMapLocations);
    mapper.addMapping(mUi->vulkanRulesRelaxed,
        SessionModel::SessionVulkanRulesRelaxed);
}

void SessionProperties::updateWidgets()
{
    const auto renderer = mUi->renderer->currentData().toString();
    const auto hasVulkanRenderer = (renderer == "Vulkan");
    mUi->shaderCompilerOptions->setVisible(hasVulkanRenderer);
}
