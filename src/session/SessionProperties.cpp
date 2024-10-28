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
    mapper.addMapping(mUi->shaderIncludePaths, SessionModel::SessionShaderIncludePaths);
}

void SessionProperties::updateWidgets() { }
