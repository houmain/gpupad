
#include "SourceEditorToolBar.h"
#include "ui_SourceEditorToolBar.h"
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
    mUi->sourceTypeButton->setPopupMode(QToolButton::MenuButtonPopup);
    mUi->sourceTypeButton->setIcon(QIcon(":images/16x16/help-faq.png"));

    const auto addSourceType = [&](QMenu* menu, const QString &text, SourceType sourceType) {
        const auto action = menu->addAction(text);
        action->setData(static_cast<int>(sourceType));
        action->setCheckable(true);
        action->setActionGroup(sourceTypeActionGroup);
    };
    const auto glsl = sourceTypeMenu->addMenu(tr("GLSL"));
    addSourceType(glsl, tr("Vertex Shader"), SourceType::GLSL_VertexShader);
    addSourceType(glsl, tr("Fragment Shader"), SourceType::GLSL_FragmentShader);
    addSourceType(glsl, tr("Geometry Shader"), SourceType::GLSL_GeometryShader);
    addSourceType(glsl, tr("Tessellation Control"), SourceType::GLSL_TessellationControl);
    addSourceType(glsl, tr("Tessellation Evaluation"), SourceType::GLSL_TessellationEvaluation);
    addSourceType(glsl, tr("Compute Shader"), SourceType::GLSL_ComputeShader);

    const auto hlsl = sourceTypeMenu->addMenu(tr("HLSL"));
    addSourceType(hlsl, tr("Vertex Shader"), SourceType::HLSL_VertexShader);
    addSourceType(hlsl, tr("Pixel Shader"), SourceType::HLSL_PixelShader);
    addSourceType(hlsl, tr("Geometry Shader"), SourceType::HLSL_GeometryShader);
    addSourceType(hlsl, tr("Hull Shader"), SourceType::HLSL_HullShader);
    addSourceType(hlsl, tr("Domain Shader"), SourceType::HLSL_DomainShader);
    addSourceType(hlsl, tr("Compute Shader"), SourceType::HLSL_ComputeShader);

    addSourceType(sourceTypeMenu, tr("JavaScript"), SourceType::JavaScript);
    addSourceType(sourceTypeMenu, tr("Plaintext"), SourceType::PlainText);

    connect(sourceTypeMenu, &QMenu::aboutToShow,
        [this, sourceTypeActionGroup]() {
            const auto actions = sourceTypeActionGroup->actions();
            for (QAction *action : actions)
                action->setChecked(static_cast<SourceType>(
                    action->data().toInt()) == sourceType());
        });
    connect(sourceTypeMenu, &QMenu::triggered, this, 
        [this](QAction *action) {
            setSourceType(static_cast<SourceType>(action->data().toInt()));
        });

    connect(mUi->sourceTypeButton, &QToolButton::toggled,
        this, &SourceEditorToolBar::validateSourceChanged);
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
