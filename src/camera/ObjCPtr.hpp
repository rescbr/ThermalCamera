#ifndef OBJC_PTR_HPP
#define OBJC_PTR_HPP

#include <memory>

struct ObjCDeleter
{
    void operator()(void* ptr) const;
};

template<typename T>
using ObjCPtr = std::unique_ptr<T, ObjCDeleter>;

#endif
