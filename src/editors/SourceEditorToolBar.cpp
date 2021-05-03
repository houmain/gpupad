
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

    const auto addSourceType = [&](const QString &text, SourceType sourceType) {
        const auto action = sourceTypeMenu->addAction(text);
        action->setData(static_cast<int>(sourceType));
        action->setCheckable(true);
        action->setActionGroup(sourceTypeActionGroup);
    };
    addSourceType(tr("Vertex Shader"), SourceType::VertexShader);
    addSourceType(tr("Fragment Shader"), SourceType::FragmentShader);
    addSourceType(tr("Geometry Shader"), SourceType::GeometryShader);
    addSourceType(tr("Tessellation Control"), SourceType::TessellationControl);
    addSourceType(tr("Tessellation Evaluation"), SourceType::TessellationEvaluation);
    addSourceType(tr("Compute Shader"), SourceType::ComputeShader);
    sourceTypeMenu->addSeparator();
    addSourceType(tr("JavaScript"), SourceType::JavaScript);
    addSourceType(tr("Plaintext"), SourceType::PlainText);

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
