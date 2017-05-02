#ifndef GLOBJECT_H
#define GLOBJECT_H

#include <QOpenGLContext>

class GLObject
{
public:
    using Free = void(*)(GLuint object);

    GLObject() = default;

    GLObject(GLuint object, Free free) noexcept
        : mObject(object), mFree(free) { }

    GLObject(GLObject &&rhs) noexcept
    {
        std::swap(mObject, rhs.mObject);
        std::swap(mFree, rhs.mFree);
    }

    GLObject& operator=(GLObject &&rhs) noexcept
    {
        auto tmp = std::move(rhs);
        std::swap(mObject, tmp.mObject);
        std::swap(mFree, tmp.mFree);
        return *this;
    }

    ~GLObject()
    {
        reset();
    }

    explicit operator bool() const { return (mObject != GL_NONE); }
    operator GLuint() const { return mObject; }

    void reset()
    {
        if (mFree) {
            mFree(mObject);
            mObject = { };
            mFree = nullptr;
        }
    }

private:
    GLuint mObject{ };
    Free mFree{ };
};

#endif // GLOBJECT_H
