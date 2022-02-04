#include "FileDialog.h"
#include <QFileDialog>
#include <QMainWindow>
#include <QMap>
#include <QProcess>
#include <QApplication>
#include <QDesktopServices>

namespace {
    const auto UntitledTag = QStringLiteral("/UT/");
    const auto SessionFileExtension = QStringLiteral("gpjs");
    const auto ShaderFileExtensions = { "glsl", "vs", "fs", "gs",
        "vert", "tesc", "tese", "geom", "frag", "comp", 
        "ps", "hlsl", "hlsli", "fx" };
    const auto ScriptFileExtensions = { "js", "qml" };
    const auto TextureFileExtensions = { "ktx", "dds", "png", "exr", 
        "tga", "bmp", "jpeg", "jpg", "pbm", "pgm", "tif", "tiff", "raw" };
    const auto VideoFileExtensions = std::initializer_list<const char*>{
#if defined(QtMultimedia_FOUND)
    "mp4", "webm", "mkv", "ogg", "mpg", "wmv", "mov", "avi"
#endif
    };

    int gNextUntitledFileIndex;
} // namespace

QString FileDialog::generateNextUntitledFileName(QString base)
{
    const auto index = gNextUntitledFileIndex++;
    return UntitledTag + base + QStringLiteral("/%1").arg(index + 1);
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

    auto name = QString(fileName).remove(0, UntitledTag.size());
    return name.left(name.lastIndexOf('/'));
}

QString FileDialog::getFullWindowTitle(const QString &fileName)
{
    if (!fileName.startsWith(UntitledTag))
        return "[*]" + QFileInfo(fileName).fileName() + " - " + 
            QDir::toNativeSeparators(QFileInfo(fileName).path());

    return "[*]" + getFileTitle(fileName);
}

QString FileDialog::getWindowTitle(const QString &fileName)
{
    return "[*]" + getFileTitle(fileName);
}

QString FileDialog::advanceSaveAsSuffix(const QString &fileName)
{
    if (isEmptyOrUntitled(fileName))
        return fileName;

    const auto dot = fileName.lastIndexOf('.');
    if (dot < 0)
        return fileName;

    const auto base = fileName.mid(0, dot);
    if (!base.endsWith(')'))
        return base + " (1)" + fileName.mid(dot);

    const auto suffix = fileName.lastIndexOf('(');
    if (suffix >= 0) {
        auto ok = false;
        const auto number = fileName.mid(suffix + 1, dot - suffix - 2).toInt(&ok);
        if (ok)
            return QString("%1(%2)%3")
                .arg(fileName.mid(0, suffix))
                .arg(number + 1)
                .arg(fileName.mid(dot));
    }
    return fileName;
}

bool FileDialog::isSessionFileName(const QString &fileName)
{
    return fileName.endsWith(SessionFileExtension);
}

bool FileDialog::isVideoFileName(const QString &fileName)
{
    const auto lowerFileName = fileName.toLower();
    for (auto extension : VideoFileExtensions)
        if (lowerFileName.endsWith(extension))
            return true;
    return false;
}

void FileDialog::showInFileManager(const QString &path)
{
#if defined(_WIN32)
    QProcess::startDetached("explorer.exe", { "/select,", QDir::toNativeSeparators(path) });
#elif defined(__APPLE__)
    QProcess::execute("/usr/bin/osascript", { "-e", "tell application \"Finder\" to reveal POSIX file \"" + path + "\"" });
    QProcess::execute("/usr/bin/osascript", { "-e", "tell application \"Finder\" to activate" });
#else
    const auto info = QFileInfo(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? path : info.path()));
#endif
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
    for (const QByteArray &format : TextureFileExtensions)
        textureFileFilter = textureFileFilter + " *." + QString(format);

    auto videoFileFilter = QString();
    if (VideoFileExtensions.size())
        for (const QByteArray &format : VideoFileExtensions)
            videoFileFilter = videoFileFilter + " *." + QString(format);

    auto scriptFileFilter = QString();
    for (const auto &ext : ScriptFileExtensions)
        scriptFileFilter = scriptFileFilter + " *." + ext;

    auto filters = QStringList();
    if ((options & AllExtensionFilters) == AllExtensionFilters)
        filters.append(tr("All Files (*)"));

    if (options & SessionExtensions)
        filters.append(qApp->applicationName() + tr(" session") +
            " (*." + SessionFileExtension + ")");
    if (options & ShaderExtensions)
        filters.append(tr("Shader files") + " (" + shaderFileFilter + ")");
    if (options & TextureExtensions)
        filters.append(tr("Texture files") + " (" + textureFileFilter + ")");
    if ((options & TextureExtensions) && !videoFileFilter.isEmpty())
        filters.append(tr("Video files") + " (" + videoFileFilter + ")");
    if (options & ScriptExtensions)
        filters.append(tr("JavaScript files") + " (" + scriptFileFilter + ")");
    const auto binaryFileFilter = QString(tr("Binary files") + " (*)");
    if (options & BinaryExtensions)
        filters.append(binaryFileFilter);

    if ((options & AllExtensionFilters) != AllExtensionFilters)
        filters.append(tr("All Files (*)"));

    dialog.setNameFilters(filters);

    dialog.setDefaultSuffix("");
    if (options & Saving) {
        if (options & ShaderExtensions)
            dialog.setDefaultSuffix(*begin(ShaderFileExtensions));
        else if (options & SessionExtensions)
            dialog.setDefaultSuffix(SessionFileExtension);
        else if (options & ScriptExtensions)
            dialog.setDefaultSuffix(*begin(ScriptFileExtensions));
        else if (options & BinaryExtensions)
            dialog.setDefaultSuffix("bin");
        else if (options & TextureExtensions)
            dialog.setDefaultSuffix(options & SavingNon2DTexture ? "ktx" : "png");
    }

    if (isUntitled(currentFileName)) {
        currentFileName = getFileTitle(currentFileName);
        if (QFileInfo(currentFileName).suffix().isEmpty() &&
            !dialog.defaultSuffix().isEmpty())
            currentFileName += "." + dialog.defaultSuffix();
    }

    dialog.selectFile(currentFileName);
    if (currentFileName.isEmpty())
        dialog.setDirectory(mDirectory.exists() ?
            mDirectory : QDir::current());

    if (dialog.exec() != QDialog::Accepted)
        return false;

    mFileNames = dialog.selectedFiles();
    for (auto &fileName : mFileNames)
        fileName = QDir::toNativeSeparators(fileName);
    mDirectory = dialog.directory();
    mAsBinaryFile = (dialog.selectedNameFilter() == binaryFileFilter);
    return true;
}

int openNotSavedDialog(QWidget *parent, const QString& fileName)
{
    QMessageBox dialog(parent);
    dialog.setIcon(QMessageBox::Question);
    dialog.setText(parent->tr("<h3>The file '%1' is not saved.</h3>"
           "Do you want to save it before closing?<br>")
           .arg(FileDialog::getFileTitle(fileName)));
    dialog.addButton(QMessageBox::Save);
    dialog.addButton(parent->tr("&Don't Save"), QMessageBox::RejectRole);
    dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(QMessageBox::Save);
    return dialog.exec();
}
