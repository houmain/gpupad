#include "FileDialog.h"
#include <QFileDialog>
#include <QImageReader>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QTemporaryFile>

namespace {
    const auto UntitledTag = QStringLiteral("/UT/");
    const auto SessionFileExtension = QStringLiteral("gpjs");
    const auto ShaderFileExtensions = { "glsl", "vs", "fs", "gs",
        "vert", "tesc", "tese", "geom", "frag", "comp" };
    const auto ScriptFileExtensions = { "js" };

    static QMap<QString, int> gNextUntitledFileIndex;
} // namespace

void FileDialog::resetNextUntitledFileIndex()
{
    gNextUntitledFileIndex.clear();
}

QString FileDialog::generateNextUntitledFileName(QString base)
{
    auto fileName = UntitledTag + base;
    auto index = gNextUntitledFileIndex[base]++;
    if (index)
        fileName += QStringLiteral(" [%1]").arg(index + 1);
    return fileName;
}

bool FileDialog::isUntitled(const QString &fileName)
{
    return fileName.startsWith(UntitledTag);
}

bool FileDialog::isEmptyOrUntitled(const QString &fileName)
{
    return fileName.isEmpty() || isUntitled(fileName);
}

QString FileDialog::getFileTitle(const QString &fileName)
{
    if (!fileName.startsWith(UntitledTag))
        return QFileInfo(fileName).fileName();

    return QString(fileName).remove(0, UntitledTag.size());
}

QString FileDialog::getFullWindowTitle(const QString &fileName)
{
    if (!fileName.startsWith(UntitledTag))
        return "[*]" + QFileInfo(fileName).fileName() + " - " + QFileInfo(fileName).path();

    return "[*]" + QString(fileName).remove(0, UntitledTag.size());
}

QString FileDialog::getWindowTitle(const QString &fileName)
{
    return "[*]" + getFileTitle(fileName);
}

bool FileDialog::isSessionFileName(const QString &fileName)
{
    return fileName.endsWith(SessionFileExtension);
}

FileDialog::FileDialog(QMainWindow *window)
    : mWindow(window)
{
}

FileDialog::~FileDialog() = default;

void FileDialog::setDirectory(QDir directory)
{
    mDirectory = directory;
}

QString FileDialog::fileName() const
{
    if (mFileNames.isEmpty())
        return { };
    return mFileNames.first();
}

bool FileDialog::exec(Options options, QString currentFileName)
{
    QFileDialog dialog(mWindow);
    dialog.setOption(QFileDialog::HideNameFilterDetails);

    if (options & Saving) {
        if (currentFileName.isEmpty())
            dialog.setWindowTitle(tr("New File"));
        else
            dialog.setWindowTitle(tr("Save '%1' As")
                .arg(getFileTitle(currentFileName)));

        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
    }
    else {
        const auto importing = ((options & Importing) != 0);
        const auto multiselect = ((options & Multiselect) != 0);
        dialog.setWindowTitle(importing ? tr("Import File") : tr("Open File"));
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(multiselect ?
            QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
    }

    auto shaderFileFilter = QString();
    for (const auto &ext : ShaderFileExtensions)
        shaderFileFilter = shaderFileFilter + " *." + ext;

    auto textureFileFilter = QString();
    textureFileFilter += " *.ktx";
    foreach (const QByteArray &format, QImageReader::supportedImageFormats())
        textureFileFilter = textureFileFilter + " *." + QString(format);

    auto scriptFileFilter = QString();
    for (const auto &ext : ScriptFileExtensions)
        scriptFileFilter = scriptFileFilter + " *." + ext;

    auto supportedFileFilter = "*." + SessionFileExtension +
        shaderFileFilter + scriptFileFilter + textureFileFilter;

    auto filters = QStringList();
    const auto binaryFileFilter = tr("Binary files") + " (*)";
    if (options & SupportedExtensions)
        filters.append(tr("Supported files") + " (" + supportedFileFilter + ")");
    if (options & SessionExtensions)
        filters.append(qApp->applicationName() + tr(" session") +
            " (*." + SessionFileExtension + ")");
    if (options & ShaderExtensions)
        filters.append(tr("GLSL shader files") + " (" + shaderFileFilter + ")");
    if (options & TextureExtensions)
        filters.append(tr("Texture files") + " (" + textureFileFilter + ")");
    if (options & BinaryExtensions)
        filters.append(binaryFileFilter);
    if (options & ScriptExtensions)
        filters.append(tr("JavaScript files") + " (" + scriptFileFilter + ")");

    filters.append(tr("All Files (*)"));
    dialog.setNameFilters(filters);

    dialog.setDefaultSuffix("");
    if (options & ShaderExtensions)
        dialog.setDefaultSuffix(*begin(ShaderFileExtensions));
    else if (options & SessionExtensions)
        dialog.setDefaultSuffix(SessionFileExtension);
    else if (options & ScriptExtensions)
        dialog.setDefaultSuffix(*begin(ScriptFileExtensions));
    else if (options & TextureExtensions)
        dialog.setDefaultSuffix("png");

    if (isUntitled(currentFileName))
        currentFileName = "";

    dialog.selectFile(currentFileName);
    if (currentFileName.isEmpty())
        dialog.setDirectory(mDirectory.exists() ?
            mDirectory : QDir::current());

    if (dialog.exec() != QDialog::Accepted)
        return false;

    if (options & Saving)
        for (const auto &fileName : dialog.selectedFiles())
            if (!QFileInfo(fileName).isWritable() && !QTemporaryFile(fileName).open()) {
                QMessageBox dialog(mWindow);
                dialog.setIcon(QMessageBox::Warning);
                dialog.setWindowTitle(tr("File Error"));
                dialog.setText(tr("Saving failed. The path is not writeable."));
                dialog.addButton(QMessageBox::Ok);
                dialog.exec();
                return false;
            }

    mFileNames = dialog.selectedFiles();
    mDirectory = dialog.directory();
    mAsBinaryFile = (dialog.selectedNameFilter() == binaryFileFilter);
    return true;
}
