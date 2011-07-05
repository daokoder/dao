
#include <llvm/ADT/StringExtras.h>
#include <clang/AST/Type.h>
#include <clang/AST/TypeLoc.h>
#include <clang/AST/Expr.h>
#include <clang/Lex/Preprocessor.h>

#include "cdaoVariable.hpp"
#include "cdaoModule.hpp"

const string daopar_int = "$(name) :int$(default)";
const string daopar_float = "$(name) :float$(default)";
const string daopar_double = "$(name) :double$(default)";
const string daopar_complex = "$(name) :complex$(default)";
const string daopar_string = "$(name) :string$(default)";
const string daopar_ints = "$(name) :array<int>";
const string daopar_floats = "$(name) :array<float>";
const string daopar_doubles = "$(name) :array<double>";
const string daopar_complexs = "$(name) :array<complex>$(default)";
const string daopar_buffer = "$(name) :cdata$(default)";
const string daopar_stream = "$(name) :stream$(default)";
const string daopar_user = "$(name) :$(daotype)$(default)";
const string daopar_userdata = "$(name) :any$(default)"; // for callback data
const string daopar_callback = "$(name) :any"; // for callback, no precise type yet! XXX

const string dao2cxx = "  $(cxxtype) $(name)= ($(cxxtype)) ";
const string dao2cxx2 = "  $(cxxtype)* $(name)= ($(cxxtype)*) ";
const string dao2cxx3 = "  $(cxxtype)** $(name)= ($(cxxtype)**) ";

const string dao2cxx_char = dao2cxx + "DValue_GetMBString( _p[$(index)] )[0];\n";
const string dao2cxx_wchar = dao2cxx + "DValue_GetWCString( _p[$(index)] )[0];\n";
const string dao2cxx_int  = dao2cxx + "_p[$(index)]->v.i;\n";
const string dao2cxx_float = dao2cxx + "_p[$(index)]->v.f;\n";
const string dao2cxx_double = dao2cxx + "_p[$(index)]->v.d;\n";
const string dao2cxx_complex = dao2cxx + "{_p[$(index)]->v.c->real, _p[$(index)]->v.c->imag};\n";
const string dao2cxx_mbs = dao2cxx2 + "DValue_GetMBString( _p[$(index)] );\n";
const string dao2cxx_wcs = dao2cxx2 + "DValue_GetWCString( _p[$(index)] );\n";
const string dao2cxx_bytes = dao2cxx2 + "DaoArray_ToByte( _p[$(index)]->v.array );\n";
const string dao2cxx_ubytes = dao2cxx2 + "DaoArray_ToUByte( _p[$(index)]->v.array );\n";
const string dao2cxx_shorts = dao2cxx2 + "DaoArray_ToShort( _p[$(index)]->v.array );\n";
const string dao2cxx_ushorts = dao2cxx2 + "DaoArray_ToUShort( _p[$(index)]->v.array );\n";
const string dao2cxx_ints = dao2cxx2 + "DaoArray_ToInt( _p[$(index)]->v.array );\n";
const string dao2cxx_uints = dao2cxx2 + "DaoArray_ToUInt( _p[$(index)]->v.array );\n";
const string dao2cxx_floats = dao2cxx2 + "DaoArray_ToFloat( _p[$(index)]->v.array );\n";
const string dao2cxx_doubles = dao2cxx2 + "DaoArray_ToDouble( _p[$(index)]->v.array );\n";
const string dao2cxx_complexs8 = dao2cxx2 + "(complex8*) DaoArray_ToFloat( _p[$(index)]->v.array );\n";
const string dao2cxx_complexs16 = dao2cxx2 + "(complex16*) DaoArray_ToDouble( _p[$(index)]->v.array );\n";

const string dao2cxx_bmat = dao2cxx3 + "DaoArray_GetMatrixB( _p[$(index)]->v.array, $(size) );\n";
const string dao2cxx_smat = dao2cxx3 + "DaoArray_GetMatrixS( _p[$(index)]->v.array, $(size) );\n";
const string dao2cxx_imat = dao2cxx3 + "DaoArray_GetMatrixI( _p[$(index)]->v.array, $(size) );\n";
const string dao2cxx_fmat = dao2cxx3 + "DaoArray_GetMatrixF( _p[$(index)]->v.array, $(size) );\n";
const string dao2cxx_dmat = dao2cxx3 + "DaoArray_GetMatrixD( _p[$(index)]->v.array, $(size) );\n";
const string dao2cxx_c8mat = dao2cxx3 + "(complex8*) DaoArray_GetMatrixF( _p[$(index)]->v.array, $(size) );\n";
const string dao2cxx_c16mat = dao2cxx3 + "(complex16*) DaoArray_GetMatrixD( _p[$(index)]->v.array, $(size) );\n";

const string dao2cxx_ubmat = dao2cxx_bmat; // TODO:
const string dao2cxx_usmat = dao2cxx_smat;
const string dao2cxx_uimat = dao2cxx_imat;

const string dao2cxx_stream = dao2cxx2 + "DaoStream_GetFile( _p[$(index)]->v.stream );\n";

const string dao2cxx_void = dao2cxx2 + 
"DValue_GetCData( _p[$(index)] );\n";
const string dao2cxx_void2 = "  $(cxxtype) $(name)= ($(cxxtype)) \
DValue_GetCData( _p[$(index)] );\n";

const string dao2cxx_user = dao2cxx2 + 
"DValue_CastCData( _p[$(index)], dao_$(typer)_Typer );\n";
const string dao2cxx_user2 = "  $(cxxtype)* $(name)= ($(cxxtype)*) \
DValue_CastCData( _p[$(index)], dao_$(typer)_Typer );\n";
const string dao2cxx_user3 = "  $(cxxtype)** $(name)= ($(cxxtype)**) \
DValue_GetCData2( _p[$(index)] );\n";
const string dao2cxx_user4 = "  $(cxxtype)* $(name)= ($(cxxtype)*) \
DValue_GetCData2( _p[$(index)] );\n";

const string dao2cxx_callback =
"  DaoMethod *_$(name) = (DaoMethod*) _p[$(index)]->v.p;\n\
  $(cxxtype) $(name) = Dao_$(callback);\n";
const string dao2cxx_userdata =
"  DaoCallbackData *$(name) = DaoCallbackData_New( _$(callback), *_p[$(index)] );\n\
  if( $(name) == NULL ){\n\
    DaoContext_RaiseException( _ctx, DAO_ERROR_PARAM, \"invalid callback\" );\n\
	return;\n\
  }\n";


const string cxx2dao = "  _dp[$(index)] = DValue_New";

const string cxx2dao_int = cxx2dao + "Integer( (int) $(name) );\n";
const string cxx2dao_float = cxx2dao + "Float( (float) $(name) );\n";
const string cxx2dao_double = cxx2dao + "Double( (double) $(name) );\n";
const string cxx2dao_int2 = cxx2dao + "Integer( (int) *$(name) );\n";
const string cxx2dao_float2 = cxx2dao + "Float( (float) *$(name) );\n";
const string cxx2dao_double2 = cxx2dao + "Double( (double) *$(name) );\n";
const string cxx2dao_mbs = cxx2dao+"MBString( (char*) $(name), strlen( (char*)$(name) ) );\n"; // XXX for char**
const string cxx2dao_wcs = cxx2dao + "WCString( (wchar_t*) $(name), wcslen( (wchar_t*)$(name) ) );\n"; // XXX for wchar_t**
const string cxx2dao_bytes = cxx2dao + "VectorB( (char*) $(name), $(size) );\n";
const string cxx2dao_ubytes = cxx2dao + "VectorUB( (unsigned char*) $(name), $(size) );\n";
const string cxx2dao_shorts = cxx2dao + "VectorS( (short*) $(name), $(size) );\n";
const string cxx2dao_ushorts = cxx2dao + "VectorUS( (unsigned short*) $(name), $(size) );\n";
const string cxx2dao_ints = cxx2dao + "VectorI( (int*) $(name), $(size) );\n";
const string cxx2dao_uints = cxx2dao + "VectorUI( (unsigned int*) $(name), $(size) );\n";
const string cxx2dao_floats = cxx2dao + "VectorF( (float*) $(name), $(size) );\n";
const string cxx2dao_doubles = cxx2dao + "VectorD( (double*) $(name), $(size) );\n";

const string cxx2dao_bmat = cxx2dao + "MatrixB( (char**) $(name), $(size), $(size2) );\n";
const string cxx2dao_ubmat = cxx2dao + "MatrixUB( (unsigned char**) $(name), $(size), $(size2) );\n";
const string cxx2dao_smat = cxx2dao + "MatrixS( (short**) $(name), $(size), $(size2) );\n";
const string cxx2dao_usmat = cxx2dao + "MatrixUS( (unsigned short**) $(name), $(size), $(size2) );\n";
const string cxx2dao_imat = cxx2dao + "MatrixI( (int**) $(name), $(size), $(size2) );\n";
const string cxx2dao_uimat = cxx2dao + "MatrixUI( (unsigned int**) $(name), $(size), $(size2) );\n";
const string cxx2dao_fmat = cxx2dao + "MatrixF( (float**) $(name), $(size), $(size2) );\n";
const string cxx2dao_dmat = cxx2dao + "MatrixD( (double**) $(name), $(size), $(size2) );\n";

const string cxx2dao_stream = cxx2dao + "Stream( (FILE*) $(refer) );\n";
const string cxx2dao_voidp = "  _dp[$(index)] = DValue_WrapCData( NULL, (void*) $(refer) );\n";
const string cxx2dao_user = "  _dp[$(index)] = DValue_WrapCData( dao_$(typer)_Typer, (void*) $(refer) );\n";

const string cxx2dao_userdata = "  DValue_Copy( _dp2[$(index)], $(name) );\n";

const string cxx2dao_qchar = cxx2dao+"Integer( $(name).digitValue() );\n";
const string cxx2dao_qchar2 = cxx2dao+"Integer( $(name)->digitValue() );\n";
const string cxx2dao_qbytearray = cxx2dao+"MBString( (char*) $(name).data(), 0 );\n";
const string cxx2dao_qstring = cxx2dao+"MBString( (char*) $(name).toLocal8Bit().data(), 0 );\n";

const string ctxput = "  DaoContext_Put";

const string ctxput_int = ctxput + "Integer( _ctx, (int) $(name) );\n";
const string ctxput_float = ctxput + "Float( _ctx, (float) $(name) );\n";
const string ctxput_double = ctxput + "Double( _ctx, (double) $(name) );\n";
const string ctxput_mbs = ctxput + "MBString( _ctx, (char*) $(name) );\n";
const string ctxput_wcs = ctxput + "WCString( _ctx, (wchar_t*) $(name) );\n";
const string ctxput_bytes = ctxput + "Bytes( _ctx, (char*) $(name), $(size) );\n"; // XXX array?
const string ctxput_shorts = ctxput + "ArrayShort( _ctx, (short*) $(name), $(size) );\n";
const string ctxput_ints = ctxput + "ArrayInteger( _ctx, (int*) $(name), $(size) );\n";
const string ctxput_floats = ctxput + "ArrayFloat( _ctx, (float*) $(name), $(size) );\n";
const string ctxput_doubles = ctxput + "ArrayDouble( _ctx, (double*) $(name), $(size) );\n";

const string ctxput_stream = ctxput + "File( _ctx, (FILE*) $(name) );\n"; //XXX PutFile
const string ctxput_voidp = ctxput + "CPointer( _ctx, (void*) $(name), 0 );\n";
const string ctxput_user = "  DaoContext_WrapCData( _ctx, (void*) $(name), dao_$(typer)_Typer );\n";
const string qt_ctxput = "  Dao_$(typer)_InitSS( ($(cxxtype)*) $(name) );\n";
const string qt_put_qobject =
"  DaoBase *dbase = DaoQt_Get_Wrapper( $(name) );\n\
  if( dbase ){\n\
    DaoContext_PutResult( _ctx, dbase );\n\
  }else{\n\
    Dao_$(typer)_InitSS( ($(cxxtype)*) $(name) );\n\
    DaoContext_WrapCData( _ctx, (void*) $(name), dao_$(typer)_Typer );\n\
  }\n\
";

const string ctxput_copycdata =
"  DaoContext_CopyCData( _ctx, (void*)& $(name), sizeof($(cxxtype)), dao_$(typer)_Typer );\n";
const string ctxput_newcdata =
"  DaoContext_PutCData( _ctx, (void*)new $(cxxtype)( $(name) ), dao_$(typer)_Typer );\n";
const string ctxput_refcdata =
"  DaoContext_WrapCData( _ctx, (void*)& $(name), dao_$(typer)_Typer );\n";

#if 0
const string qt_qlist_decl = 
"typedef $(qtype)<$(item)> $(qtype)_$(item);
void Dao_Put$(qtype)_$(item)( DaoContext *ctx, const $(qtype)_$(item) & qlist );
void Dao_Get$(qtype)_$(item)( DaoList *dlist, $(qtype)_$(item) & qlist );
";
const string qt_qlist_decl2 = 
"typedef $(qtype)<$(item)*> $(qtype)P_$(item);
void Dao_Put$(qtype)P_$(item)( DaoContext *ctx, const $(qtype)P_$(item) & qlist );
void Dao_Get$(qtype)P_$(item)( DaoList *dlist, $(qtype)P_$(item) & qlist );
";
const string qt_daolist_func =
"void Dao_Put$(qtype)_$(item)( DaoContext *ctx, const $(qtype)_$(item) & qlist )
{
	DaoList *dlist = DaoContext_PutList( ctx );
	DValue it = { DAO_CDATA, 0, 0, 0, {0} };
	int i, m = qlist.size();
	for(i=0; i<m; i++){
		it.v.cdata = DaoCData_New( dao_$(item)_Typer, new $(item)( qlist[i] ) );
		DaoList_PushBack( dlist, it );
	}
}
void Dao_Get$(qtype)_$(item)( DaoList *dlist, $(qtype)_$(item) & qlist )
{
	int i, m = DaoList_Size( dlist );
	for(i=0; i<m; i++){
		DValue it = DaoList_GetItem( dlist, i );
		if( it.t != DAO_CDATA ) continue;
		if( ! DaoCData_IsType( it.v.cdata, dao_$(item)_Typer ) ) continue;
		qlist.append( *($(item)*) DValue_CastCData( & it, dao_$(item)_Typer ) );
	}
}
";
const string qt_daolist_func_virt =
"void Dao_Put$(qtype)_$(item)( DaoContext *ctx, const $(qtype)_$(item) & qlist )
{
	DaoList *dlist = DaoContext_PutList( ctx );
	DValue it = { DAO_CDATA, 0, 0, 0, {0} };
	int i, m = qlist.size();
	for(i=0; i<m; i++){
		it.v.cdata = DaoCData_New( dao_$(item)_Typer, new $(item)( qlist[i] ) );
		DaoList_PushBack( dlist, it );
	}
}
void Dao_Get$(qtype)_$(item)( DaoList *dlist, $(qtype)_$(item) & qlist )
{
	int i, m = DaoList_Size( dlist );
	for(i=0; i<m; i++){
		DValue it = DaoList_GetItem( dlist, i );
		if( it.t != DAO_CDATA ) continue;
		if( ! DaoCData_IsType( it.v.cdata, dao_$(item)_Typer ) ) continue;
		qlist.append( * ($(item)*) DValue_CastCData( & it, dao_$(item)_Typer ) );
	}
}
";
const string qt_daolist_func2 =
"void Dao_Put$(qtype)P_$(item)( DaoContext *ctx, const $(qtype)P_$(item) & qlist )
{
	DaoList *dlist = DaoContext_PutList( ctx );
	DValue it = { DAO_CDATA, 0, 0, 0, {0} };
	int i, m = qlist.size();
	for(i=0; i<m; i++){
		it.v.cdata = DaoCData_Wrap( dao_$(item)_Typer, qlist[i] );
		DaoList_PushBack( dlist, it );
	}
}
void Dao_Get$(qtype)P_$(item)( DaoList *dlist, $(qtype)P_$(item) & qlist )
{
	int i, m = DaoList_Size( dlist );
	for(i=0; i<m; i++){
		DValue it = DaoList_GetItem( dlist, i );
		if( it.t != DAO_CDATA ) continue;
		if( ! DaoCData_IsType( it.v.cdata, dao_$(item)_Typer ) ) continue;
		qlist.append( ($(item)*) DValue_CastCData( & it, dao_$(item)_Typer ) );
	}
}
";
const string qt_daolist_func_virt2 =
"void Dao_Put$(qtype)P_$(item)( DaoContext *ctx, const $(qtype)P_$(item) & qlist )
{
	DaoList *dlist = DaoContext_PutList( ctx );
	DValue it = { DAO_CDATA, 0, 0, 0, {0} };
	int i, m = qlist.size();
	for(i=0; i<m; i++){
		it.v.cdata = DaoCData_Wrap( dao_$(item)_Typer, qlist[i] );
		DaoList_PushBack( dlist, it );
	}
}
void Dao_Get$(qtype)P_$(item)( DaoList *dlist, $(qtype)P_$(item) & qlist )
{
	int i, m = DaoList_Size( dlist );
	for(i=0; i<m; i++){
		DValue it = DaoList_GetItem( dlist, i );
		if( it.t != DAO_CDATA ) continue;
		if( ! DaoCData_IsType( it.v.cdata, dao_$(item)_Typer ) ) continue;
		qlist.append( ($(item)*) DValue_CastCData( & it, dao_$(item)_Typer ) );
	}
}
";
const string qt_dao2cxx_list =
"  $(cxxtype) $(name);
  Dao_Get$(qtype)_$(item)( _p[$(index)]->v.list, $(name) );
";
const string qt_dao2cxx_list2 =
"  $(cxxtype) $(name);
  Dao_Get$(qtype)P_$(item)( _p[$(index)]->v.list, $(name) );
";
const string qt_daolist_codes = "  Dao_Put$(qtype)_$(item)( _ctx, $(name) );\n";
const string qt_daolist_codes2 = "  Dao_Put$(qtype)P_$(item)( _ctx, $(name) );\n";
#endif

const string parset_int = "  _p[$(index)]->v.i = (int) $(name);\n";
const string parset_float = "  _p[$(index)]->v.f = (float) $(name);\n";
const string parset_double = "  _p[$(index)]->v.d = (double) $(name);\n";
const string parset_mbs = "  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name) );\n";
const string parset_wcs = "  DString_SetWCS( _p[$(index)]->v.s, (wchar_t*) $(name) );\n";
const string parset_bytes = "  DaoArray_FromByte( _p[$(index)]->v.array );\n";
const string parset_ubytes = "  DaoArray_FromUByte( _p[$(index)]->v.array );\n";
const string parset_shorts = "  DaoArray_FromShort( _p[$(index)]->v.array );\n";
const string parset_ushorts = "  DaoArray_FromUShort( _p[$(index)]->v.array );\n";
const string parset_ints = "  DaoArray_FromInt( _p[$(index)]->v.array );\n";
const string parset_uints = "  DaoArray_FromUInt( _p[$(index)]->v.array );\n";
const string parset_floats = "  DaoArray_FromFloat( _p[$(index)]->v.array );\n";
const string parset_doubles = "  DaoArray_FromDouble( _p[$(index)]->v.array );\n";

const string dao2cxx2cst = "  const $(cxxtype)* $(name)= (const $(cxxtype)*) ";
const string dao2cxx_mbs_cst = dao2cxx2cst + "DValue_GetMBString( _p[$(index)] );\n";
const string dao2cxx_wcs_cst = dao2cxx2cst + "DValue_GetWCString( _p[$(index)] );\n";

const string dao2cxx_mbs2 = 
"  $(cxxtype)* $(name)_old = ($(cxxtype)*) DValue_GetMBString( _p[$(index)] );\n\
  size_t $(name)_len = strlen( $(name)_old );\n\
  $(cxxtype)* $(name) = ($(cxxtype)*) malloc( $(name)_len + 1 );\n\
  void* $(name)_p = strncpy( $(name), $(name)_old, $(name)_len );\n";

const string dao2cxx_wcs2 = 
"  $(cxxtype)* $(name)_old = ($(cxxtype)*) DValue_GetWCString( _p[$(index)] );\n\
  size_t $(name)_len = wcslen( $(name)_old ) * sizeof(wchar_t);\n\
  $(cxxtype)* $(name) = ($(cxxtype)*) malloc( $(name)_len + sizeof(wchar_t) );\n\
  void* $(name)_p = memcpy( $(name), $(name)_old, $(name)_len );\n";

const string parset_mbs2 = 
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name) );\n\
  free( $(name) );\n";

const string parset_wcs2 = 
"  DString_SetWCS( _p[$(index)]->v.s, (wchar_t*) $(name) );\n\
  free( $(name) );\n";

const string dao2cxx_qchar = "  QChar $(name)( (int)_p[$(index)]->v.i );\n";
const string dao2cxx_qchar2 =
"  QChar $(name)( (int)_p[$(index)]->v.i )\
  QChar *$(name) = & _$(name);\n";

const string parset_qchar = "  _p[$(index)]->v.i = $(name).digitValue();\n";
const string parset_qchar2 = "  _p[$(index)]->v.i = $(name)->digitValue();\n";
const string ctxput_qchar = "  DaoContext_PutInteger( _ctx, $(name).digitValue() );\n";
const string ctxput_qchar2 = "  DaoContext_PutInteger( _ctx, $(name)->digitValue() );\n";

const string dao2cxx_qbytearray =
"  char *_mbs$(index) = DValue_GetMBString( _p[$(index)] );\n\
  QByteArray $(name)( _mbs$(index) );\n";

const string dao2cxx_qbytearray2 =
"  char *_mbs$(index) = DValue_GetMBString( _p[$(index)] );\n\
  QByteArray _$(name)( _mbs$(index) );\n\
  QByteArray *$(name) = & _$(name);\n";

const string parset_qbytearray =
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name).data() );\n";
const string parset_qbytearray2 =
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name)->data() );\n";
const string ctxput_qbytearray = 
"  DaoContext_PutMBString( _ctx, $(name).data() );\n";

const string dao2cxx_qstring =
"  char *_mbs$(index) = DValue_GetMBString( _p[$(index)] );\n\
  QString $(name)( _mbs$(index) );\n";

const string dao2cxx_qstring2 =
"  char *_mbs$(index) = DValue_GetMBString( _p[$(index)] );\n\
  QString _$(name)( _mbs$(index) );\n\
  QString *$(name) = & _$(name);\n";

const string parset_qstring =
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name).toLocal8Bit().data() );\n";
const string parset_qstring2 =
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name)->toLocal8Bit().data() );\n";
const string ctxput_qstring = 
"  DaoContext_PutMBString( _ctx, $(name).toLocal8Bit().data() );\n";

const string getres_i = "  if( _res.t == DAO_INTEGER ) $(name)= ($(cxxtype)) ";
const string getres_f = "  if( _res.t == DAO_FLOAT ) $(name)= ($(cxxtype)) ";
const string getres_d = "  if( _res.t == DAO_DOUBLE ) $(name)= ($(cxxtype)) ";
const string getres_s = "  if( _res.t == DAO_STRING ) $(name)= ($(cxxtype)*) ";
const string getres_a = "  if( _res.t == DAO_ARRAY )\n    $(name)= ($(cxxtype)*) ";
const string getres_p = "  if( _res.t == DAO_CDATA ) $(name)= ($(cxxtype)) ";
const string getres_io = "  if( _res.t == DAO_STREAM ) $(name)= ($(cxxtype)) ";

const string getres_int  = getres_i + "_res.v.i;\n";
const string getres_float = getres_f + "_res.v.f;\n";
const string getres_double = getres_d + "_res.v.d;\n";
const string getres_mbs = getres_s + "DValue_GetMBString( & _res );\n";
const string getres_wcs = getres_s + "DValue_GetWCString( & _res );\n";
const string getres_bytes = getres_a + "DaoArray_ToByte( _res.v.array );\n";
const string getres_ubytes = getres_a + "DaoArray_ToUByte( _res.v.array );\n";
const string getres_shorts = getres_a + "DaoArray_ToShort( _res.v.array );\n";
const string getres_ushorts = getres_a + "DaoArray_ToUShort( _res.v.array );\n";
const string getres_ints = getres_a + "DaoArray_ToInt( _res.v.array );\n";
const string getres_uints = getres_a + "DaoArray_ToUInt( _res.v.array );\n";
const string getres_floats = getres_a + "DaoArray_ToFloat( _res.v.array );\n";
const string getres_doubles = getres_a + "DaoArray_ToDouble( _res.v.array );\n";
const string getres_stream = getres_io + "DaoStream_GetFile( _res.v.stream );\n";
const string getres_buffer = getres_p + "DValue_GetCData( & _res );\n";

const string getres_qchar =
"  if( _res.t == DAO_INTEGER ) $(name)= QChar( (int)_res.v.i );\n";
const string getres_qbytearray =
"  if( _res.t == DAO_STRING ) $(name)= DValue_GetMBString( & _res );\n";
const string getres_qstring =
"  if( _res.t == DAO_STRING ) $(name)= DValue_GetMBString( & _res );\n";

const string getres_cdata = 
"  if( _res.t == DAO_OBJECT && (_cd = DaoObject_MapCData( _res.v.object, dao_$(typer)_Typer ) ) ){\n\
    _res.t = DAO_CDATA;\n\
    _res.v.cdata = _cd;\n\
  }\n\
  if( _res.t == DAO_CDATA && DaoCData_IsType( _res.v.cdata, dao_$(typer)_Typer ) ){\n";

const string getres_user = getres_cdata +
"    $(name) = ($(cxxtype)*) DValue_CastCData( &_res, dao_$(typer)_Typer );\n  }\n";

const string getres_user2 = getres_cdata +
"    $(name) = *($(cxxtype)*) DValue_CastCData( &_res, dao_$(typer)_Typer );\n  }\n";


const string getitem_int = ctxput + "Integer( _ctx, (int) self->$(name)[_p[1]->v.i] );\n";
const string getitem_float = ctxput + "Float( _ctx, (float) self->$(name)[_p[1]->v.i] );\n";
const string getitem_double = ctxput + "Double( _ctx, (double) self->$(name)[_p[1]->v.i] );\n";

const string setitem_int = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n\
  self->$(name)[_p[1]->v.i] = _p[2]->v.i;\n";
const string setitem_float = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n\
  self->$(name)[_p[1]->v.i] = _p[2]->v.f;\n";
const string setitem_double = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n\
  self->$(name)[_p[1]->v.i] = _p[2]->v.d;\n";

const string getitem_int2 = ctxput + "Integer( _ctx, (int) (*self)[_p[1]->v.i] );\n";
const string getitem_float2 = ctxput + "Float( _ctx, (float) (*self)[_p[1]->v.i] );\n";
const string getitem_double2 = ctxput + "Double( _ctx, (double) (*self)[_p[1]->v.i] );\n";

const string setitem_int2 = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n\
  (*self)[_p[1]->v.i] = _p[2]->v.i;\n";

const string setitem_float2 = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n\
  (*self)[_p[1]->v.i] = _p[2]->v.f;\n";

const string setitem_double2 = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n\
  (*self)[_p[1]->v.i] = _p[2]->v.d;\n";

const string setter_int = "  self->$(name) = ($(cxxtype)) _p[1]->v.i;\n";
const string setter_float = "  self->$(name) = ($(cxxtype)) _p[1]->v.f;\n";
const string setter_double = "  self->$(name) = ($(cxxtype)) _p[1]->v.d;\n";
const string setter_string = // XXX array?
"  int size = DString_Size( _p[1]->v.s );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DValue_GetMBString( _p[1] ), size );\n";

const string setter_shorts =
"  int size = DaoArray_Size( _p[1]->v.array );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoArray_ToShort( _p[1]->v.array ), size*sizeof(short) );\n";

const string setter_ints =
"  int size = DaoArray_Size( _p[1]->v.array );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoArray_ToInt( _p[1]->v.array ), size*sizeof(int) );\n";

const string setter_floats =
"  int size = DaoArray_Size( _p[1]->v.array );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoArray_ToFloat( _p[1]->v.array ), size*sizeof(float) );\n";

const string setter_doubles =
"  int size = DaoArray_Size( _p[1]->v.array );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoArray_ToDouble( _p[1]->v.array ), size*sizeof(double) );\n";

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );
extern string cdao_qname_to_idname( const string & qname );
extern string normalize_type_name( const string & name );

struct CDaoVarTemplates
{
	string daopar;
	string dao2cxx;
	string cxx2dao;
	string ctxput;
	string parset;
	string getres;
	string setter;
	string get_item;
	string set_item;

	void Generate( CDaoVariable *var, map<string,string> & kvmap, int daopid, int cxxpid );
	void SetupIntScalar(){
		daopar = daopar_int;
		dao2cxx = dao2cxx_int;
		cxx2dao = cxx2dao_int;
		ctxput = ctxput_int;
		getres = getres_int;
		setter = setter_int;
	}
	void SetupFloatScalar(){
		daopar = daopar_float;
		dao2cxx = dao2cxx_float;
		cxx2dao = cxx2dao_float;
		ctxput = ctxput_float;
		getres = getres_float;
		setter = setter_float;
	}
	void SetupDoubleScalar(){
		daopar = daopar_double;
		dao2cxx = dao2cxx_double;
		cxx2dao = cxx2dao_double;
		ctxput = ctxput_double;
		getres = getres_double;
		setter = setter_double;
	}
	void SetupMBString(){
		daopar = daopar_string;
		dao2cxx = dao2cxx_mbs;
		cxx2dao = cxx2dao_mbs;
		ctxput = ctxput_mbs;
		parset = parset_mbs;
		getres = getres_mbs;
	}
	void SetupWCString(){
		daopar = daopar_string;
		dao2cxx = dao2cxx_wcs;
		cxx2dao = cxx2dao_wcs;
		ctxput = ctxput_wcs;
		parset = parset_wcs;
		getres = getres_wcs;
	}
};
void CDaoVarTemplates::Generate( CDaoVariable *var, map<string,string> & kvmap, int daopid, int cxxpid )
{
	string dft, typer = cdao_qname_to_idname( var->daotype );
	char sindex[50];
	sprintf( sindex, "%i", daopid );
	if( var->isNullable ) dft = "|null";
	if( var->daodefault.size() ) dft += " =" + var->daodefault;

	kvmap[ "daotype" ] = var->daotype;
	kvmap[ "cxxtype" ] = var->cxxtype2;
	kvmap[ "typer" ] = typer;
	kvmap[ "name" ] = var->name;
	kvmap[ "namespace" ] = "";
	kvmap[ "namespace2" ] = "";
	kvmap[ "index" ] = sindex;
	kvmap[ "default" ] = dft;
	kvmap[ "refer" ] = var->cxxcall;
	if( var->isCallback ){
		var->cxxtype = normalize_type_name( var->qualType.getAsString() );
		kvmap[ "cxxtype" ] = var->cxxtype;
	}
	var->daopar = cdao_string_fill( daopar, kvmap );
	var->dao2cxx = cdao_string_fill( dao2cxx, kvmap );
	var->parset = cdao_string_fill( parset, kvmap );
	sprintf( sindex, "%i", cxxpid );
	kvmap[ "index" ] = sindex;
	var->cxx2dao = cdao_string_fill( cxx2dao, kvmap );
	if( daopid == VAR_INDEX_RETURN ){
		var->ctxput = cdao_string_fill( ctxput, kvmap );
		var->getres = cdao_string_fill( getres, kvmap );
	}else if( daopid == VAR_INDEX_FIELD ){
		if( not var->qualType.isConstQualified() ){
			var->setter = cdao_string_fill( setter, kvmap );
			var->set_item = cdao_string_fill( set_item, kvmap );
		}
		var->get_item = cdao_string_fill( get_item, kvmap );
		kvmap[ "name" ] = "self->" + var->name;
		var->getter = cdao_string_fill( ctxput, kvmap );
	}
	//outs() << var->daopar << "\n";
	//outs() << var->dao2cxx << "\n";
	//outs() << var->cxx2dao << "\n";
	//outs() << var->parset << "\n";
}

CDaoVariable::CDaoVariable( CDaoModule *mod, const VarDecl *decl )
{
	module = mod;
	initor = NULL;
	isNullable = false;
	isCallback = false;
	isUserData = false;
	hasArrayHint = false;
	unsupported = false;
	useDefault = true;
	SetDeclaration( decl );
}
void CDaoVariable::SetQualType( QualType qtype )
{
	qualType = qtype;
	canoType = qtype.getCanonicalType();
}
void CDaoVariable::SetDeclaration( const VarDecl *decl )
{
	if( decl == NULL ) return;
	name = decl->getName().str();
	//outs() << "variable: " << name << " " << decl->getType().getAsString() << "\n";
	SetQualType( decl->getTypeSourceInfo()->getType() );
	initor = decl->getAnyInitializer();
}
void CDaoVariable::SetHints( const string & hints )
{
	size_t pos = hints.find( "_dao_hint_" );
	string hints2, hint;
	name = hints.substr( 0, pos );
	if( pos == string::npos ) return;
	hints2 = hints.substr( pos+4 );
	while( hints2.find( "_hint_" ) == 0 ){
		pos = hints2.find( '_', 6 );
		if( pos == string::npos ){
			hint = hints2.substr( 6 );
		}else{
			hint = hints2.substr( 6, pos - 6 );
		}
		if( hint == "nullable" ){
			isNullable = true;
		}if( hint == "unsupported" ){
			unsupported = true;
		}if( hint == "callbackdata" ){
			isUserData = true;
			size_t pos2 = hints2.find( "_hint_", pos );
			if( pos2 == string::npos ){
				callback = hints2.substr( pos+1 );
			}else if( pos2 > pos ){
				callback = hints2.substr( pos+1, pos2 - pos - 1 );
			}
			if( callback == "" ) errs() << "Warning: need callback name for \"callbackdata\" hint!\n";
		}else if( hint == "array" ){
			size_t pos2 = hints2.find( "_hint_", pos );
			hint = "";
			if( pos2 != pos ){
				if( pos2 == string::npos ){
					hint = hints2.substr( pos+1 );
				}else if( pos2 > pos ){
					hint = hints2.substr( pos+1, pos2 - pos - 1 );
				}
			}
			size_t from = 0;
			while( (pos = hint.find( '_', from )) < pos2 ){
				sizes.push_back( hint.substr( from, pos - from ) );
				from = pos + 1;
			}
			if( from < pos2 ){
				if( pos2 == string::npos ){
					sizes.push_back( hint.substr( from ) );
				}else{
					sizes.push_back( hint.substr( from, pos2 - from ) );
				}
			}
			//outs() << "array hint: " << hint << " " << sizes.size() << "\n";
			pos = pos2;
		}
		hints2.erase( 0, pos );
	}
}
int CDaoVariable::Generate( int daopar_index, int cxxpar_index )
{
	string prefix, suffix;
	int retcode;
	if( name == "" ) name = "_p" + utostr( daopar_index );
	retcode = Generate2( daopar_index, cxxpar_index );
	unsupported = unsupported or (retcode != 0);
	if( unsupported == false ){
		MakeCxxParameter( prefix, suffix );
		cxxpar = prefix + " " + name + suffix;
	}
	return retcode || unsupported;
}
int CDaoVariable::Generate2( int daopar_index, int cxxpar_index )
{
	if( initor ){
		SourceRange range = initor->getSourceRange();
		daodefault = module->ExtractSource( range, true );
		cxxdefault = "=" + daodefault;
		if( daodefault == "0L" ) daodefault = "0";

		Preprocessor & pp = module->compiler->getPreprocessor();
		SourceManager & sm = module->compiler->getSourceManager();
		SourceLocation start = sm.getInstantiationLoc( range.getBegin() );
		SourceLocation end = sm.getInstantiationLoc( range.getEnd() );
		const char *p = sm.getCharacterData( start );
		const char *q = sm.getCharacterData( pp.getLocForEndOfToken( end ) );

		Lexer lexer( start, module->compiler->getLangOpts(), p, p, q );
		Token token;
		vector<Token> tokens;
		while( lexer.getBufferLocation() < q ){
			lexer.Lex( token );
			tokens.push_back( token );
		}
		if( tokens.size() > 1 ){
			for(int i=0,n=tokens.size(); i<n; i++){
				tok::TokenKind kind = tokens[i].getKind();
				if( kind != tok::raw_identifier && kind != tok::coloncolon ){
					daodefault = "0";
					isNullable = true;
					useDefault = false;
					break;
				}
			}
		}
	}
#if 0
	if( ParmVarDecl *par = dyn_cast<ParmVarDecl>( decl ) ){
		SourceRange range = par->getDefaultArgRange();
		outs() << (range.getBegin() == range.getEnd()) << "\n";
	}
#endif
	const Type *type = canoType.getTypePtr();
	string ctypename = normalize_type_name( canoType.getAsString() );
	cxxtype2 = normalize_type_name( GetStrippedType( canoType ).getAsString() );
	cxxtype = ctypename;
	cxxcall = name;
	//outs() << cxxtype << " " << cxxdefault << "  " << "\n";
	//outs() << type->isPointerType() << " is pointer type\n";
	//outs() << type->isArrayType() << " is array type\n";
	//outs() << type->isConstantArrayType() << " is constant array type\n";
	if( type->isBuiltinType() )
		return Generate( (const BuiltinType*)type, daopar_index, cxxpar_index );
	if( type->isPointerType() and not hasArrayHint )
		return Generate( (const PointerType*)type, daopar_index, cxxpar_index );
	if( type->isReferenceType() )
		return Generate( (const ReferenceType*)type, daopar_index, cxxpar_index );
	if( type->isArrayType() )
		return Generate( (const ArrayType*)type, daopar_index, cxxpar_index );
	if( type->isEnumeralType() ){
		daotype = "int";
		CDaoVarTemplates tpl;
		tpl.SetupIntScalar();
		map<string,string> kvmap;
		tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
		return 0;
	}
	return 1;
}
int CDaoVariable::Generate( const BuiltinType *type, int daopar_index, int cxxpar_index )
{
	CDaoVarTemplates tpl;
	if( type->isArithmeticType() ){
		daotype = "int";
		isNullable = false;
		tpl.SetupIntScalar();
		switch( type->getKind() ){
		case BuiltinType::Bool :
		case BuiltinType::Char_U :
		case BuiltinType::UChar :
		case BuiltinType::WChar_U :
		case BuiltinType::Char16 :
		case BuiltinType::Char32 :
		case BuiltinType::UShort :
		case BuiltinType::UInt :
		case BuiltinType::ULong :
		case BuiltinType::ULongLong : // FIXME
		case BuiltinType::UInt128 : // FIXME
		case BuiltinType::Char_S :
		case BuiltinType::SChar :
		case BuiltinType::WChar_S :
		case BuiltinType::Short :
		case BuiltinType::Int :
		case BuiltinType::Long :
		case BuiltinType::LongLong : // FIXME
		case BuiltinType::Int128 : // FIXME
			break;
		case BuiltinType::Float :
			daotype = "float";
			tpl.SetupFloatScalar();
			break;
		case BuiltinType::Double :
		case BuiltinType::LongDouble : // FIXME
			daotype = "double";
			tpl.SetupDoubleScalar();
			break;
		default : break;
		}
	}
	map<string,string> kvmap;
	tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
	return 0;
}
int CDaoVariable::Generate( const PointerType *type, int daopar_index, int cxxpar_index )
{
	QualType qtype2 = type->getPointeeType();
	string ctypename2 = qtype2.getAsString();
	const Type *type2 = qtype2.getTypePtr();
	const RecordDecl *decl = type2->getAsCXXRecordDecl();
	if( decl == NULL ){
		const RecordType *t = type2->getAsStructureType();
		if( t == NULL ) t = type2->getAsUnionType();
		if( t ) decl = t->getDecl();
	}

	if( sizes.size() == 1 ) return GenerateForArray( qtype2, sizes[0], daopar_index, cxxpar_index );
	if( sizes.size() == 2 && type2->isPointerType() ){
		QualType qtype3 = ((const PointerType*)type2)->getPointeeType();
		return GenerateForArray( qtype3, sizes[0], sizes[1], daopar_index, cxxpar_index );
	}

	CDaoVarTemplates tpl;
	map<string,string> kvmap;
	kvmap[ "size" ] = "0";
	if( type2->isBuiltinType() and type2->isArithmeticType() ){
		const BuiltinType *type3 = (const BuiltinType*) type2;
		daotype = "int";
		cxxcall = "&" + name;
		isNullable = false;
		tpl.SetupIntScalar();
		switch( type3->getKind() ){
		case BuiltinType::Char_U :
		case BuiltinType::UChar :
		case BuiltinType::Char_S :
		case BuiltinType::SChar :
			daotype = "string";
			cxxcall = name;
			tpl.SetupMBString();
			tpl.parset = parset_mbs;
			if( daodefault == "0" || daodefault == "NULL" ) daodefault = "\'\'";
			break;
		case BuiltinType::WChar_U :
		case BuiltinType::WChar_S :
		case BuiltinType::Char16 :
		case BuiltinType::Char32 :
			daotype = "string";
			cxxcall = name;
			tpl.SetupWCString();
			tpl.parset = parset_wcs;
			if( daodefault == "0" || daodefault == "NULL" ) daodefault = "\"\"";
			break;
		case BuiltinType::UShort :
		case BuiltinType::Bool :
		case BuiltinType::UInt :
		case BuiltinType::ULong :
		case BuiltinType::ULongLong :
		case BuiltinType::UInt128 :
		case BuiltinType::Short :
		case BuiltinType::Int :
		case BuiltinType::Long :
		case BuiltinType::LongLong :  // FIXME
		case BuiltinType::Int128 :  // FIXME
			daotype = "int";
			tpl.SetupIntScalar();
			tpl.parset = parset_int;
			tpl.ctxput = ctxput_ints;
			tpl.getres = getres_ints;
			tpl.cxx2dao = cxx2dao_int2;
			break;
		case BuiltinType::Float :
			daotype = "float";
			tpl.SetupFloatScalar();
			tpl.parset = parset_float;
			tpl.ctxput = ctxput_floats;
			tpl.getres = getres_floats;
			tpl.cxx2dao = cxx2dao_float2;
			break;
		case BuiltinType::Double :
		case BuiltinType::LongDouble : // FIXME
			daotype = "double";
			tpl.SetupDoubleScalar();
			tpl.parset = parset_double;
			tpl.ctxput = ctxput_doubles;
			tpl.getres = getres_doubles;
			tpl.cxx2dao = cxx2dao_double2;
			break;
		default : break;
		}
	}else if( type2->isEnumeralType() ){
		daotype = "int";
		cxxcall = "&" + name;
		isNullable = false;

		tpl.SetupIntScalar();
		tpl.parset = parset_int;
		tpl.ctxput = ctxput_ints;
		tpl.getres = getres_ints;
		tpl.cxx2dao = cxx2dao_int2;
	}else if( decl ){
		daotype = decl->getQualifiedNameAsString();
		decl = decl->getDefinition();
		tpl.daopar = daopar_user;
		tpl.ctxput = ctxput_user;
		tpl.getres = getres_user;
		tpl.dao2cxx = dao2cxx_user2;
		tpl.cxx2dao = cxx2dao_user;
		if( daodefault == "0" || daodefault == "NULL" ){
			daodefault = "null";
			isNullable = true;
		}
	}else if( type->isVoidPointerType() ){
		if( isUserData ){
			daotype = "any";
			cxxtype = "DValue";
			cxxpar = "DValue " + name;
			cxxcall = name;
			tpl.daopar = daopar_userdata;
			tpl.dao2cxx = dao2cxx_userdata;
			tpl.cxx2dao = cxx2dao_userdata;
			kvmap[ "callback" ] = callback;
		}else{
			daotype = "cdata";
			tpl.daopar = daopar_buffer;
			tpl.dao2cxx = dao2cxx_void;
			tpl.cxx2dao = cxx2dao_voidp;
			tpl.ctxput = ctxput_voidp;
		}
		if( daodefault == "0" || daodefault == "NULL" ){
			daodefault = "null";
			isNullable = true;
		}
	}else if( const FunctionProtoType *ft = dyn_cast<FunctionProtoType>( type2 ) ){
		//outs() << "callback: " << qualType.getAsString() << " " << qtype2.getAsString() << "\n";
		if( module->allCallbacks.find( ft ) == module->allCallbacks.end() ){
			module->allCallbacks[ ft ] = new CDaoFunction( module );
			CDaoFunction *func = module->allCallbacks[ ft ];
			string qname = GetStrippedType( qualType ).getAsString();
			func->SetCallback( (FunctionProtoType*)ft, NULL, qname );
			func->cxxName = cdao_qname_to_idname( qname );
			if( func->retype.callback == "" ){
				errs() << "Warning: callback \"" << qualType.getAsString() << "\" is not supported!\n";
				func->excluded = true;
			}
		}
		CDaoFunction *func = module->allCallbacks[ ft ];
		func->Generate();
		if( func->excluded ) return 1;
		daotype = "any";
		isCallback = true;
		tpl.daopar = daopar_callback;
		tpl.dao2cxx = dao2cxx_callback;
		kvmap[ "callback" ] = func->cxxName; // XXX
	}else{
		return 1;
	}
	tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
	return 0;
}
int CDaoVariable::Generate( const ReferenceType *type, int daopar_index, int cxxpar_index )
{
	QualType qtype2 = type->getPointeeType();
	string ctypename2 = qtype2.getAsString();
	const Type *type2 = qtype2.getTypePtr();
	const RecordDecl *decl = type2->getAsCXXRecordDecl();
	if( decl == NULL ){
		const RecordType *t = type2->getAsStructureType();
		if( t == NULL ) t = type2->getAsUnionType();
		if( t ) decl = t->getDecl();
	}

	CDaoVarTemplates tpl;
	if( type2->isBuiltinType() and type2->isArithmeticType() ){
		const BuiltinType *type3 = (const BuiltinType*) type2;
		daotype = "int";
		cxxcall = name;
		isNullable = false;
		tpl.SetupIntScalar();
		switch( type3->getKind() ){
		case BuiltinType::Char_U :
		case BuiltinType::UChar :
		case BuiltinType::Char_S :
		case BuiltinType::SChar :
		case BuiltinType::WChar_U :
		case BuiltinType::WChar_S :
		case BuiltinType::Char16 :
		case BuiltinType::Char32 :
		case BuiltinType::UShort :
		case BuiltinType::Bool :
		case BuiltinType::UInt :
		case BuiltinType::ULong :
		case BuiltinType::ULongLong :
		case BuiltinType::UInt128 :
		case BuiltinType::Short :
		case BuiltinType::Int :
		case BuiltinType::Long :
		case BuiltinType::LongLong :  // FIXME
		case BuiltinType::Int128 :  // FIXME
			daotype = "int";
			tpl.SetupIntScalar();
			tpl.parset = parset_int;
			break;
		case BuiltinType::Float :
			daotype = "float";
			tpl.SetupFloatScalar();
			tpl.parset = parset_float;
			break;
		case BuiltinType::Double :
		case BuiltinType::LongDouble : // FIXME
			daotype = "double";
			tpl.SetupDoubleScalar();
			tpl.parset = parset_double;
			break;
		default : break;
		}
	}else if( type2->isEnumeralType() ){
		daotype = "int";
		cxxcall = name;
		isNullable = false;

		tpl.SetupIntScalar();
		tpl.parset = parset_int;
	}else if( decl ){
		daotype = decl->getQualifiedNameAsString();
		decl = decl->getDefinition();
		cxxcall = "*" + name;
		tpl.daopar = daopar_user;
		tpl.ctxput = ctxput_user;
		tpl.getres = getres_user;
		tpl.dao2cxx = dao2cxx_user2;
		tpl.cxx2dao = cxx2dao_user;
		if( daodefault == "0" || daodefault == "NULL" ){
			daodefault = "null";
			isNullable = true;
		}
	}else{
		return 1;
	}
	map<string,string> kvmap;
	tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
	return 0;
}
int CDaoVariable::Generate( const ArrayType *type, int daopar_index, int cxxpar_index )
{
	string size;
	if( type->isConstantArrayType() ){
		ConstantArrayType *at = (ConstantArrayType*) type;
		size = at->getSize().toString( 10, false );
	}
	return GenerateForArray( type->getElementType(), size, daopar_index, cxxpar_index );
}
int CDaoVariable::GenerateForArray( QualType elemtype, string size, int daopar_index, int cxxpar_index )
{
	const Type *type2 = elemtype.getTypePtr();
	string ctypename2 = elemtype.getAsString();
	if( type2->isArrayType() ){
		const ArrayType *type3 = (ArrayType*) type2;
		string size2;
		if( type2->isConstantArrayType() ){
			ConstantArrayType *at = (ConstantArrayType*) type2;
			size2 = at->getSize().toString( 10, false );
		}
		return GenerateForArray( type3->getElementType(), size, size2, daopar_index, cxxpar_index );
	}
	CDaoVarTemplates tpl;
	if( type2->isBuiltinType() and type2->isArithmeticType() ){
		const BuiltinType *type3 = (const BuiltinType*) type2;
		daotype = "array<int>";
		dao_itemtype = "int";
		cxxcall = name;
		tpl.daopar = daopar_ints;
		tpl.ctxput = ctxput_ints;
		tpl.parset = parset_ints;
		tpl.getres = getres_ints;
		tpl.setter = setter_ints;
		tpl.get_item = name == "this" ? getitem_int2 : getitem_int;
		tpl.set_item = name == "this" ? setitem_int2 : setitem_int;
		if( daodefault == "0" || daodefault == "NULL" ) daodefault = "[]";
		switch( type3->getKind() ){
		case BuiltinType::Char_S :
		case BuiltinType::SChar :
			tpl.dao2cxx = dao2cxx_bytes;
			tpl.cxx2dao = cxx2dao_bytes;
			tpl.ctxput = ctxput_bytes;
			tpl.parset = parset_bytes;
			tpl.getres = getres_bytes;
			tpl.setter = setter_string;
			break;
		case BuiltinType::Bool :
		case BuiltinType::Char_U :
		case BuiltinType::UChar :
			tpl.dao2cxx = dao2cxx_ubytes;
			tpl.cxx2dao = cxx2dao_ubytes;
			tpl.ctxput = ctxput_bytes;
			tpl.parset = parset_ubytes;
			tpl.getres = getres_ubytes;
			tpl.setter = setter_string;
			break;
		case BuiltinType::UShort :
			tpl.dao2cxx = dao2cxx_ushorts;
			tpl.cxx2dao = cxx2dao_ushorts;
			tpl.ctxput = ctxput_shorts;
			tpl.parset = parset_ushorts;
			tpl.getres = getres_ushorts;
			tpl.setter = setter_shorts;
			break;
		case BuiltinType::Char16 :
		case BuiltinType::Short :
			tpl.dao2cxx = dao2cxx_shorts;
			tpl.cxx2dao = cxx2dao_shorts;
			tpl.ctxput = ctxput_shorts;
			tpl.parset = parset_shorts;
			tpl.getres = getres_shorts;
			tpl.setter = setter_shorts;
			break;
		case BuiltinType::WChar_U : // FIXME: check size
		case BuiltinType::UInt :
		case BuiltinType::ULong :
		case BuiltinType::ULongLong : // FIXME
		case BuiltinType::UInt128 : // FIXME
			tpl.dao2cxx = dao2cxx_uints;
			tpl.cxx2dao = cxx2dao_uints;
			break;
		case BuiltinType::WChar_S :
		case BuiltinType::Char32 :
		case BuiltinType::Int :
		case BuiltinType::Long : // FIXME: check size
		case BuiltinType::LongLong : // FIXME
		case BuiltinType::Int128 : // FIXME
			tpl.dao2cxx = dao2cxx_ints;
			tpl.cxx2dao = cxx2dao_ints;
			break;
		case BuiltinType::Float :
			daotype = "array<float>";
			dao_itemtype = "float";
			tpl.daopar = daopar_floats;
			tpl.dao2cxx = dao2cxx_floats;
			tpl.cxx2dao = cxx2dao_floats;
			tpl.ctxput = ctxput_floats;
			tpl.parset = parset_floats;
			tpl.getres = getres_floats;
			tpl.setter = setter_floats;
			tpl.get_item = name == "this" ? getitem_float2 : getitem_float;
			tpl.set_item = name == "this" ? setitem_float2 : setitem_float;
			break;
		case BuiltinType::Double :
		case BuiltinType::LongDouble : // FIXME
			daotype = "array<double>";
			dao_itemtype = "double";
			tpl.daopar = daopar_doubles;
			tpl.dao2cxx = dao2cxx_doubles;
			tpl.cxx2dao = cxx2dao_doubles;
			tpl.ctxput = ctxput_doubles;
			tpl.parset = parset_doubles;
			tpl.getres = getres_doubles;
			tpl.setter = setter_doubles;
			tpl.get_item = name == "this" ? getitem_double2 : getitem_double;
			tpl.set_item = name == "this" ? setitem_double2 : setitem_double;
			break;
		default : break;
		}
	}else{
		return 1;
	}
	map<string,string> kvmap;
	if( size.size() ) kvmap[ "size" ] = size;
	tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
	return 0;
}
int CDaoVariable::GenerateForArray( QualType elemtype, string size, string size2, int dpid, int cpid )
{
	const Type *type2 = elemtype.getTypePtr();
	string ctypename2 = elemtype.getAsString();
	CDaoVarTemplates tpl;
	if( type2->isBuiltinType() and type2->isArithmeticType() ){
		const BuiltinType *type3 = (const BuiltinType*) type2;
		daotype = "array<int>";
		dao_itemtype = "int";
		cxxcall = name;
		tpl.daopar = daopar_ints;
		tpl.ctxput = ctxput_ints;
		tpl.parset = parset_ints;
		tpl.getres = getres_ints;
		tpl.setter = setter_ints;
		tpl.get_item = name == "this" ? getitem_int2 : getitem_int;
		tpl.set_item = name == "this" ? setitem_int2 : setitem_int;
		if( daodefault == "0" || daodefault == "NULL" ) daodefault = "[]";
		switch( type3->getKind() ){
		case BuiltinType::Char_S :
		case BuiltinType::SChar :
			tpl.dao2cxx = dao2cxx_bmat;
			tpl.cxx2dao = cxx2dao_bmat;
			tpl.ctxput = ctxput_bytes;
			tpl.parset = parset_bytes;
			tpl.getres = getres_bytes;
			tpl.setter = setter_string;
			break;
		case BuiltinType::Bool :
		case BuiltinType::Char_U :
		case BuiltinType::UChar :
			tpl.dao2cxx = dao2cxx_ubmat;
			tpl.cxx2dao = cxx2dao_ubmat;
			tpl.ctxput = ctxput_bytes;
			tpl.parset = parset_ubytes;
			tpl.getres = getres_ubytes;
			tpl.setter = setter_string;
			break;
		case BuiltinType::UShort :
			tpl.dao2cxx = dao2cxx_usmat;
			tpl.cxx2dao = cxx2dao_usmat;
			tpl.ctxput = ctxput_shorts;
			tpl.parset = parset_ushorts;
			tpl.getres = getres_ushorts;
			tpl.setter = setter_shorts;
			break;
		case BuiltinType::Char16 :
		case BuiltinType::Short :
			tpl.dao2cxx = dao2cxx_smat;
			tpl.cxx2dao = cxx2dao_smat;
			tpl.ctxput = ctxput_shorts;
			tpl.parset = parset_shorts;
			tpl.getres = getres_shorts;
			tpl.setter = setter_shorts;
			break;
		case BuiltinType::WChar_U : // FIXME: check size
		case BuiltinType::UInt :
		case BuiltinType::ULong :
		case BuiltinType::ULongLong : // FIXME
		case BuiltinType::UInt128 : // FIXME
			tpl.dao2cxx = dao2cxx_uimat;
			tpl.cxx2dao = cxx2dao_uimat;
			break;
		case BuiltinType::WChar_S :
		case BuiltinType::Char32 :
		case BuiltinType::Int :
		case BuiltinType::Long : // FIXME: check size
		case BuiltinType::LongLong : // FIXME
		case BuiltinType::Int128 : // FIXME
			tpl.dao2cxx = dao2cxx_imat;
			tpl.cxx2dao = cxx2dao_imat;
			break;
		case BuiltinType::Float :
			daotype = "array<float>";
			dao_itemtype = "float";
			tpl.daopar = daopar_floats;
			tpl.dao2cxx = dao2cxx_fmat;
			tpl.cxx2dao = cxx2dao_fmat;
			tpl.ctxput = ctxput_floats;
			tpl.parset = parset_floats;
			tpl.getres = getres_floats;
			tpl.setter = setter_floats;
			tpl.get_item = name == "this" ? getitem_float2 : getitem_float;
			tpl.set_item = name == "this" ? setitem_float2 : setitem_float;
			break;
		case BuiltinType::Double :
		case BuiltinType::LongDouble : // FIXME
			daotype = "array<double>";
			dao_itemtype = "double";
			tpl.daopar = daopar_doubles;
			tpl.dao2cxx = dao2cxx_dmat;
			tpl.cxx2dao = cxx2dao_dmat;
			tpl.ctxput = ctxput_doubles;
			tpl.parset = parset_doubles;
			tpl.getres = getres_doubles;
			tpl.setter = setter_doubles;
			tpl.get_item = name == "this" ? getitem_double2 : getitem_double;
			tpl.set_item = name == "this" ? setitem_double2 : setitem_double;
			break;
		default : break;
		}
	}else{
		return 1;
	}
	map<string,string> kvmap;
	if( size.size() ) kvmap[ "size" ] = size;
	if( size2.size() ) kvmap[ "size2" ] = size2;
	tpl.Generate( this, kvmap, dpid, cpid );
	return 0;
}
void CDaoVariable::MakeCxxParameter( string & prefix, string & suffix )
{
	const Type *type = qualType.getTypePtr();
	if( dyn_cast<TypedefType>( type ) ){
		prefix = qualType.getAsString();
	}else{
		MakeCxxParameter( canoType, prefix, suffix );
	}
}
void CDaoVariable::MakeCxxParameter( QualType qtype, string & prefix, string & suffix )
{
	const Type *type = qtype.getTypePtr();
	const RecordDecl *decl = type->getAsCXXRecordDecl();
	if( decl == NULL ){
		const RecordType *t = type->getAsStructureType();
		if( t == NULL ) t = type->getAsUnionType();
		if( t ) decl = t->getDecl();
	}
	if( type->isBuiltinType() ){
		prefix = qtype.getAsString() + prefix;
	}else if( type->isPointerType() ){
		const PointerType *type2 = (const PointerType*) type;
		prefix += "*";
		MakeCxxParameter( type2->getPointeeType(), prefix, suffix );
	}else if( type->isReferenceType() ){
		const ReferenceType *type2 = (const ReferenceType*) type;
		prefix += "&";
		MakeCxxParameter( type2->getPointeeType(), prefix, suffix );
	}else if( type->isArrayType() ){
		const ArrayType *type2 = (const ArrayType*) type;
		string size = "[]";
		if( type->isConstantArrayType() ){
			ConstantArrayType *at = (ConstantArrayType*) type;
			size = "[" + at->getSize().toString( 10, false ) + "]";
		}
		suffix += size;
		MakeCxxParameter( type2->getElementType(), prefix, suffix );
	}else if( type->isEnumeralType() ){
		const EnumType *type2 = (const EnumType*) type;
		const EnumDecl *edec = type2->getDecl();
		const DeclContext *parent = edec ? edec->getParent() : NULL;
		if( parent && parent->isRecord() && edec->getAccess() != AS_public ) unsupported = true;
		prefix = normalize_type_name( qtype.getAsString() ) + prefix;
	}else if( decl ){
		// const C & other: const is part of the name, not a qualifier.
		prefix = normalize_type_name(qtype.getAsString()) + prefix;
	}
}
QualType CDaoVariable::GetStrippedType( QualType qtype )
{
	const Type *type = qtype.getTypePtr();
	if( type->isPointerType() ){
		const PointerType *type2 = (const PointerType*) type;
		return type2->getPointeeType();
		return GetStrippedType( type2->getPointeeType() );
	}else if( type->isReferenceType() ){
		const ReferenceType *type2 = (const ReferenceType*) type;
		return GetStrippedType( type2->getPointeeType() );
	}else if( type->isArrayType() ){
		const ArrayType *type2 = (const ArrayType*) type;
		return GetStrippedType( type2->getElementType() );
	}
	return qtype;
}
