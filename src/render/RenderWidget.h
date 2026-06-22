#pragma once

#include "widgets/WindowWidget.h"
#include "AdapterIdentity.h"

class RenderWidget : public WindowWidget
{
    Q_OBJECT
public:
    explicit RenderWidget(bool enableVSync = false, QWidget *parent = nullptr);
    explicit RenderWidget(QWidget *parent) = delete;

    const AdapterIdentity &adapterIdentity() const { return mAdapterIdentity; }
    void setAdapter(const AdapterIdentity &adapterIdentity);
    void redraw();

private:
    const bool mEnableVSync;
    AdapterIdentity mAdapterIdentity;
};
