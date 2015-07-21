

#ifdef DAO_API
#  undef DAO_API
#endif

#define DAO_API(Linkage,Type,Name,Signature)  Type (*_##Name) Signature = NULL


#ifdef DAO_HAS_STREAM
#include"../stream/dao_stream.h"
#endif


#ifdef DAO_HAS_IMAGE
#include"../image/source/dao_image.h"
#endif
