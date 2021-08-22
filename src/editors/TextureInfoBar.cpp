#include "TextureInfoBar.h"
#include "ui_TextureInfoBar.h"
#include <QVector4D>

TextureInfoBar::TextureInfoBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TextureInfoBar)
{
    ui->setupUi(this);

    connect(ui->closeButton, &QPushButton::clicked, this,
        &TextureInfoBar::cancelled);
}

TextureInfoBar::~TextureInfoBar()
{
    delete ui;
}

void TextureInfoBar::setMousePosition(const QPointF &mousePosition)
{
    ui->mousePositionX->setText(QStringLiteral("x: %1").arg(mousePosition.x()));
    ui->mousePositionY->setText(QStringLiteral("y: %1").arg(mousePosition.y()));
}

void TextureInfoBar::setPickerColor(const QVector4D &color)
{
    const auto map = [](auto v) { return static_cast<int>(qRound(v * 255)); };
    ui->pickerColorR->setText(QStringLiteral("R: %1").arg(map(color.x())));
    ui->pickerColorG->setText(QStringLiteral("G: %1").arg(map(color.y())));
    ui->pickerColorB->setText(QStringLiteral("B: %1").arg(map(color.z())));
    ui->pickerColorA->setText(QStringLiteral("A: %1").arg(map(color.w())));

    ui->color->setStyleSheet("background: " +
        QColor::fromRgbF(color.x(), color.y(), color.z(), color.w()).name(QColor::HexRgb));
}

void TextureInfoBar::setPickerEnabled(bool enabled)
{
    if (mIsPickerEnabled != enabled) {
        mIsPickerEnabled = enabled;
        Q_EMIT pickerEnabledChanged(enabled);
    }
}
