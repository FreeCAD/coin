#ifdef ABI_BREAKING_OPTIMIZE
#include <Inventor/SbByteBuffer.h>

SbByteBuffer SbByteBuffer::invalidBuffer_;
#else
#define PIMPL_IMPLEMENTATION
#include <Inventor/SbByteBufferP.icc>

SbByteBuffer SbByteBufferP::invalidBuffer_;
#endif
