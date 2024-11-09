#pragma once

#include "SourceType.h"
#include <QWidget>

namespace Ui {
    class SourceEditorToolBar;
}

class SourceEditorToolBar final : public QWidget
{
    Q_OBJECT
public:
    explicit SourceEditorToolBar(QWidget *parent);
    ~SourceEditorToolBar() override;

    SourceType sourceType() const { return mSourceType; }
    void setSourceType(SourceType sourceType);
    bool validateSource() const;
    void setValidateSource(bool validate);
    bool showWhiteSpace() const;
    void setShowWhiteSpace(bool how);

Q_SIGNALS:
    void sourceTypeChanged(SourceType sourceType);
    void validateSourceChanged(bool validate);

private:
    Ui::SourceEditorToolBar *mUi;
    SourceType mSourceType{};
};
