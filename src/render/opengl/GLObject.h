#pragma once

#include <utility>

class GLContext;

class GLObject
{
public:
    using GLuint = unsigned int;
    using Free = void (*)(GLContext &context, GLuint object);

    GLObject() = default;

    GLObject(GLContext *context, GLuint object, Free free) noexcept
        : mContext(context)
        , mObject(object)
        , mFree(free)
    {
    }

    GLObject(GLObject &&rhs) noexcept
    {
        std::swap(mContext, rhs.mContext);
        std::swap(mObject, rhs.mObject);
        std::swap(mFree, rhs.mFree);
    }

    GLObject &operator=(GLObject &&rhs) noexcept
    {
        auto tmp = std::move(rhs);
        std::swap(mContext, tmp.mContext);
        std::swap(mObject, tmp.mObject);
        std::swap(mFree, tmp.mFree);
        return *this;
    }

    ~GLObject() { reset(); }

    explicit operator bool() const { return (mObject != 0); }
    operator GLuint() const { return mObject; }

    void reset()
    {
        if (mFree) {
            mFree(*mContext, mObject);
            mObject = {};
            mFree = nullptr;
        }
    }

private:
    GLContext *mContext{};
    GLuint mObject{};
    Free mFree{};
};
