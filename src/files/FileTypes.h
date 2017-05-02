#ifndef TYPES_H
#define TYPES_H

#include "EditActions.h"
#include <QSharedPointer>

class SourceEditorSettings;
class SourceFile;
using SourceFilePtr = QSharedPointer<SourceFile>;
class ImageFile;
using ImageFilePtr = QSharedPointer<ImageFile>;
class BinaryFile;
using BinaryFilePtr = QSharedPointer<BinaryFile>;
class SourceEditor;
class BinaryEditor;
class ImageEditor;
class SessionEditor;

#endif // TYPES_H
