#pragma once
#include <tbx/move_only.h>
#include <exception>

namespace Core {

template <typename T>
class Singleton {
public:
    NO_COPY(Singleton);
    NO_MOVE(Singleton);

    inline static T& getSingleton()
    {
        return *s_instance;
    }
    inline static T* getSingletonPtr()
    {
        return s_instance;
    }

protected:
    inline Singleton(T* instance)
    {
        if (s_instance != nullptr)
            throw std::exception {};

        s_instance = instance;
    }

private:
    inline static T* s_instance { nullptr };
};

}
