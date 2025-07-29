// https://github.com/electronicarts/EASTL/blob/master/doc/SampleNewAndDelete.cpp

/////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include <cassert> // for assert
#include <corecrt_malloc.h> // for _aligned_malloc
#include <cstddef> // for size_t
#include <cstdlib> // for malloc, free

///////////////////////////////////////////////////////////////////////////////
// throw specification wrappers, which allow for portability.
///////////////////////////////////////////////////////////////////////////////

#if defined(EA_COMPILER_NO_EXCEPTIONS) && (!defined(__MWERKS__) || defined(_MSL_NO_THROW_SPECS)) && !defined(EA_COMPILER_RVCT)
#define THROW_SPEC_0 // Throw 0 arguments
#define THROW_SPEC_1(x) // Throw 1 argument
#else
#define THROW_SPEC_0 throw()
#define THROW_SPEC_1(x) throw(x)
#endif

///////////////////////////////////////////////////////////////////////////////
// operator new used by EASTL
///////////////////////////////////////////////////////////////////////////////

void* operator new[](size_t size, const char* /*name*/, int /*flags*/,
    unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return malloc(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* /*name*/,
    int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    assert(alignmentOffset == 0);
    (void)alignmentOffset;

    // Substitute your aligned malloc.
    return _aligned_malloc(size, alignment);
}

///////////////////////////////////////////////////////////////////////////////
// Other operator new as typically required by applications.
///////////////////////////////////////////////////////////////////////////////

void* operator new(size_t size)
{
    return malloc(size);
}

void* operator new[](size_t size)
{
    return malloc(size);
}

///////////////////////////////////////////////////////////////////////////////
// Operator delete, which is shared between operator new implementations.
///////////////////////////////////////////////////////////////////////////////

void operator delete(void* p) noexcept
{
    if (p) // The standard specifies that 'delete NULL' is a valid operation.
        free(p);
}

void operator delete[](void* p) noexcept
{
    if (p)
        free(p);
}
