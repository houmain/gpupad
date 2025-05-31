#include "FileDialog.h"
#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMainWindow>
#include <QMap>
#include <QProcess>
#include <QStandardPaths>

namespace {
    const auto UntitledTag = QStringLiteral("/UT/");
    const auto SessionFileExtension = QStringLiteral("gpjs");
    const auto ShaderFileExtensions = { "glsl", "vs", "fs", "gs", "vert",
        "tesc", "tese", "geom", "frag", "comp", "task", "mesh", "rgen", "rint",
        "rahit", "rchit", "rmiss", "rcall", "ps", "hlsl", "hlsli", "fx", "h" };
    const auto ScriptFileExtensions = { "js", "json", "qml", "lua" };
    const auto TextureFileExtensions = { "ktx", "dds", "png", "exr", "tga",
        "bmp", "jpeg", "jpg", "pbm", "pgm", "tif", "tiff", "raw" };
    const auto VideoFileExtensions = std::initializer_list<const char *>
    {
#if defined(QtMultimedia_FOUND)
        "mp4", "webm", "mkv", "ogg", "mpg", "wmv", "mov", "avi"
#endif
    };

    int gNextUntitledFileIndex;

    QString getDefaultSourceTypeExtension(SourceType sourceType)
    {
        switch (sourceType) {
        case SourceType::PlainText:
        case SourceType::Generic:                    break;
        case SourceType::GLSL_VertexShader:          return "vert";
        case SourceType::GLSL_FragmentShader:        return "frag";
        case SourceType::GLSL_GeometryShader:        return "geom";
        case SourceType::GLSL_TessControlShader:     return "tesc";
        case SourceType::GLSL_TessEvaluationShader:  return "tese";
        case SourceType::GLSL_ComputeShader:         return "comp";
        case SourceType::GLSL_TaskShader:            return "task";
        case SourceType::GLSL_MeshShader:            return "mesh";
        case SourceType::GLSL_RayGenerationShader:   return "rgen";
        case SourceType::GLSL_RayIntersectionShader: return "rint";
        case SourceType::GLSL_RayAnyHitShader:       return "rahit";
        case SourceType::GLSL_RayClosestHitShader:   return "rchit";
        case SourceType::GLSL_RayMissShader:         return "rmiss";
        case SourceType::GLSL_RayCallableShader:     return "rcall";
        case SourceType::HLSL_VertexShader:
        case SourceType::HLSL_PixelShader:
        case SourceType::HLSL_GeometryShader:
        case SourceType::HLSL_HullShader:
        case SourceType::HLSL_DomainShader:
        case SourceType::HLSL_ComputeShader:
        case SourceType::HLSL_AmplificationShader:
        case SourceType::HLSL_MeshShader:
        case SourceType::HLSL_RayGenerationShader:
        case SourceType::HLSL_RayIntersectionShader:
        case SourceType::HLSL_RayAnyHitShader:
        case SourceType::HLSL_RayClosestHitShader:
        case SourceType::HLSL_RayMissShader:
        case SourceType::HLSL_RayCallableShader:     return "hlsl";
        case SourceType::JavaScript:                 return "js";
        }
        return "txt";
    }
} // namespace

const QString SamplesDir = QStringLiteral("samples");
const QString ActionsDir = QStringLiteral("actions");

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
        return "[*]" + QFileInfo(fileName).fileName() + " - "
            + QDir::toNativeSeparators(QFileInfo(fileName).path());

    return "[*]" + getFileTitle(fileName);
}

QString FileDialog::getWindowTitle(const QString &fileName)
{
    return "[*]" + getFileTitle(fileName);
}

QString FileDialog::getFileExtension(const QString &fileName)
{
    if (fileName.isEmpty())
        return "";
    if (isUntitled(fileName))
        return getFileExtension(getFileTitle(fileName));
    return QFileInfo(fileName).suffix().toLower();
}

bool FileDialog::isSessionFileName(const QString &fileName)
{
    return (getFileExtension(fileName) == SessionFileExtension);
}

bool FileDialog::isShaderFileName(const QString &fileName)
{
    const auto extension = getFileExtension(fileName);
    for (const auto &ext : ShaderFileExtensions)
        if (ext == extension)
            return true;
    return false;
}

bool FileDialog::isScriptFileName(const QString &fileName)
{
    const auto extension = getFileExtension(fileName);
    for (const auto &ext : ScriptFileExtensions)
        if (ext == extension)
            return true;
    return false;
}

bool FileDialog::isTextureFileName(const QString &fileName)
{
    const auto extension = getFileExtension(fileName);
    for (const auto &ext : TextureFileExtensions)
        if (ext == extension)
            return true;
    return false;
}

bool FileDialog::isVideoFileName(const QString &fileName)
{
    const auto extension = getFileExtension(fileName);
    for (const auto &ext : VideoFileExtensions)
        if (ext == extension)
            return true;
    return false;
}

FileDialog::FileDialog(QMainWindow *window) : mWindow(window) { }

FileDialog::~FileDialog() = default;

void FileDialog::setDirectory(QDir directory)
{
    if (mDirectory != directory) {
        mDirectory = directory;
        Q_EMIT directoryChanged(directory);
    }
}

QString FileDialog::fileName() const
{
    if (mFileNames.isEmpty())
        return {};
    return mFileNames.first();
}

bool FileDialog::exec(Options options, QString currentFileName,
    SourceType sourceType)
{
    QFileDialog dialog(mWindow);
    dialog.setOption(QFileDialog::HideNameFilterDetails);

    if (options & Directory) {
        dialog.setWindowTitle(tr("Select Directory"));
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(QFileDialog::Directory);
    } else if (options & Saving) {
        if (currentFileName.isEmpty())
            dialog.setWindowTitle(tr("New File"));
        else
            dialog.setWindowTitle(
                tr("Save '%1' As").arg(getFileTitle(currentFileName)));

        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
    } else {
        const auto importing = ((options & Importing) != 0);
        const auto multiselect = ((options & Multiselect) != 0);
        dialog.setWindowTitle(importing ? tr("Import File") : tr("Open File"));
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(multiselect ? QFileDialog::ExistingFiles
                                       : QFileDialog::ExistingFile);
    }

    auto shaderFileFilter = QString();
    for (const auto &ext : ShaderFileExtensions)
        shaderFileFilter = shaderFileFilter + " *." + ext;

    auto textureFileFilter = QString();
    for (auto format : TextureFileExtensions)
        textureFileFilter = textureFileFilter + " *." + QString(format);

    auto videoFileFilter = QString();
    if (VideoFileExtensions.size())
        for (auto format : VideoFileExtensions)
            videoFileFilter = videoFileFilter + " *." + QString(format);

    auto scriptFileFilter = QString();
    for (const auto &ext : ScriptFileExtensions)
        scriptFileFilter = scriptFileFilter + " *." + ext;

    auto filters = QStringList();
    if ((options & AllExtensionFilters) == AllExtensionFilters)
        filters.append(tr("All Files (*)"));

    if (options & SessionExtensions)
        filters.append(qApp->applicationName() + tr(" session") + " (*."
            + SessionFileExtension + ")");
    if (options & ShaderExtensions)
        filters.append(tr("Shader files") + " (" + shaderFileFilter + ")");
    if (options & TextureExtensions)
        filters.append(tr("Texture files") + " (" + textureFileFilter + ")");
    if ((options & TextureExtensions) && !videoFileFilter.isEmpty())
        filters.append(tr("Video files") + " (" + videoFileFilter + ")");
    if (options & ScriptExtensions)
        filters.append(tr("Script files") + " (" + scriptFileFilter + ")");
    const auto binaryFileFilter = QString(tr("Binary files") + " (*)");
    if (options & BinaryExtensions)
        filters.append(binaryFileFilter);

    if ((options & AllExtensionFilters) != AllExtensionFilters)
        filters.append(tr("All Files (*)"));

    dialog.setNameFilters(filters);

    dialog.setDefaultSuffix("");
    if (options & Saving) {
        if (options & (ShaderExtensions | ScriptExtensions))
            dialog.setDefaultSuffix(getDefaultSourceTypeExtension(sourceType));
        else if (options & SessionExtensions)
            dialog.setDefaultSuffix(SessionFileExtension);
        else if (options & BinaryExtensions)
            dialog.setDefaultSuffix("bin");
        else if (options & TextureExtensions)
            dialog.setDefaultSuffix(options & SavingNon2DTexture ? "ktx"
                                                                 : "png");
    }

    if (isUntitled(currentFileName)) {
        currentFileName = getFileTitle(currentFileName);
        if (QFileInfo(currentFileName).suffix().isEmpty()
            && !dialog.defaultSuffix().isEmpty())
            currentFileName += "." + dialog.defaultSuffix();
    }

    if (QFileInfo(currentFileName).isAbsolute())
        dialog.setDirectory(QFileInfo(currentFileName).dir());
    else if (mDirectory.exists())
        dialog.setDirectory(mDirectory);
    dialog.selectFile(currentFileName);

    const auto extension = getFileExtension(currentFileName);
    for (const auto &filter : filters)
        if (filter.contains("*." + extension)) {
            dialog.selectNameFilter(filter);
            break;
        }

    if (dialog.exec() != QDialog::Accepted)
        return false;

    mFileNames = dialog.selectedFiles();
    for (auto &fileName : mFileNames)
        fileName = toNativeCanonicalFilePath(fileName);
    mAsBinaryFile = (dialog.selectedNameFilter() == binaryFileFilter);
    setDirectory(dialog.directory());
    return true;
}

bool isNativeCanonicalFilePath(const QString &fileName)
{
    return (toNativeCanonicalFilePath(fileName) == fileName);
}

QString toNativeCanonicalFilePath(const QString &fileName)
{
    if (FileDialog::isEmptyOrUntitled(fileName))
        return fileName;
    auto fileInfo = QFileInfo(fileName);
    Q_ASSERT(fileInfo.isAbsolute());
    return QDir::toNativeSeparators(
        fileInfo.exists() ? fileInfo.canonicalFilePath() : fileName);
}

QString getFirstDirEntry(const QString &path)
{
    if (QFileInfo(path).isDir()) {
        auto dir = QDir(path);
        dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
        auto entries = dir.entryInfoList();
        if (!entries.isEmpty())
            return entries.first().filePath();
    }
    return path;
}

void showInFileManager(const QString &path)
{
#if defined(_WIN32)
    QProcess::startDetached("explorer.exe",
        { "/select,", QDir::toNativeSeparators(getFirstDirEntry(path)) });
#elif defined(__APPLE__)
    QProcess::execute("/usr/bin/osascript",
        { "-e",
            "tell application \"Finder\" to reveal POSIX file \"" + path
                + "\"" });
    QProcess::execute("/usr/bin/osascript",
        { "-e", "tell application \"Finder\" to activate" });
#else
    const auto info = QFileInfo(path);
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(info.isDir() ? path : info.path()));
#endif
}

int showNotSavedDialog(QWidget *parent, const QString &fileName)
{
    auto dialog = QMessageBox(parent);
    dialog.setIcon(QMessageBox::Question);
    dialog.setText(parent
                       ->tr("<h3>The file '%1' is not saved.</h3>"
                            "Do you want to save it before closing?<br>")
                       .arg(FileDialog::getFileTitle(fileName)));
    dialog.addButton(QMessageBox::Save);
    dialog.addButton(parent->tr("&Don't Save"), QMessageBox::RejectRole);
    dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(QMessageBox::Save);
    return dialog.exec();
}

bool showSavingFailedMessage(QWidget *parent, const QString &fileName)
{
    auto dialog = QMessageBox(parent);
    dialog.setIcon(QMessageBox::Critical);
    auto text = parent->tr("<h3>Saving file '%1' failed.</h3>")
                    .arg(FileDialog::getFileTitle(fileName));
    text += parent->tr("Is the path writeable");
    if (FileDialog::isTextureFileName(fileName))
        text +=
            parent->tr(" and does the file format support the texture format");
    text += "?<br>";
    dialog.setText(text);
    dialog.addButton(QMessageBox::Retry);
    dialog.addButton(QMessageBox::Cancel);
    return (dialog.exec() == QMessageBox::Retry);
}

void showCopyingSessionFailedMessage(QWidget *parent)
{
    auto dialog = QMessageBox(parent);
    dialog.setIcon(QMessageBox::Warning);
    dialog.setText(parent->tr("<h3>Copying session files failed.</h3>"
                              "Not all files in session could be copied.<br>"));
    dialog.addButton(QMessageBox::Ok);
    dialog.exec();
}

QDir getInstallDirectory(const QString &dirName)
{
    const auto binDir = QDir(QCoreApplication::applicationDirPath());
    const auto installDir = (binDir.dirName() == "bin"
            ? QDir::cleanPath(binDir.filePath(".."))
            : binDir);
    const auto searchPaths = std::initializer_list<QDir>{
#if !defined(NDEBUG)
        installDir.filePath("../../extra"),
        installDir.filePath("../extra"),
        installDir.filePath("extra"),
        installDir.filePath(".."),
        installDir.filePath("../.."),
#endif
#if defined(_WIN32)
        installDir.path(),
#else
        installDir.filePath("share/" + QCoreApplication::organizationName()),
        QDir(qEnvironmentVariable("APPDIR") + "/usr/share/"
            + QCoreApplication::organizationName()),
#endif
        binDir.path(),
    };
    for (const auto &path : searchPaths)
        if (path.exists(dirName))
            return QDir::cleanPath(path.filePath(dirName));

    return QDir();
}

QDir getUserDirectory(const QString &dirName)
{
    auto config =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    auto dir = QDir::cleanPath(config + "/../" + dirName);
    QDir().mkpath(dir);
    return dir;
}

QList<QDir> getApplicationDirectories(const QString &dirName)
{
    auto result = QList<QDir>();
    const auto dirs = {
        getInstallDirectory(dirName),
        getUserDirectory(dirName),
    };
    for (const auto &dir : dirs)
        if (dir != QDir())
            result.append(dir);
    return result;
}

QFileInfoList enumerateApplicationPaths(const QString &dirName,
    QDir::Filters filters)
{
    auto entries = QFileInfoList();
    for (auto &dir : getApplicationDirectories(dirName)) {
        dir.setFilter(filters | QDir::NoDotAndDotDot);
        entries += dir.entryInfoList();
    }
    return entries;
}
