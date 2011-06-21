
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
const string daopar_user = "$(name) :$(namespace2)$(type2)$(default)";
const string daopar_userdata = "$(name) :any$(default)"; // for callback data
const string daopar_callback = "$(name) :any"; // for callback, no precise type yet! XXX

const string dao2cxx = "  $(namespace)$(type) $(name)= ($(namespace)$(type)) ";
const string dao2cxx2 = "  $(namespace)$(type)* $(name)= ($(namespace)$(type)*) ";
const string dao2cxx3 = "  $(namespace)$(type)** $(name)= ($(namespace)$(type)**) ";

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

const string dao2cxx_stream = dao2cxx2 + "DaoStream_GetFile( _p[$(index)]->v.stream );\n";

const string dao2cxx_void = dao2cxx2 + 
"DValue_GetCData( _p[$(index)] );\n";
const string dao2cxx_void2 = "  $(namespace)$(type) $(name)= ($(namespace)$(type)) "
"DValue_GetCData( _p[$(index)] );\n";

const string dao2cxx_user = dao2cxx2 + 
"DValue_CastCData( _p[$(index)], dao_$(type2)_Typer );\n";
const string dao2cxx_user2 = "  $(namespace)$(type) $(name)= ($(namespace)$(type)) "
"DValue_CastCData( _p[$(index)], dao_$(type2)_Typer );\n";
const string dao2cxx_user3 = "  $(namespace)$(type)** $(name)= ($(namespace)$(type)**) "
"DValue_GetCData2( _p[$(index)] );\n";
const string dao2cxx_user4 = "  $(namespace)$(type)* $(name)= ($(namespace)$(type)*) "
"DValue_GetCData2( _p[$(index)] );\n";

const string dao2cxx_callback =
"  DaoMethod *_ro = (DaoMethod*) _p[$(index)]->v.p;\
  $(type) *$(name) = Dao_$(type);\n";
const string dao2cxx_userdata =
"  DValue *_ud = _p[$(index)];\
  DaoCallbackData *$(name) = DaoCallbackData_New( _ro, *_ud );\
  if( $(name) == NULL ){\
    DaoContext_RaiseException( _ctx, DAO_ERROR_PARAM, \"invalid callback\" );\
	return;\
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

const string cxx2dao_stream = cxx2dao + "Stream( (FILE*) $(refer) );\n";
const string cxx2dao_voidp = "  _dp[$(index)] = DValue_WrapCData( NULL, (void*) $(refer) );\n";
const string cxx2dao_user = "  _dp[$(index)] = DValue_WrapCData( dao_$(type2)_Typer, (void*) $(refer) );\n";

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
const string ctxput_user = "  DaoContext_WrapCData( _ctx, (void*) $(name), dao_$(type2)_Typer );\n";
const string qt_ctxput = "  Dao_$(type2)_InitSS( ($(type)*) $(name) );\n";
const string qt_put_qobject =
"  DaoBase *dbase = DaoQt_Get_Wrapper( $(name) );\
  if( dbase ){\
    DaoContext_PutResult( _ctx, dbase );\
  }else{\
    Dao_$(type2)_InitSS( ($(type)*) $(name) );\
    DaoContext_WrapCData( _ctx, (void*) $(name), dao_$(type2)_Typer );\
  }\
";

const string ctxput_copycdata =
"  DaoContext_CopyCData( _ctx, (void*)& $(name), sizeof($(namespace)$(type)), dao_$(type2)_Typer );\n";
const string ctxput_newcdata =
"  DaoContext_PutCData( _ctx, (void*)new $(namespace)$(type)( $(name) ), dao_$(type2)_Typer );\n";
const string ctxput_refcdata =
"  DaoContext_WrapCData( _ctx, (void*)& $(name), dao_$(type2)_Typer );\n";

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
"  $(type) $(name);
  Dao_Get$(qtype)_$(item)( _p[$(index)]->v.list, $(name) );
";
const string qt_dao2cxx_list2 =
"  $(type) $(name);
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

const string dao2cxx2cst = "  const $(type)* $(name)= (const $(type)*) ";
const string dao2cxx_mbs_cst = dao2cxx2cst + "DValue_GetMBString( _p[$(index)] );\n";
const string dao2cxx_wcs_cst = dao2cxx2cst + "DValue_GetWCString( _p[$(index)] );\n";

const string dao2cxx_mbs2 = "  $(type)* $(name)_old = ($(type)*)"
"DValue_GetMBString( _p[$(index)] );\n"
"  size_t $(name)_len = strlen( $(name)_old );\n"
"  $(type)* $(name) = ($(type)*) malloc( $(name)_len + 1 );\n"
"  void* $(name)_p = strncpy( $(name), $(name)_old, $(name)_len );\n";
const string dao2cxx_wcs2 = "  $(type)* $(name)_old = ($(type)*)"
"DValue_GetWCString( _p[$(index)] );\n"
"  size_t $(name)_len = wcslen( $(name)_old ) * sizeof(wchar_t);\n"
"  $(type)* $(name) = ($(type)*) malloc( $(name)_len + sizeof(wchar_t) );\n"
"  void* $(name)_p = memcpy( $(name), $(name)_old, $(name)_len );\n";
const string parset_mbs2 = "  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name) );\n"
"  free( $(name) );\n";
const string parset_wcs2 = "  DString_SetWCS( _p[$(index)]->v.s, (wchar_t*) $(name) );\n"
"  free( $(name) );\n";

const string dao2cxx_qchar = "  QChar $(name)( (int)_p[$(index)]->v.i );\n";
const string dao2cxx_qchar2 =
"  QChar $(name)( (int)_p[$(index)]->v.i )\
  QChar *$(name) = & _$(name);\
";
const string parset_qchar = "  _p[$(index)]->v.i = $(name).digitValue();\n";
const string parset_qchar2 = "  _p[$(index)]->v.i = $(name)->digitValue();\n";
const string ctxput_qchar = "  DaoContext_PutInteger( _ctx, $(name).digitValue() );\n";
const string ctxput_qchar2 = "  DaoContext_PutInteger( _ctx, $(name)->digitValue() );\n";

const string dao2cxx_qbytearray =
"  char *_mbs$(index) = DValue_GetMBString( _p[$(index)] );\
  QByteArray $(name)( _mbs$(index) );\
";
const string dao2cxx_qbytearray2 =
"  char *_mbs$(index) = DValue_GetMBString( _p[$(index)] );\
  QByteArray _$(name)( _mbs$(index) );\
  QByteArray *$(name) = & _$(name);\
";
const string parset_qbytearray =
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name).data() );\n";
const string parset_qbytearray2 =
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name)->data() );\n";
const string ctxput_qbytearray = 
"  DaoContext_PutMBString( _ctx, $(name).data() );\n";

const string dao2cxx_qstring =
"  char *_mbs$(index) = DValue_GetMBString( _p[$(index)] );\
  QString $(name)( _mbs$(index) );\
";
const string dao2cxx_qstring2 =
"  char *_mbs$(index) = DValue_GetMBString( _p[$(index)] );\
  QString _$(name)( _mbs$(index) );\
  QString *$(name) = & _$(name);\
";
const string parset_qstring =
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name).toLocal8Bit().data() );\n";
const string parset_qstring2 =
"  DString_SetMBS( _p[$(index)]->v.s, (char*) $(name)->toLocal8Bit().data() );\n";
const string ctxput_qstring = 
"  DaoContext_PutMBString( _ctx, $(name).toLocal8Bit().data() );\n";

const string getres_i = "  if( _res.t == DAO_INTEGER ) $(name)= ($(type)) ";
const string getres_f = "  if( _res.t == DAO_FLOAT ) $(name)= ($(type)) ";
const string getres_d = "  if( _res.t == DAO_DOUBLE ) $(name)= ($(type)) ";
const string getres_s = "  if( _res.t == DAO_STRING ) $(name)= ($(type)*) ";
const string getres_a = "  if( _res.t == DAO_ARRAY )\n    $(name)= ($(type)*) ";
const string getres_p = "  if( _res.t == DAO_CDATA ) $(name)= ($(type)) ";
const string getres_io = "  if( _res.t == DAO_STREAM ) $(name)= ($(type)) ";

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
"  if( _res.t == DAO_OBJECT && (_cd = DaoObject_MapCData( _res.v.object, dao_$(type2)_Typer ) ) ){\
    _res.t = DAO_CDATA;\
    _res.v.cdata = _cd;\
  }\
  if( _res.t == DAO_CDATA && DaoCData_IsType( _res.v.cdata, dao_$(type2)_Typer ) ){\
";

const string getres_user = getres_cdata +
"    $(name) = ($(namespace)$(type)*) DValue_CastCData( &_res, dao_$(type2)_Typer );\n  }\n";

const string getres_user2 = getres_cdata +
"    $(name) = *($(namespace)$(type)*) DValue_CastCData( &_res, dao_$(type2)_Typer );\n  }\n";


const string getitem_int = ctxput + "Integer( _ctx, (int) self->$(name)[_p[1]->v.i] );\n";
const string getitem_float = ctxput + "Float( _ctx, (float) self->$(name)[_p[1]->v.i] );\n";
const string getitem_double = ctxput + "Double( _ctx, (double) self->$(name)[_p[1]->v.i] );\n";

const string setitem_int = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n"
"  self->$(name)[_p[1]->v.i] = _p[2]->v.i;\n";
const string setitem_float = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n"
"  self->$(name)[_p[1]->v.i] = _p[2]->v.f;\n";
const string setitem_double = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n"
"  self->$(name)[_p[1]->v.i] = _p[2]->v.d;\n";

const string getitem_int2 = ctxput + "Integer( _ctx, (int) (*self)[_p[1]->v.i] );\n";
const string getitem_float2 = ctxput + "Float( _ctx, (float) (*self)[_p[1]->v.i] );\n";
const string getitem_double2 = ctxput + "Double( _ctx, (double) (*self)[_p[1]->v.i] );\n";

const string setitem_int2 = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n"
"  (*self)[_p[1]->v.i] = _p[2]->v.i;\n";
const string setitem_float2 = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n"
"  (*self)[_p[1]->v.i] = _p[2]->v.f;\n";
const string setitem_double2 = 
"  if( _p[1]->v.i < 0 || _p[1]->v.i >= $(size) ) return;\n"
"  (*self)[_p[1]->v.i] = _p[2]->v.d;\n";

const string setter_int = "  self->$(name) = ($(type)) _p[1]->v.i;\n";
const string setter_float = "  self->$(name) = ($(type)) _p[1]->v.f;\n";
const string setter_double = "  self->$(name) = ($(type)) _p[1]->v.d;\n";
const string setter_string = // XXX array?
"  int size = DString_Size( _p[1]->v.s );\n"
"  if( size > $(size) ) size = $(size);\n"
"  memmove( self->$(name), DValue_GetMBString( _p[1] ), size );\n";
const string setter_shorts =
"  int size = DaoArray_Size( _p[1]->v.array );\n"
"  if( size > $(size) ) size = $(size);\n"
"  memmove( self->$(name), DaoArray_ToShort( _p[1]->v.array ), size*sizeof(short) );\n";
const string setter_ints =
"  int size = DaoArray_Size( _p[1]->v.array );\n"
"  if( size > $(size) ) size = $(size);\n"
"  memmove( self->$(name), DaoArray_ToInt( _p[1]->v.array ), size*sizeof(int) );\n";
const string setter_floats =
"  int size = DaoArray_Size( _p[1]->v.array );\n"
"  if( size > $(size) ) size = $(size);\n"
"  memmove( self->$(name), DaoArray_ToFloat( _p[1]->v.array ), size*sizeof(float) );\n";
const string setter_doubles =
"  int size = DaoArray_Size( _p[1]->v.array );\n"
"  if( size > $(size) ) size = $(size);\n"
"  memmove( self->$(name), DaoArray_ToDouble( _p[1]->v.array ), size*sizeof(double) );\n";

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );

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

	void Generate( CDaoVariable *var, map<string,string> & kvmap, int offset = 0 );
};
void CDaoVarTemplates::Generate( CDaoVariable *var, map<string,string> & kvmap, int offset )
{
	char sindex[50];
	string cdft;

	sprintf( sindex, "%i", var->index - offset );
	if( var->cxxdefault.size() ) cdft = " =" + var->cxxdefault;

	kvmap[ "type" ] = var->cxxtype;
	kvmap[ "type2" ] = var->cxxtype;
	kvmap[ "name" ] = var->name;
	kvmap[ "namespace" ] = "";
	kvmap[ "namespace2" ] = "";
	kvmap[ "index" ] = sindex;
	kvmap[ "default" ] = cdft;
	var->daopar = cdao_string_fill( daopar, kvmap );
	var->dao2cxx = cdao_string_fill( dao2cxx, kvmap );
	var->parset = cdao_string_fill( parset, kvmap );
	var->cxx2dao = cdao_string_fill( cxx2dao, kvmap );
	if( var->index == VAR_INDEX_RETURN ){
		var->ctxput = cdao_string_fill( ctxput, kvmap );
		var->getres = cdao_string_fill( getres, kvmap );
	}else if( var->index == VAR_INDEX_FIELD ){
		if( not var->varDecl->getType().isConstQualified() ){
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

CDaoVariable::CDaoVariable( CDaoModule *mod, VarDecl *decl, int id )
{
	index = id;
	module = mod;
	varDecl = NULL;
	hasNullableHint = false;
	hasArrayHint = false;
	unsupport = true;
	SetDeclaration( decl );
}
void CDaoVariable::SetDeclaration( VarDecl *decl )
{
	varDecl = decl;
	if( decl == NULL ) return;
	name = decl->getName().str();
	//outs() << "variable: " << name << "\n";
}
void CDaoVariable::SetHints( const string & hints )
{
}
int CDaoVariable::Generate( int offset )
{
	int retcode = Generate2( offset );
	unsupport = retcode != 0;
	return retcode;
}
int CDaoVariable::Generate2( int offset )
{
	const Expr *e = varDecl->getAnyInitializer();
	if( e ){
		Preprocessor & pp = module->compiler->getPreprocessor();
		SourceManager & sm = module->compiler->getSourceManager();
		SourceLocation start = sm.getInstantiationLoc( e->getLocStart() );
		SourceLocation end = sm.getInstantiationLoc( e->getLocEnd() );
		SourceLocation start2 = sm.getSpellingLoc( e->getLocStart() );
		SourceLocation end2 = sm.getSpellingLoc( e->getLocEnd() );
		const char *p = sm.getCharacterData( start );
		const char *p2 = sm.getCharacterData( start2 );
		const char *q = sm.getCharacterData( pp.getLocForEndOfToken( end ) );
		const char *q2 = sm.getCharacterData( pp.getLocForEndOfToken( end2 ) );
		for(; p!=q; p++) cxxdefault += *p;
		for(; p2!=q2; p2++) cxxdefault2 += *p2;
	}
#if 0
	if( ParmVarDecl *par = dyn_cast<ParmVarDecl>( decl ) ){
		SourceRange range = par->getDefaultArgRange();
		outs() << (range.getBegin() == range.getEnd()) << "\n";
	}
#endif
	outs() << cxxdefault << "  " << cxxdefault2 << "\n";
	QualType qtype = varDecl->getTypeSourceInfo()->getType();
	const Type *type = qtype.getTypePtr();
	string ctypename = qtype.getAsString();
	cxxtype = ctypename;
	cxxtype2 = "";
	cxxcall = name;
	//outs() << type->isPointerType() << " is pointer type\n";
	//outs() << type->isArrayType() << " is array type\n";
	//outs() << type->isConstantArrayType() << " is constant array type\n";
	if( type->isBuiltinType() ) return Generate( (const BuiltinType*)type, offset );
	if( type->isPointerType() and not hasArrayHint )
		return Generate( (const PointerType*)type, offset );
	if( type->isArrayType() ) return Generate( (const ArrayType*)type, offset );
	return 1;
}
int CDaoVariable::Generate( const BuiltinType *type, int offset )
{
	CDaoVarTemplates tpl;
	//vdefault2 = vdefault;
	if( type->isArithmeticType() ){
		daotype = "int";
		cxxpar = cxxtype + " " + name;
		cxxpar_enum_virt = cxxpar;
#if 0
		if( isClassEnum ){
			cxxpar_enum_virt = 'int ' + name;
			cxxtype2 = 'int';
		}
#endif
		tpl.daopar = daopar_int;
		tpl.dao2cxx = dao2cxx_int;
		tpl.cxx2dao = cxx2dao_int;
		tpl.ctxput = ctxput_int;
		tpl.getres = getres_int;
		tpl.setter = setter_int;
		//if( vdefault.pfind( '= %s* \'.\'' ) ) vdefault += '[0]';
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
			tpl.daopar = daopar_float;
			tpl.dao2cxx = dao2cxx_float;
			tpl.cxx2dao = cxx2dao_float;
			tpl.ctxput = ctxput_float;
			tpl.getres = getres_float;
			tpl.setter = setter_float;
			//if( vdefault.pfind( '= %s* (0|NULL|0f|0%.0f)' ) ) vdefault = '=0.0';
			break;
		case BuiltinType::Double :
		case BuiltinType::LongDouble : // FIXME
			daotype = "double";
			tpl.daopar = daopar_double;
			tpl.dao2cxx = dao2cxx_double;
			tpl.cxx2dao = cxx2dao_double;
			tpl.ctxput = ctxput_double;
			tpl.getres = getres_double;
			tpl.setter = setter_double;
			break;
		default : break;
		}
	}
	map<string,string> kvmap;
	tpl.Generate( this, kvmap, offset );
	return 0;
}
int CDaoVariable::Generate( const PointerType *type, int offset )
{
	QualType qtype2 = type->getPointeeType();
	const Type *type2 = qtype2.getTypePtr();
	string ctypename2 = qtype2.getAsString();

	CXXRecordDecl *decl = type2->getAsCXXRecordDecl();
	if( decl ) outs() << (void*) decl << " " << (void*) decl->getDefinition() << "\n";

	CDaoVarTemplates tpl;
	//vdefault2 = vdefault;
	if( type2->isBuiltinType() and type2->isArithmeticType() ){
		const BuiltinType *type3 = (const BuiltinType*) type2;
		daotype = "int";
		cxxtype = ctypename2;
		cxxpar = cxxtype + " " + name;
		cxxpar_enum_virt = cxxpar;
		cxxcall = "&" + name;
#if 0
		if( isClassEnum ){
			cxxpar_enum_virt = 'int ' + name;
			cxxtype2 = 'int';
		}
#endif
		tpl.daopar = daopar_int;
		tpl.dao2cxx = dao2cxx_int;
		tpl.cxx2dao = cxx2dao_int;
		tpl.ctxput = ctxput_int;
		tpl.getres = getres_int;
		tpl.setter = setter_int;
		//if( vdefault.pfind( '= %s* \'.\'' ) ) vdefault += '[0]';
		switch( type3->getKind() ){
		case BuiltinType::Char_U :
		case BuiltinType::UChar :
		case BuiltinType::Char_S :
		case BuiltinType::SChar :
			daotype = "string";
			tpl.daopar = daopar_string;
			tpl.dao2cxx = dao2cxx_mbs;
			tpl.cxx2dao = cxx2dao_mbs;
			tpl.ctxput = ctxput_mbs;
			tpl.parset = parset_mbs;
			tpl.getres = getres_mbs;
			//if( vdefault.pfind( '= %s* (0|NULL)' ) ) vdefault = '=\\\'\\\'';
			break;
		case BuiltinType::WChar_U :
		case BuiltinType::WChar_S :
		case BuiltinType::Char16 :
		case BuiltinType::Char32 :
			daotype = "string";
			tpl.daopar = daopar_string;
			tpl.dao2cxx = dao2cxx_wcs;
			tpl.cxx2dao = cxx2dao_wcs;
			tpl.ctxput = ctxput_wcs;
			tpl.parset = parset_wcs;
			tpl.getres = getres_wcs;
			//if( vdefault.pfind( '= %s* (0|NULL)' ) ) vdefault = '=\\\"\\\"';
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
			tpl.daopar = daopar_int;
			tpl.dao2cxx = dao2cxx_int;
			tpl.cxx2dao = cxx2dao_int;
			tpl.ctxput = ctxput_int;
			tpl.parset = parset_int;
			tpl.getres = getres_int;
#if 0
			if( refer == '*' ){
				tpl.ctxput = ctxput_ints;
				tpl.getres = getres_ints;
				tpl.cxx2dao = cxx2dao_int2;
			}
#endif
			//if( vdefault.pfind( '= %s* (0|NULL)' ) ) vdefault = '=0';
			break;
		case BuiltinType::Float :
			daotype = "float";
			tpl.daopar = daopar_float;
			tpl.dao2cxx = dao2cxx_float;
			tpl.cxx2dao = cxx2dao_float;
			tpl.ctxput = ctxput_float;
			tpl.parset = parset_float;
			tpl.getres = getres_float;
#if 0
			if( refer == '*' ){
				tpl.ctxput = ctxput_floats;
				tpl.getres = getres_floats;
				tpl.cxx2dao = cxx2dao_float2;
			}
#endif
			//if( vdefault.pfind( '= %s* (0|NULL|0f|0%.0f)' ) ) vdefault = '=0.0';
			break;
		case BuiltinType::Double :
		case BuiltinType::LongDouble : // FIXME
			daotype = "double";
			tpl.daopar = daopar_double;
			tpl.dao2cxx = dao2cxx_double;
			tpl.cxx2dao = cxx2dao_double;
			tpl.ctxput = ctxput_double;
			tpl.parset = parset_double;
			tpl.getres = getres_double;
#if 0
			if( refer == '*' ){
				tpl.ctxput = ctxput_doubles;
				tpl.getres = getres_doubles;
				tpl.cxx2dao = cxx2dao_double2;
			}
#endif
			//if( vdefault.pfind( '= %s* (0|NULL|0f|0%.0f)' ) ) vdefault = '=0.0';
			break;
		default : break;
		}
	}
	map<string,string> kvmap;
	tpl.Generate( this, kvmap, offset );
	return 0;
}
int CDaoVariable::Generate( const ArrayType *type, int offset )
{
	QualType qtype2 = type->getElementType();
	const Type *type2 = qtype2.getTypePtr();
	string ctypename2 = qtype2.getAsString();

	CDaoVarTemplates tpl;
	//vdefault2 = vdefault;
	if( type2->isBuiltinType() and type2->isArithmeticType() ){
		const BuiltinType *type3 = (const BuiltinType*) type2;
		daotype = "array<int>";
		dao_itemtype = "int";
		cxxtype = ctypename2;
		cxxpar = cxxtype + " " + name;
		cxxpar_enum_virt = cxxpar;
		cxxcall = name;
#if 0
		if( isClassEnum ){
			cxxpar_enum_virt = 'int ' + name;
			cxxtype2 = 'int';
		}
#endif
		tpl.daopar = daopar_ints;
		tpl.ctxput = ctxput_ints;
		tpl.parset = parset_ints;
		tpl.getres = getres_ints;
		tpl.setter = setter_ints;
		tpl.get_item = name == "this" ? getitem_int2 : getitem_int;
		tpl.set_item = name == "this" ? setitem_int2 : setitem_int;
		//if( vdefault.pfind( '= %s* \'.\'' ) ) vdefault += '[0]';
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
	}
	map<string,string> kvmap;
	if( type->isConstantArrayType() ){
		ConstantArrayType *at = (ConstantArrayType*) type;
		kvmap[ "size" ] = at->getSize().toString( 10, false );
	}
	tpl.Generate( this, kvmap, offset );
	return 0;
}
