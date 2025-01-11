
#include "SourceEditorToolBar.h"
#include "ui_SourceEditorToolBar.h"
#include "Singletons.h"
#include "Settings.h"
#include <QActionGroup>
#include <QMenu>

SourceEditorToolBar::SourceEditorToolBar(QWidget *parent)
    : QWidget(parent)
    , mUi(new Ui::SourceEditorToolBar())
{
    mUi->setupUi(this);

    const auto sourceTypeMenu = new QMenu(this);
    const auto sourceTypeActionGroup = new QActionGroup(this);
    mUi->sourceTypeButton->setMenu(sourceTypeMenu);

    const auto addSourceType = [&](QMenu *menu, const QString &text,
                                   SourceType sourceType) {
        const auto action = menu->addAction(text);
        action->setData(static_cast<int>(sourceType));
        action->setCheckable(true);
        action->setActionGroup(sourceTypeActionGroup);
    };
    const auto glsl = sourceTypeMenu->addMenu(tr("GLSL"));
    addSourceType(glsl, tr("Vertex Shader"), SourceType::GLSL_VertexShader);
    addSourceType(glsl, tr("Fragment Shader"), SourceType::GLSL_FragmentShader);
    addSourceType(glsl, tr("Geometry Shader"), SourceType::GLSL_GeometryShader);
    addSourceType(glsl, tr("Tessellation Control Shader"),
        SourceType::GLSL_TessControlShader);
    addSourceType(glsl, tr("Tessellation Evaluation Shader"),
        SourceType::GLSL_TessEvaluationShader);
    addSourceType(glsl, tr("Compute Shader"), SourceType::GLSL_ComputeShader);
    addSourceType(glsl, tr("Task Shader"), SourceType::GLSL_TaskShader);
    addSourceType(glsl, tr("Mesh Shader"), SourceType::GLSL_MeshShader);
    addSourceType(glsl, tr("Ray Generation Shader"),
        SourceType::GLSL_RayGenerationShader);
    addSourceType(glsl, tr("Ray Intersection Shader"),
        SourceType::GLSL_RayIntersectionShader);
    addSourceType(glsl, tr("Ray Any Hit Shader"),
        SourceType::GLSL_RayAnyHitShader);
    addSourceType(glsl, tr("Ray Closest Hit Shader"),
        SourceType::GLSL_RayClosestHitShader);
    addSourceType(glsl, tr("Ray Miss Shader"), SourceType::GLSL_RayMissShader);
    addSourceType(glsl, tr("Ray Callable Shader"),
        SourceType::GLSL_RayCallableShader);

    const auto hlsl = sourceTypeMenu->addMenu(tr("HLSL"));
    addSourceType(hlsl, tr("Vertex Shader"), SourceType::HLSL_VertexShader);
    addSourceType(hlsl, tr("Pixel Shader"), SourceType::HLSL_PixelShader);
    addSourceType(hlsl, tr("Geometry Shader"), SourceType::HLSL_GeometryShader);
    addSourceType(hlsl, tr("Hull Shader"), SourceType::HLSL_HullShader);
    addSourceType(hlsl, tr("Domain Shader"), SourceType::HLSL_DomainShader);
    addSourceType(hlsl, tr("Compute Shader"), SourceType::HLSL_ComputeShader);
    addSourceType(hlsl, tr("Amplification Shader"),
        SourceType::HLSL_AmplificationShader);
    addSourceType(hlsl, tr("Mesh Shader"), SourceType::HLSL_MeshShader);
    addSourceType(hlsl, tr("Ray Generation Shader"),
        SourceType::HLSL_RayGenerationShader);
    addSourceType(hlsl, tr("Ray Intersection Shader"),
        SourceType::HLSL_RayIntersectionShader);
    addSourceType(hlsl, tr("Ray Any Hit Shader"),
        SourceType::HLSL_RayAnyHitShader);
    addSourceType(hlsl, tr("Ray Closest Hit Shader"),
        SourceType::HLSL_RayClosestHitShader);
    addSourceType(hlsl, tr("Ray Miss Shader"), SourceType::HLSL_RayMissShader);
    addSourceType(hlsl, tr("Ray Callable Shader"),
        SourceType::HLSL_RayCallableShader);

    addSourceType(sourceTypeMenu, tr("JavaScript"), SourceType::JavaScript);
    sourceTypeMenu->addSeparator();
    addSourceType(sourceTypeMenu, tr("Generic"), SourceType::Generic);
    addSourceType(sourceTypeMenu, tr("Plaintext"), SourceType::PlainText);

    connect(sourceTypeMenu, &QMenu::aboutToShow,
        [this, sourceTypeActionGroup]() {
            const auto actions = sourceTypeActionGroup->actions();
            for (QAction *action : actions)
                action->setChecked(
                    static_cast<SourceType>(action->data().toInt())
                    == sourceType());
        });
    connect(sourceTypeMenu, &QMenu::triggered, this, [this](QAction *action) {
        setSourceType(static_cast<SourceType>(action->data().toInt()));
    });

    connect(mUi->sourceTypeButton, &QToolButton::toggled, this,
        &SourceEditorToolBar::validateSourceChanged);

    const auto &settings = Singletons::settings();
    mUi->lineWrapButton->setChecked(settings.lineWrap());
    connect(mUi->lineWrapButton, &QToolButton::toggled, &settings,
        &Settings::setLineWrap);
    connect(&settings, &Settings::lineWrapChanged, mUi->lineWrapButton,
        &QToolButton::setChecked);
}

SourceEditorToolBar::~SourceEditorToolBar()
{
    delete mUi;
}

bool SourceEditorToolBar::validateSource() const
{
    return mUi->sourceTypeButton->isChecked();
}

void SourceEditorToolBar::setValidateSource(bool validate)
{
    mUi->sourceTypeButton->setChecked(validate);
}

void SourceEditorToolBar::setSourceType(SourceType sourceType)
{
    if (mSourceType != sourceType) {
        mSourceType = sourceType;
        Q_EMIT sourceTypeChanged(sourceType);
    }
}
