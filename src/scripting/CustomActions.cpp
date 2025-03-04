#include "CustomActions.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "ScriptEngineJavaScript.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "editors/EditorManager.h"
#include "ui_CustomActions.h"
#include "objects/AppScriptObject.h"
#include "objects/SessionScriptObject.h"
#include <QAction>
#include <QFileSystemModel>
#include <QInputDialog>
#include <QListView>
#include <QMessageBox>
#include <QStandardPaths>

class CustomAction final : public QAction
{
public:
    explicit CustomAction(const QString &filePath) : mFilePath(filePath)
    {
        auto source = QString();
        if (Singletons::fileCache().getSource(mFilePath, &source)) {
            mScriptEngine = std::make_unique<ScriptEngineJavaScript>();
            mScriptEngine->setTimeout(5000);
            mAppScriptObject = new AppScriptObject(mScriptEngine->jsEngine());
            mScriptEngine->setGlobal("app", mAppScriptObject);
            mScriptEngine->evaluateScript(source, mFilePath, mMessages);
        }

        auto name = mScriptEngine->getGlobal("name");
        setText(name.isUndefined() ? QFileInfo(mFilePath).baseName()
                                   : name.toString());
    }

    bool isApplicable(const QModelIndexList &selection, MessagePtrSet &messages)
    {
        auto applicable = mScriptEngine->getGlobal("applicable");
        if (applicable.isCallable()) {
            auto result = mScriptEngine->call(applicable,
                { getItems(selection) }, 0, messages);
            if (result.isBool())
                return result.toBool();
            return false;
        }
        return true;
    }

    void apply(const QModelIndexList &selection, MessagePtrSet &messages)
    {
        auto apply = mScriptEngine->getGlobal("apply");
        if (apply.isCallable()) {
            mScriptEngine->call(apply, { getItems(selection) }, 0, messages);
        }
    }

private:
    QJSValue getItems(const QModelIndexList &selectedIndices)
    {
        auto array =
            mScriptEngine->jsEngine()->newArray(selectedIndices.size());
        auto i = 0;
        for (const auto &index : selectedIndices)
            array.setProperty(i++,
                mAppScriptObject->sessionScriptObject().getItem(index));
        return array;
    }

    const QString mFilePath;
    MessagePtrSet mMessages;
    std::unique_ptr<ScriptEngineJavaScript> mScriptEngine;
    AppScriptObject *mAppScriptObject{};
};

CustomActions::CustomActions(QWidget *parent)
    : QDialog(parent, Qt::Tool)
    , ui(new Ui::CustomActions)
    , mModel(new QFileSystemModel(this))
{
    ui->setupUi(this);
    connect(ui->close, &QPushButton::clicked, this, &QDialog::close);

    mModel->setNameFilterDisables(false);
    mModel->setReadOnly(false);
    mModel->setFilter(QDir::Files);
    mModel->setNameFilters({ "*.js" });
    ui->actions->setModel(mModel);

    mModel->setRootPath(getUserDirectory("actions").path());
    ui->actions->setRootIndex(mModel->index(mModel->rootPath()));

    connect(ui->actions, &QListView::activated, this,
        &CustomActions::editAction);
    connect(ui->newAction, &QPushButton::clicked, this,
        &CustomActions::newAction);
    connect(ui->importAction, &QPushButton::clicked, this,
        &CustomActions::importAction);
    connect(ui->editAction, &QPushButton::clicked, this,
        &CustomActions::editAction);
    connect(ui->deleteAction, &QPushButton::clicked, this,
        &CustomActions::deleteAction);
    connect(ui->actions->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &CustomActions::updateWidgets);
}

CustomActions::~CustomActions()
{
    delete ui;
}

void CustomActions::newAction()
{
    auto ok = false;
    auto filePath =
        QDir(mModel->rootPath())
            .filePath(QInputDialog::getText(this, tr("New Custom Action..."),
                tr("Enter the new name:") + "                   ",
                QLineEdit::Normal, "", &ok));
    if (ok && !filePath.isEmpty()) {
        if (QFileInfo(filePath).suffix().isEmpty())
            filePath += ".js";
        QFile(filePath).open(QFile::WriteOnly | QFile::Append);
    }
}

void CustomActions::importAction()
{
    auto options = FileDialog::Options(FileDialog::Importing
        | FileDialog::ScriptExtensions);
    if (Singletons::fileDialog().exec(options)) {
        auto source = Singletons::fileDialog().fileName();
        auto fileName = QFileInfo(source).fileName();
        auto dest = QDir(mModel->rootPath()).filePath(fileName);
        if (QFileInfo(dest).isFile()) {
            if (QMessageBox::question(this, tr("Confirm to replace file"),
                    tr("Do you want to replace '%1'?").arg(fileName),
                    QMessageBox::Yes | QMessageBox::Cancel)
                != QMessageBox::Yes)
                return;
            QFile::remove(dest);
        }
        QFile::copy(source, dest);
    }
}

void CustomActions::editAction()
{
    const auto filePath = toNativeCanonicalFilePath(
        mModel->fileInfo(ui->actions->currentIndex()).filePath());
    close();
    Singletons::editorManager().openEditor(filePath);
}

void CustomActions::deleteAction()
{
    auto filePath = mModel->fileInfo(ui->actions->currentIndex()).filePath();
    if (QMessageBox::question(this, tr("Confirm to delete file"),
            tr("Do you really want to delete '%1'?")
                .arg(QFileInfo(filePath).fileName()),
            QMessageBox::Yes | QMessageBox::Cancel)
        != QMessageBox::Yes)
        return;

    QFile::remove(filePath);
}

void CustomActions::actionTriggered()
{
    auto &action = static_cast<CustomAction &>(
        *qobject_cast<QAction *>(QObject::sender()));

    mMessages.clear();
    action.apply(mSelection, mMessages);
}

void CustomActions::updateWidgets()
{
    const auto hasSelection = ui->actions->currentIndex().isValid();
    ui->editAction->setEnabled(hasSelection);
    ui->deleteAction->setEnabled(hasSelection);
}

void CustomActions::updateActions()
{
    Singletons::fileCache().updateFromEditors();

    mActions.clear();
    auto root = mModel->index(mModel->rootPath());
    for (auto i = 0; i < mModel->rowCount(root); i++) {
        const auto filePath = toNativeCanonicalFilePath(
            mModel->filePath(mModel->index(i, 0, root)));
        mActions.emplace_back(new CustomAction(filePath));
        connect(mActions.back().get(), &QAction::triggered, this,
            &CustomActions::actionTriggered);
    }
}

void CustomActions::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    activateWindow();
}

void CustomActions::setSelection(const QModelIndexList &selection)
{
    mSelection = selection;
}

QList<QAction *> CustomActions::getApplicableActions()
{
    updateActions();

    auto actions = QList<QAction *>();
    for (const auto &action : mActions)
        if (action->isApplicable(mSelection, mMessages))
            actions += action.get();
    return actions;
}
