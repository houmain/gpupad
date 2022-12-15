#pragma once

#include <QProxyStyle>

class Style : public QProxyStyle
{
    Q_OBJECT

public:
    Style();

    QIcon standardIcon(StandardPixmap standardIcon,
                       const QStyleOption *opt = nullptr,
                       const QWidget *widget = nullptr) const override;

    QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap,
        const QStyleOption *opt) const override;

private:
    QIcon mDockClose;
    QIcon mDockRestore;
};
