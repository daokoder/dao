
#ifdef DAO_API
#  undef DAO_API
#endif

#define DAO_API(Linkage,Type,Name,Signature)  Linkage Type (*_##Name) Signature; Type (*_##Name) Signature = NULL


#ifdef DAO_HAS_STREAM
#define DAO_STREAM
#include"../stream/dao_stream.h"
#endif


#ifdef DAO_HAS_IMAGE
#define DAO_IMAGE
#include"../image/source/dao_image.h"
#endif


#ifdef DAO_HAS_TIME
#define DAO_TIME
#include"../time/dao_time.h"
#endif
