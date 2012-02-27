
#include <llvm/ADT/StringExtras.h>
#include <clang/AST/Type.h>
#include <clang/AST/TypeLoc.h>
#include <clang/AST/Expr.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Lex/Preprocessor.h>

#include "cdaoVariable.hpp"
#include "cdaoModule.hpp"

const string daopar_int = "$(name) :int$(default)";
const string daopar_float = "$(name) :float$(default)";
const string daopar_double = "$(name) :double$(default)";
const string daopar_complex = "$(name) :complex$(default)";
const string daopar_string = "$(name) :$(dao)string$(default)";
const string daopar_ints = "$(name) :$(dao)array<int>";
const string daopar_floats = "$(name) :$(dao)array<float>";
const string daopar_doubles = "$(name) :$(dao)array<double>";
const string daopar_complexs = "$(name) :$(dao)array<complex>$(default)";
const string daopar_buffer = "$(name) :cdata$(default)";
const string daopar_stream = "$(name) :stream$(default)";
const string daopar_user = "$(name) :$(daotype)$(default)";
const string daopar_userdata = "$(name) :any$(default)"; // for callback data
const string daopar_callback = "$(name) :any"; // for callback, no precise type yet! XXX

const string dao2cxx = "  $(cxxtype) $(name) = ($(cxxtype)) ";
const string dao2cxx2 = "  $(cxxtype)* $(name) = ($(cxxtype)*) ";
const string dao2cxx3 = "  $(cxxtype)* $(name) = ($(cxxtype)*) ";
const string dao2cxx4 = "  $(cxxtype) (*$(name))[$(size2)] = ($(cxxtype)(*)[$(size2)]) ";

const string dao2cxx_char = dao2cxx + "DaoValue_TryGetMBString( _p[$(index)] )[0];\n";
const string dao2cxx_wchar = dao2cxx + "DaoValue_TryGetWCString( _p[$(index)] )[0];\n";
const string dao2cxx_int  = dao2cxx + "DaoValue_TryGetInteger( _p[$(index)] );\n";
const string dao2cxx_float = dao2cxx + "DaoValue_TryGetFloat( _p[$(index)] );\n";
const string dao2cxx_double = dao2cxx + "DaoValue_TryGetDouble( _p[$(index)] );\n";
const string dao2cxx_complex = dao2cxx + "DaoValue_TryGetComplex( _p[$(index)] );\n";
const string dao2cxx_mbs = dao2cxx2 + "DaoValue_TryGetMBString( _p[$(index)] );\n";
const string dao2cxx_wcs = dao2cxx2 + "DaoValue_TryGetWCString( _p[$(index)] );\n";
const string dao2cxx_bytes = dao2cxx2 + "DaoArray_ToSByte( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_ubytes = dao2cxx2 + "DaoArray_ToUByte( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_shorts = dao2cxx2 + "DaoArray_ToSShort( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_ushorts = dao2cxx2 + "DaoArray_ToUShort( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_ints = dao2cxx2 + "DaoArray_ToSInt( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_uints = dao2cxx2 + "DaoArray_ToUInt( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_floats = dao2cxx2 + "DaoArray_ToFloat( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_doubles = dao2cxx2 + "DaoArray_ToDouble( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_complexs8 = dao2cxx2 + "(complex8*) DaoArray_ToFloat( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_complexs16 = dao2cxx2 + "(complex16*) DaoArray_ToDouble( (DaoArray*)_p[$(index)] );\n";

const string dao2cxx_bmat = dao2cxx3 + "DaoArray_GetMatrixB( (DaoArray*)_p[$(index)], $(size) );\n";
const string dao2cxx_smat = dao2cxx3 + "DaoArray_GetMatrixS( (DaoArray*)_p[$(index)], $(size) );\n";
const string dao2cxx_imat = dao2cxx3 + "DaoArray_GetMatrixI( (DaoArray*)_p[$(index)], $(size) );\n";
const string dao2cxx_fmat = dao2cxx3 + "DaoArray_GetMatrixF( (DaoArray*)_p[$(index)], $(size) );\n";
const string dao2cxx_dmat = dao2cxx3 + "DaoArray_GetMatrixD( (DaoArray*)_p[$(index)], $(size) );\n";
const string dao2cxx_c8mat = dao2cxx3 + "(complex8*) DaoArray_GetMatrixF( (DaoArray*)_p[$(index)], $(size) );\n";
const string dao2cxx_c16mat = dao2cxx3 + "(complex16*) DaoArray_GetMatrixD( (DaoArray*)_p[$(index)], $(size) );\n";

const string dao2cxx_ubmat = dao2cxx_bmat; // TODO:
const string dao2cxx_usmat = dao2cxx_smat;
const string dao2cxx_uimat = dao2cxx_imat;

const string dao2cxx_bmat2 = dao2cxx4 + "DaoArray_ToSByte( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_smat2 = dao2cxx4 + "DaoArray_ToSShort( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_imat2 = dao2cxx4 + "DaoArray_ToSInt( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_fmat2 = dao2cxx4 + "DaoArray_ToFloat( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_dmat2 = dao2cxx4 + "DaoArray_ToDouble( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_c8mat2 = dao2cxx4 + "(complex8*) DaoArray_ToFloat( (DaoArray*)_p[$(index)] );\n";
const string dao2cxx_c16mat2 = dao2cxx4 + "(complex16*) DaoArray_ToDouble( (DaoArray*)_p[$(index)] );\n";

const string dao2cxx_ubmat2 = dao2cxx_bmat2; // TODO:
const string dao2cxx_usmat2 = dao2cxx_smat2;
const string dao2cxx_uimat2 = dao2cxx_imat2;


const string dao2cxx_stream = dao2cxx2 + "DaoStream_GetFile( (DaoStream*)_p[$(index)] );\n";

const string dao2cxx_void = 
"  $(cxxtype)* $(name) = ($(cxxtype)*) DaoValue_TryGetCdata( _p[$(index)] );\n";

const string dao2cxx_void2 =
"  $(cxxtype) $(name) = ($(cxxtype)) DaoValue_TryGetCdata( _p[$(index)] );\n";

const string dao2cxx_user =
 "  $(cxxtype)* $(name) = ($(cxxtype)*) DaoValue_TryCastCdata( _p[$(index)], dao_type_$(typer) );\n";

const string dao2cxx_user2 = 
"  $(cxxtype)* $(name) = ($(cxxtype)*) DaoValue_TryCastCdata( _p[$(index)], dao_type_$(typer) );\n";

const string dao2cxx_user3 =
"  $(cxxtype)** $(name) = ($(cxxtype)**) DaoValue_TryGetCdata2( _p[$(index)] );\n";

const string dao2cxx_user4 = 
"  $(cxxtype)* $(name) = ($(cxxtype)*) DaoValue_TryGetCdata2( _p[$(index)] );\n";

const string dao2cxx_callback =
"  DaoTuple *_$(name) = (DaoTuple*) _p[$(index)];\n\
  $(cxxtype) $(name) = Dao_$(callback);\n";

const string dao2cxx_userdata = "  DaoTuple *$(name) = (DaoTuple*) _p[$(index)];\n";


const string cxx2dao = "  DaoFactory_New";

const string cxx2dao_int = cxx2dao + "Integer( _fac, (int) $(name) );\n";
const string cxx2dao_float = cxx2dao + "Float(  _fac,(float) $(name) );\n";
const string cxx2dao_double = cxx2dao + "Double( _fac, (double) $(name) );\n";
const string cxx2dao_int2 = cxx2dao + "Integer( _fac, (int) *$(name) );\n";
const string cxx2dao_float2 = cxx2dao + "Float( _fac, (float) *$(name) );\n";
const string cxx2dao_double2 = cxx2dao + "Double( _fac, (double) *$(name) );\n";
const string cxx2dao_mbs = cxx2dao+"MBString( _fac, (char*) $(name), strlen( (char*)$(name) ) );\n"; // XXX for char**
const string cxx2dao_wcs = cxx2dao + "WCString( _fac, (wchar_t*) $(name), wcslen( (wchar_t*)$(name) ) );\n"; // XXX for wchar_t**
const string cxx2dao_bytes = cxx2dao + "VectorSB( _fac, (signed char*) $(name), $(size) );\n";
const string cxx2dao_ubytes = cxx2dao + "VectorUB( _fac, (unsigned char*) $(name), $(size) );\n";
const string cxx2dao_shorts = cxx2dao + "VectorSS( _fac, (signed short*) $(name), $(size) );\n";
const string cxx2dao_ushorts = cxx2dao + "VectorUS( _fac, (unsigned short*) $(name), $(size) );\n";
const string cxx2dao_ints = cxx2dao + "VectorSI( _fac, (signed int*) $(name), $(size) );\n";
const string cxx2dao_uints = cxx2dao + "VectorUI( _fac, (unsigned int*) $(name), $(size) );\n";
const string cxx2dao_floats = cxx2dao + "VectorF( _fac, (float*) $(name), $(size) );\n";
const string cxx2dao_doubles = cxx2dao + "VectorD( _fac, (double*) $(name), $(size) );\n";

const string cxx2dao_bmat = cxx2dao + "MatrixSB( _fac, (signed char**) $(name), $(size), $(size2) );\n";
const string cxx2dao_ubmat = cxx2dao + "MatrixUB( _fac, (unsigned char**) $(name), $(size), $(size2) );\n";
const string cxx2dao_smat = cxx2dao + "MatrixSS( _fac, (signed short**) $(name), $(size), $(size2) );\n";
const string cxx2dao_usmat = cxx2dao + "MatrixUS( _fac, (unsigned short**) $(name), $(size), $(size2) );\n";
const string cxx2dao_imat = cxx2dao + "MatrixSI( _fac, (signed int**) $(name), $(size), $(size2) );\n";
const string cxx2dao_uimat = cxx2dao + "MatrixUI( _fac, (unsigned int**) $(name), $(size), $(size2) );\n";
const string cxx2dao_fmat = cxx2dao + "MatrixF( _fac, (float**) $(name), $(size), $(size2) );\n";
const string cxx2dao_dmat = cxx2dao + "MatrixD( _fac, (double**) $(name), $(size), $(size2) );\n";

const string cxx2dao_bmat2 = cxx2dao_bmat; // XXX
const string cxx2dao_ubmat2 = cxx2dao_ubmat;
const string cxx2dao_smat2 = cxx2dao_smat;
const string cxx2dao_usmat2 = cxx2dao_usmat;
const string cxx2dao_imat2 = cxx2dao_imat;
const string cxx2dao_uimat2 = cxx2dao_uimat;
const string cxx2dao_fmat2 = cxx2dao_fmat;
const string cxx2dao_dmat2 = cxx2dao_dmat;

const string cxx2dao_stream = cxx2dao + "Stream( _fac, (FILE*) $(refer) );\n";
const string cxx2dao_voidp = "  DaoFactory_NewCdata( _fac, NULL, (void*) $(refer), 0 );\n";
const string cxx2dao_user = "  DaoFactory_NewCdata( _fac, dao_type_$(typer), (void*) $(refer), 0 );\n";

const string cxx2dao_userdata = "  DaoFactory_CacheValue( _fac, $(name) );\n";

const string cxx2dao_qchar = cxx2dao+"Integer( _fac, $(name).digitValue() );\n";
const string cxx2dao_qchar2 = cxx2dao+"Integer( _fac, $(name)->digitValue() );\n";
const string cxx2dao_qbytearray = cxx2dao+"MBString( _fac, (char*) $(name).data(), 0 );\n";
const string cxx2dao_qstring = cxx2dao+"MBString( _fac, (char*) $(name).toLocal8Bit().data(), 0 );\n";

const string ctxput = "  DaoProcess_Put";

const string ctxput_int = ctxput + "Integer( _proc, (int) $(name) );\n";
const string ctxput_float = ctxput + "Float( _proc, (float) $(name) );\n";
const string ctxput_double = ctxput + "Double( _proc, (double) $(name) );\n";
const string ctxput_mbs = ctxput + "MBString( _proc, (char*) $(name) );\n";
const string ctxput_wcs = ctxput + "WCString( _proc, (wchar_t*) $(name) );\n";
const string ctxput_bytes = ctxput + "Bytes( _proc, (char*) $(name), $(size) );\n"; // XXX array?
const string ctxput_shorts = ctxput + "ArrayShort( _proc, (short*) $(name), $(size) );\n";
const string ctxput_ints = ctxput + "ArrayInteger( _proc, (daoint*) $(name), $(size) );\n"; // XXX
const string ctxput_floats = ctxput + "ArrayFloat( _proc, (float*) $(name), $(size) );\n";
const string ctxput_doubles = ctxput + "ArrayDouble( _proc, (double*) $(name), $(size) );\n";

const string ctxput_stream = ctxput + "File( _proc, (FILE*) $(name) );\n"; //XXX PutFile
const string ctxput_voidp = ctxput + "Cdata( _proc, (void*) $(name), NULL );\n";
const string ctxput_user = "  DaoProcess_WrapCdata( _proc, (void*) $(name), dao_type_$(typer) );\n";
const string qt_procput = "  Dao_$(typer)_InitSS( ($(cxxtype)*) $(name) );\n";
const string qt_put_qobject =
"  DaoValue *dbase = DaoQt_Get_Wrapper( $(name) );\n\
  if( dbase ){\n\
    DaoProcess_PutValue( _proc, dbase );\n\
  }else{\n\
    Dao_$(typer)_InitSS( ($(cxxtype)*) $(name) );\n\
    DaoProcess_WrapCdata( _proc, (void*) $(name), dao_type_$(typer) );\n\
  }\n\
";

const string ctxput_copycdata =
"  DaoProcess_CopyCdata( _proc, (void*)&$(name), sizeof($(cxxtype)), dao_type_$(typer) );\n";
const string ctxput_newcdata =
"  DaoProcess_PutCdata( _proc, (void*)new $(cxxtype)( $(name) ), dao_type_$(typer) );\n";
const string ctxput_refcdata =
"  DaoProcess_WrapCdata( _proc, (void*)&$(name), dao_type_$(typer) );\n";

#if 0
const string qt_qlist_decl = 
"typedef $(qtype)<$(item)> $(qtype)_$(item);
void Dao_Put$(qtype)_$(item)( DaoProcess *ctx, const $(qtype)_$(item) & qlist );
void Dao_Get$(qtype)_$(item)( DaoList *dlist, $(qtype)_$(item) & qlist );
";
const string qt_qlist_decl2 = 
"typedef $(qtype)<$(item)*> $(qtype)P_$(item);
void Dao_Put$(qtype)P_$(item)( DaoProcess *ctx, const $(qtype)P_$(item) & qlist );
void Dao_Get$(qtype)P_$(item)( DaoList *dlist, $(qtype)P_$(item) & qlist );
";
const string qt_daolist_func =
"void Dao_Put$(qtype)_$(item)( DaoProcess *ctx, const $(qtype)_$(item) & qlist )
{
	DaoList *dlist = DaoProcess_PutList( ctx );
	DaoValue it = { DAO_CDATA, 0, 0, 0, {0} };
	int i, m = qlist.size();
	for(i=0; i<m; i++){
		it.v.cdata = DaoCdata_New( dao_$(item)_Typer, new $(item)( qlist[i] ) );
		DaoList_PushBack( dlist, it );
	}
}
void Dao_Get$(qtype)_$(item)( DaoList *dlist, $(qtype)_$(item) & qlist )
{
	int i, m = DaoList_Size( dlist );
	for(i=0; i<m; i++){
		DaoValue it = DaoList_GetItem( dlist, i );
		if( it.t != DAO_CDATA ) continue;
		if( ! DaoCdata_IsType( it.v.cdata, dao_$(item)_Typer ) ) continue;
		qlist.append( *($(item)*) DaoValue_TryCastCdata( & it, dao_$(item)_Typer ) );
	}
}
";
const string qt_daolist_func_virt =
"void Dao_Put$(qtype)_$(item)( DaoProcess *ctx, const $(qtype)_$(item) & qlist )
{
	DaoList *dlist = DaoProcess_PutList( ctx );
	DaoValue it = { DAO_CDATA, 0, 0, 0, {0} };
	int i, m = qlist.size();
	for(i=0; i<m; i++){
		it.v.cdata = DaoCdata_New( dao_$(item)_Typer, new $(item)( qlist[i] ) );
		DaoList_PushBack( dlist, it );
	}
}
void Dao_Get$(qtype)_$(item)( DaoList *dlist, $(qtype)_$(item) & qlist )
{
	int i, m = DaoList_Size( dlist );
	for(i=0; i<m; i++){
		DaoValue it = DaoList_GetItem( dlist, i );
		if( it.t != DAO_CDATA ) continue;
		if( ! DaoCdata_IsType( it.v.cdata, dao_$(item)_Typer ) ) continue;
		qlist.append( * ($(item)*) DaoValue_TryCastCdata( & it, dao_$(item)_Typer ) );
	}
}
";
const string qt_daolist_func2 =
"void Dao_Put$(qtype)P_$(item)( DaoProcess *ctx, const $(qtype)P_$(item) & qlist )
{
	DaoList *dlist = DaoProcess_PutList( ctx );
	DaoValue it = { DAO_CDATA, 0, 0, 0, {0} };
	int i, m = qlist.size();
	for(i=0; i<m; i++){
		it.v.cdata = DaoCdata_Wrap( dao_$(item)_Typer, qlist[i] );
		DaoList_PushBack( dlist, it );
	}
}
void Dao_Get$(qtype)P_$(item)( DaoList *dlist, $(qtype)P_$(item) & qlist )
{
	int i, m = DaoList_Size( dlist );
	for(i=0; i<m; i++){
		DaoValue it = DaoList_GetItem( dlist, i );
		if( it.t != DAO_CDATA ) continue;
		if( ! DaoCdata_IsType( it.v.cdata, dao_$(item)_Typer ) ) continue;
		qlist.append( ($(item)*) DaoValue_TryCastCdata( & it, dao_$(item)_Typer ) );
	}
}
";
const string qt_daolist_func_virt2 =
"void Dao_Put$(qtype)P_$(item)( DaoProcess *ctx, const $(qtype)P_$(item) & qlist )
{
	DaoList *dlist = DaoProcess_PutList( ctx );
	DaoValue it = { DAO_CDATA, 0, 0, 0, {0} };
	int i, m = qlist.size();
	for(i=0; i<m; i++){
		it.v.cdata = DaoCdata_Wrap( dao_$(item)_Typer, qlist[i] );
		DaoList_PushBack( dlist, it );
	}
}
void Dao_Get$(qtype)P_$(item)( DaoList *dlist, $(qtype)P_$(item) & qlist )
{
	int i, m = DaoList_Size( dlist );
	for(i=0; i<m; i++){
		DaoValue it = DaoList_GetItem( dlist, i );
		if( it.t != DAO_CDATA ) continue;
		if( ! DaoCdata_IsType( it.v.cdata, dao_$(item)_Typer ) ) continue;
		qlist.append( ($(item)*) DaoValue_TryCastCdata( & it, dao_$(item)_Typer ) );
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
const string qt_daolist_codes = "  Dao_Put$(qtype)_$(item)( _proc, $(name) );\n";
const string qt_daolist_codes2 = "  Dao_Put$(qtype)P_$(item)( _proc, $(name) );\n";
#endif

const string parset_int = "  DaoInteger_Set( (DaoInteger*)_p[$(index)], (int)$(name) );\n";
const string parset_float = "  DaoFloat_Set( (DaoFloat*)_p[$(index)], (float)$(name) );\n";
const string parset_double = "  DaoDouble_Set( (DaoDouble*)_p[$(index)], (double)$(name) );\n";
const string parset_mbs = "  DaoString_SetMBS( (DaoString*)_p[$(index)], (char*)$(name) );\n";
const string parset_wcs = "  DaoString_SetWCS( (DaoString*)_p[$(index)], (wchar_t*)$(name) );\n";
const string parset_bytes = "  DaoArray_FromSByte( (DaoArray*)_p[$(index)] );\n";
const string parset_ubytes = "  DaoArray_FromUByte( (DaoArray*)_p[$(index)] );\n";
const string parset_shorts = "  DaoArray_FromSShort( (DaoArray*)_p[$(index)] );\n";
const string parset_ushorts = "  DaoArray_FromUShort( (DaoArray*)_p[$(index)] );\n";
const string parset_ints = "  DaoArray_FromSInt( (DaoArray*)_p[$(index)] );\n";
const string parset_uints = "  DaoArray_FromUInt( (DaoArray*)_p[$(index)] );\n";
const string parset_floats = "  DaoArray_FromFloat( (DaoArray*)_p[$(index)] );\n";
const string parset_doubles = "  DaoArray_FromDouble( (DaoArray*)_p[$(index)] );\n";

const string dao2cxx2cst = "  const $(cxxtype)* $(name)= (const $(cxxtype)*) ";
const string dao2cxx_mbs_cst = dao2cxx2cst + "DaoValue_TryGetMBString( _p[$(index)] );\n";
const string dao2cxx_wcs_cst = dao2cxx2cst + "DaoValue_TryGetWCString( _p[$(index)] );\n";

const string dao2cxx_mbs2 = 
"  $(cxxtype)* $(name)_old = ($(cxxtype)*) DaoValue_TryGetMBString( _p[$(index)] );\n\
  size_t $(name)_len = strlen( $(name)_old );\n\
  $(cxxtype)* $(name) = ($(cxxtype)*) malloc( $(name)_len + 1 );\n\
  void* $(name)_p = strncpy( $(name), $(name)_old, $(name)_len );\n";

const string dao2cxx_wcs2 = 
"  $(cxxtype)* $(name)_old = ($(cxxtype)*) DaoValue_TryGetWCString( _p[$(index)] );\n\
  size_t $(name)_len = wcslen( $(name)_old ) * sizeof(wchar_t);\n\
  $(cxxtype)* $(name) = ($(cxxtype)*) malloc( $(name)_len + sizeof(wchar_t) );\n\
  void* $(name)_p = memcpy( $(name), $(name)_old, $(name)_len );\n";

const string parset_mbs2 = 
"  DaoString_SetMBS( (DaoString*)_p[$(index)], (char*)$(name) );\n\
  free( $(name) );\n";

const string parset_wcs2 = 
"  DaoString_SetWCS( (DaoString*)_p[$(index)], (wchar_t*)$(name) );\n\
  free( $(name) );\n";

const string dao2cxx_qchar = "  QChar $(name)( (int)DaoValue_TryGetInteger( _p[$(index)] ) );\n";
const string dao2cxx_qchar2 =
"  QChar $(name)( (int)DaoValue_TryGetInteger( _p[$(index)] ) )\
  QChar *$(name) = & _$(name);\n";

const string parset_qchar = "  DaoInteger_Set( (DaoInteger*)_p[$(index)], $(name).digitValue() );\n";
const string parset_qchar2 = "  DaoInteger_Set( (DaoInteger*)_p[$(index)], $(name)->digitValue() );\n";
const string ctxput_qchar = "  DaoProcess_PutInteger( _proc, $(name).digitValue() );\n";
const string ctxput_qchar2 = "  DaoProcess_PutInteger( _proc, $(name)->digitValue() );\n";

const string dao2cxx_qbytearray =
"  char *_mbs$(index) = DaoValue_TryGetMBString( _p[$(index)] );\n\
  QByteArray $(name)( _mbs$(index) );\n";

const string dao2cxx_qbytearray2 =
"  char *_mbs$(index) = DaoValue_TryGetMBString( _p[$(index)] );\n\
  QByteArray _$(name)( _mbs$(index) );\n\
  QByteArray *$(name) = & _$(name);\n";

const string parset_qbytearray =
"  DaoString_SetMBS( (DaoString*)_p[$(index)], (char*) $(name).data() );\n";
const string parset_qbytearray2 =
"  DaoString_SetMBS( (DaoString*)_p[$(index)], (char*) $(name)->data() );\n";
const string ctxput_qbytearray = 
"  DaoProcess_PutMBString( _proc, $(name).data() );\n";

const string dao2cxx_qstring =
"  char *_mbs$(index) = DaoValue_TryGetMBString( _p[$(index)] );\n\
  QString $(name)( _mbs$(index) );\n";

const string dao2cxx_qstring2 =
"  char *_mbs$(index) = DaoValue_TryGetMBString( _p[$(index)] );\n\
  QString _$(name)( _mbs$(index) );\n\
  QString *$(name) = & _$(name);\n";

const string parset_qstring =
"  DaoString_SetMBS( (DaoString*)_p[$(index)], (char*)$(name).toLocal8Bit().data() );\n";
const string parset_qstring2 =
"  DaoString_SetMBS( (DaoString*)_p[$(index)], (char*)$(name)->toLocal8Bit().data() );\n";
const string ctxput_qstring = 
"  DaoProcess_PutMBString( _proc, $(name).toLocal8Bit().data() );\n";

const string getres_i = "  if(DaoValue_CastInteger(_res)) $(name)=($(cxxtype))";
const string getres_f = "  if(DaoValue_CastFloat(_res)) $(name)=($(cxxtype))";
const string getres_d = "  if(DaoValue_CastDouble(_res)) $(name)=($(cxxtype))";
const string getres_s = "  if(DaoValue_CastString(_res)) $(name)=($(cxxtype)*)";
const string getres_a = "  if(DaoValue_CastArray(_res))\n    $(name)=($(cxxtype)*)";
const string getres_p = "  if(DaoValue_CastCdata(_res)) $(name)=($(cxxtype)) ";
const string getres_io = "  if(DaoValue_CastStream(_res)) $(name)=($(cxxtype))";

const string getres_int  = getres_i + "DaoValue_TryGetInteger(_res);\n";
const string getres_float = getres_f + "DaoValue_TryGetFloat(_res);\n";
const string getres_double = getres_d + "DaoValue_TryGetDouble(_res);\n";
const string getres_mbs = getres_s + "DaoValue_TryGetMBString( _res );\n";
const string getres_wcs = getres_s + "DaoValue_TryGetWCString( _res );\n";
const string getres_bytes = getres_a + "DaoArray_ToByte( (DaoArray*)_res );\n";
const string getres_ubytes = getres_a + "DaoArray_ToUByte( (DaoArray*)_res );\n";
const string getres_shorts = getres_a + "DaoArray_ToShort( (DaoArray*)_res );\n";
const string getres_ushorts = getres_a + "DaoArray_ToUShort( (DaoArray*)_res );\n";
const string getres_ints = getres_a + "DaoArray_ToSInt( (DaoArray*)_res );\n";
const string getres_uints = getres_a + "DaoArray_ToUInt( (DaoArray*)_res );\n";
const string getres_floats = getres_a + "DaoArray_ToFloat( (DaoArray*)_res );\n";
const string getres_doubles = getres_a + "DaoArray_ToDouble( (DaoArray*)_res );\n";
const string getres_stream = getres_io + "DaoStream_GetFile( (DaoArray*)_res );\n";
const string getres_buffer = getres_p + "DaoValue_TryGetCdata( _res );\n";

const string getres_qchar =
"  if(DaoValue_CastInteger(_res)) $(name)= QChar(DaoValue_TryGetInteger(_res));\n";
const string getres_qbytearray =
"  if(DaoValue_CastString(_res)) $(name)= DaoValue_TryGetMBString( _res );\n";
const string getres_qstring =
"  if(DaoValue_CastString(_res)) $(name)= DaoValue_TryGetMBString( _res );\n";

const string getres_cdata = 
"  if( DaoValue_CastObject(_res) ) _res = (DaoValue*)DaoObject_CastCdata( (DaoObject*)_res, dao_type_$(typer) );\n\
  if( DaoValue_CastCdata(_res) && DaoCdata_IsType( (DaoCdata*)_res, dao_type_$(typer) ) ){\n";

const string getres_user = getres_cdata +
"    $(name) = ($(cxxtype)*) DaoValue_TryCastCdata( _res, dao_type_$(typer) );\n  }\n";

const string getres_user2 = getres_cdata +
"    $(name) = *($(cxxtype)*) DaoValue_TryCastCdata( _res, dao_type_$(typer) );\n  }\n";


const string getitem_int = ctxput + "Integer( _proc, (int) self->$(name)[DaoValue_TryGetInteger(_p[1])] );\n";
const string getitem_float = ctxput + "Float( _proc, (float) self->$(name)[DaoValue_TryGetInteger(_p[1])] );\n";
const string getitem_double = ctxput + "Double( _proc, (double) self->$(name)[DaoValue_TryGetInteger(_p[1])] );\n";

const string setitem_int = 
"  if( DaoValue_TryGetInteger(_p[1]) < 0 || DaoValue_TryGetInteger(_p[1]) >= $(size) ) return;\n\
  self->$(name)[DaoValue_TryGetInteger(_p[1])] = DaoValue_TryGetInteger(_p[2]);\n";
const string setitem_float = 
"  if( DaoValue_TryGetInteger(_p[1]) < 0 || DaoValue_TryGetInteger(_p[1]) >= $(size) ) return;\n\
  self->$(name)[DaoValue_TryGetInteger(_p[1])] = DaoValue_TryGetFloat(_p[2]);\n";
const string setitem_double = 
"  if( DaoValue_TryGetInteger(_p[1]) < 0 || DaoValue_TryGetInteger(_p[1]) >= $(size) ) return;\n\
  self->$(name)[DaoValue_TryGetInteger(_p[1])] = DaoValue_TryGetDouble(_p[2]);\n";

const string getitem_int2 = ctxput + "Integer( _proc, (int) (*self)[DaoValue_TryGetInteger(_p[1])] );\n";
const string getitem_float2 = ctxput + "Float( _proc, (float) (*self)[DaoValue_TryGetInteger(_p[1])] );\n";
const string getitem_double2 = ctxput + "Double( _proc, (double) (*self)[DaoValue_TryGetInteger(_p[1])] );\n";

const string setitem_int2 = 
"  if( DaoValue_TryGetInteger(_p[1]) < 0 || DaoValue_TryGetInteger(_p[1]) >= $(size) ) return;\n\
  (*self)[DaoValue_TryGetInteger(_p[1])] = DaoValue_TryGetInteger(_p[2]);\n";

const string setitem_float2 = 
"  if( DaoValue_TryGetInteger(_p[1]) < 0 || DaoValue_TryGetInteger(_p[1]) >= $(size) ) return;\n\
  (*self)[DaoValue_TryGetInteger(_p[1])] = DaoValue_TryGetFloat(_p[2]);\n";

const string setitem_double2 = 
"  if( DaoValue_TryGetInteger(_p[1]) < 0 || DaoValue_TryGetInteger(_p[1]) >= $(size) ) return;\n\
  (*self)[DaoValue_TryGetInteger(_p[1])] = DaoValue_TryGetDouble(_p[2]);\n";

const string setter_int = "  self->$(name) = ($(cxxtype)) DaoValue_TryGetInteger(_p[1]);\n";
const string setter_float = "  self->$(name) = ($(cxxtype)) DaoValue_TryGetFloat(_p[1]);\n";
const string setter_double = "  self->$(name) = ($(cxxtype)) DaoValue_TryGetDouble(_p[1]);\n";
const string setter_string = // XXX array?
"  int size = DaoString_Size( (DaoString*)_p[1] );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoValue_TryGetMBString( _p[1] ), size );\n";

const string setter_shorts =
"  int size = DaoArray_Size( (DaoArray*)_p[1] );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoArray_ToShort( (DaoArray*)_p[1] ), size*sizeof(short) );\n";

const string setter_ints =
"  int size = DaoArray_Size( (DaoArray*)_p[1] );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoArray_ToSInt( (DaoArray*)_p[1] ), size*sizeof(int) );\n";

const string setter_floats =
"  int size = DaoArray_Size( (DaoArray*)_p[1] );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoArray_ToFloat( (DaoArray*)_p[1] ), size*sizeof(float) );\n";

const string setter_doubles =
"  int size = DaoArray_Size( (DaoArray*)_p[1] );\n\
  if( size > $(size) ) size = $(size);\n\
  memmove( self->$(name), DaoArray_ToDouble( (DaoArray*)_p[1] ), size*sizeof(double) );\n";

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );
extern string cdao_qname_to_idname( const string & qname );
extern string normalize_type_name( const string & name );
extern string cdao_make_dao_template_type_name( const string & name );
extern string cdao_substitute_typenames( const string & qname );

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
	if( var->isNullable ) dft = "|none";
	if( var->daodefault.size() ) dft += " =" + var->daodefault;
	if( var->cxxtyper.size() ) typer = var->cxxtyper;

	if( var->daotype.find( "std::" ) == 0 ) var->daotype.replace( 0, 5, "stdcxx::" );
	kvmap[ "daotype" ] = var->daotype;
	kvmap[ "cxxtype" ] = var->cxxtype2;
	kvmap[ "typer" ] = typer;
	kvmap[ "name" ] = var->name;
	kvmap[ "namespace" ] = "";
	kvmap[ "namespace2" ] = "";
	kvmap[ "index" ] = sindex;
	kvmap[ "default" ] = dft;
	kvmap[ "refer" ] = var->qualtype->isPointerType() ? var->name : "&" + var->name;
	if( var->isCallback ){
		var->cxxtype = normalize_type_name( var->qualtype.getAsString() );
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
		if( not var->qualtype.isConstQualified() ){
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
	hostype = NULL;
	initor = NULL;
	isNullable = false;
	isCallback = false;
	isUserData = false;
	hasArrayHint = false;
	unsupported = false;
	useDefault = true;
	isArithmeticType = false;
	isObjectType = false;
	isPointerType = false;
	useDaoString = false;
	SetDeclaration( decl );
}
void CDaoVariable::SetQualType( QualType qtype, SourceLocation loc )
{
	qualtype = qtype;
	location = loc;
}
void CDaoVariable::SetDeclaration( const VarDecl *decl )
{
	if( decl == NULL ) return;
	name = decl->getName().str();
	//outs() << ">>> variable: " << name << " " << decl->getType().getAsString() << "\n";
	//outs() << ">>> variable: " << name << " " << decl->getTypeSourceInfo()->getType().getAsString() << "\n";
	SetQualType( decl->getTypeSourceInfo()->getType(), decl->getLocation() );
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
		}if( hint == "string" ){
			useDaoString = true;
		}if( hint == "callbackdata" ){
			isUserData = true;
			size_t pos2 = hints2.find( "_hint_", pos );
			if( pos2 == string::npos ){
				callback = hints2.substr( pos+1 );
			}else if( pos2 > pos ){
				callback = hints2.substr( pos+1, pos2 - pos - 1 );
			}
			if( callback == "" ) errs() << "Warning: need callback name for \"callbackdata\" hint!\n";
		}else if( hint == "array" || hint == "qname" ){
			size_t pos2 = hints2.find( "_hint_", pos );
			vector<string> *parts = & sizes;
			if( hint == "qname" ) parts = & scopes;
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
				parts->push_back( hint.substr( from, pos - from ) );
				from = pos + 1;
			}
			if( from < pos2 ){
				if( pos2 == string::npos ){
					parts->push_back( hint.substr( from ) );
				}else{
					parts->push_back( hint.substr( from, pos2 - from ) );
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
		prefix = cdao_substitute_typenames( prefix );
		suffix = cdao_substitute_typenames( suffix );
		cxxpar = prefix + " " + name + suffix;
		cxxtype = prefix + suffix;
	}
	if( unsupported ) outs()<<"unsupported: "<<cxxtype<<" "<<name<<"\n";
	return retcode || unsupported;
}
int CDaoVariable::Generate2( int daopar_index, int cxxpar_index )
{
	if( initor ){
		SourceRange range = initor->getSourceRange();
		daodefault = module->ExtractSource( range, true );

		for(int i=0,n=scopes.size(); i<n; i++){
			daodefault = scopes[i] + "::" + daodefault;
			cxxdefault = scopes[i] + "::" + cxxdefault;
		}
		cxxdefault = "=" + daodefault;
		if( daodefault == "0L" ) daodefault = "0";

		std::replace( daodefault.begin(), daodefault.end(), '\"', '\'');

		Preprocessor & pp = module->compiler->getPreprocessor();
		SourceManager & sm = module->compiler->getSourceManager();
		SourceLocation start = sm.getExpansionLoc( range.getBegin() );
		SourceLocation end = sm.getExpansionLoc( range.getEnd() );
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
	QualType canotype = qualtype.getCanonicalType();
	string ctypename = normalize_type_name( canotype.getAsString() );
	cxxtype2 = normalize_type_name( GetStrippedTypeName( canotype ) );
	cxxtype = ctypename;
	cxxcall = name;
	//outs() << cxxtype << " " << cxxdefault << "  " << type->getTypeClassName() << "\n";
	//outs() << qualType.getAsString() << " " << qualType->getTypeClassName() << "\n";
	//outs() << type->isPointerType() << " is pointer type\n";
	//outs() << type->isArrayType() << " is array type\n";
	//outs() << type->isConstantArrayType() << " is constant array type\n";

	CDaoVarTemplates tpl;
	tpl.SetupIntScalar();
	map<string,string> kvmap;
	if( daodefault.size() > 1 ){
		int m = daodefault.size();
		if( daodefault[m-1] == 'f' && isdigit( daodefault[m-2] ) ) daodefault.erase( m-1 );
	}
	if( canotype->isBuiltinType() ){
		return GenerateForBuiltin( daopar_index, cxxpar_index );
	}else if( canotype->isPointerType() and not hasArrayHint ){
		isPointerType = true;
		return GenerateForPointer( daopar_index, cxxpar_index );
	}else if( canotype->isReferenceType() ){
		return GenerateForReference( daopar_index, cxxpar_index );
	}else if( canotype->isArrayType() ){
		return GenerateForArray( daopar_index, cxxpar_index );
	}else if( canotype->isEnumeralType() ){
		daotype = "int";
		isArithmeticType = true;
		tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
		return 0;
	}else if( CDaoUserType *UT = module->HandleUserType( canotype, location ) ){
		if( UT->unsupported ) return 1;
		UT->used = true;
		daotype = cdao_make_dao_template_type_name( UT->qname );
		cxxtype2 = UT->qname;
		cxxtyper = UT->idname;
		cxxcall = "*" + name;
		isObjectType = true;
		if( daodefault.size() ) useDefault = false;
		tpl.daopar = daopar_user;
		tpl.getres = getres_user2;
		tpl.dao2cxx = dao2cxx_user2;
		tpl.cxx2dao = cxx2dao_user;
		tpl.setter = "";
		if( daopar_index == VAR_INDEX_RETURN ){
			if( dyn_cast<CXXRecordDecl>( UT->decl ) ){
				tpl.ctxput = ctxput_newcdata;
			}else{
				tpl.ctxput = ctxput_copycdata;
			}
		}else{
			tpl.ctxput = ctxput_refcdata;
		}
		if( daodefault == "0" || daodefault == "NULL" ){
			daodefault = "none";
			isNullable = true;
		}
		tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
		return 0;
	}
	//TryHandleTemplateClass( qualtype );
	return 1;
}
int CDaoVariable::GenerateForBuiltin( int daopar_index, int cxxpar_index )
{
	QualType canotype = qualtype.getCanonicalType();
	CDaoVarTemplates tpl;
	if( canotype->isArithmeticType() ){
		daotype = "int";
		isNullable = false;
		isArithmeticType = true;
		tpl.SetupIntScalar();
		switch( canotype->getAs<BuiltinType>()->getKind() ){
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
		if( cxxtype != daotype ) module->daoTypedefs[ cxxtype ] = daotype;
	}
	map<string,string> kvmap;
	tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
	return 0;
}
int CDaoVariable::GenerateForPointer( int daopar_index, int cxxpar_index )
{
	QualType canotype = qualtype.getCanonicalType();
	QualType qtype1 = qualtype->getPointeeType();
	QualType qtype2 = canotype->getPointeeType();

	if( sizes.size() == 1 ) return GenerateForArray( qtype2, sizes[0], daopar_index, cxxpar_index );
	if( sizes.size() == 2 && qtype2->isPointerType() ){
		QualType qtype3 = qtype2->getAs<PointerType>()->getPointeeType();
		return GenerateForArray( qtype3, sizes[0], sizes[1], daopar_index, cxxpar_index );
	}

	CDaoVarTemplates tpl;
	map<string,string> kvmap;
	kvmap[ "size" ] = "0";
	kvmap[ "dao" ] = "";
	if( qtype2->isBuiltinType() and qtype2->isArithmeticType() ){
		const BuiltinType *type = qtype2->getAs<BuiltinType>();
		daotype = "int";
		cxxcall = "&" + name;
		isNullable = false;
		tpl.SetupIntScalar();
		switch( type->getKind() ){
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
		tpl.setter = "";
	}else if( qtype2->isEnumeralType() ){
		daotype = "int";
		cxxcall = "&" + name;
		isNullable = false;

		tpl.SetupIntScalar();
		tpl.parset = parset_int;
		tpl.ctxput = ctxput_ints;
		tpl.getres = getres_ints;
		tpl.cxx2dao = cxx2dao_int2;
		tpl.setter = "";
	}else if( CDaoUserType *UT = module->HandleUserType( qtype1, location ) ){
		if( UT->unsupported ) return 1;
		UT->used = true;
		if( qtype1.getAsString() == "FILE" ){
			daotype = "stream";
			cxxtype = "FILE";
			tpl.daopar = daopar_stream;
			tpl.dao2cxx = dao2cxx_stream;
			tpl.getres = getres_stream;
			tpl.ctxput = ctxput_stream;
			tpl.cxx2dao = cxx2dao_stream;
			if( daodefault == "0" || daodefault == "NULL" ) daodefault = "io";
		}else{
			daotype = cdao_make_dao_template_type_name( UT->qname );
			cxxtype2 = UT->qname;
			cxxtyper = UT->idname;
			tpl.daopar = daopar_user;
			tpl.ctxput = ctxput_user;
			tpl.getres = getres_user;
			tpl.dao2cxx = dao2cxx_user2;
			tpl.cxx2dao = cxx2dao_user;
			if( daodefault == "0" || daodefault == "NULL" ){
				daodefault = "none";
				isNullable = true;
			}
		}
	}else if( canotype->isVoidPointerType() ){
		if( isUserData ){
			daotype = "any";
			cxxtype = "DaoValue*";
			cxxpar = "DaoValue *" + name;
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
			daodefault = "none";
			isNullable = true;
		}
	}else if( const FunctionProtoType *ft = qtype2->getAs<FunctionProtoType>() ){
		//outs() << "callback: " << qualType.getAsString() << " " << qtype2.getAsString() << "\n";
		if( module->allCallbacks.find( ft ) == module->allCallbacks.end() ){
			module->allCallbacks[ ft ] = new CDaoFunction( module );
			CDaoFunction *func = module->allCallbacks[ ft ];
			string qname = GetStrippedTypeName( qtype1 );
			func->SetCallback( (FunctionProtoType*)ft, NULL, qname );
			func->cxxName = cdao_qname_to_idname( qname );
			func->location = location;
			if( func->retype.callback == "" ){
				errs() << "Warning: callback \"" << qtype1.getAsString() << "\" is not supported!\n";
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
int CDaoVariable::GenerateForReference( int daopar_index, int cxxpar_index )
{
	QualType canotype = qualtype.getCanonicalType();
	QualType qtype1 = qualtype->getPointeeType();
	QualType qtype2 = canotype->getPointeeType();

	CDaoVarTemplates tpl;
	if( qtype2->isBuiltinType() and qtype2->isArithmeticType() ){
		const BuiltinType *type = qtype2->getAs<BuiltinType>();
		daotype = "int";
		cxxcall = name;
		isNullable = false;
		tpl.SetupIntScalar();
		switch( type->getKind() ){
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
	}else if( qtype2->isEnumeralType() ){
		daotype = "int";
		cxxcall = name;
		isNullable = false;

		tpl.SetupIntScalar();
		tpl.parset = parset_int;
	}else if( CDaoUserType *UT = module->HandleUserType( qtype1, location ) ){
		if( UT->unsupported ) return 1;
		UT->used = true;
		daotype = cdao_make_dao_template_type_name( UT->qname );
		cxxtype2 = UT->qname;
		cxxtyper = UT->idname;
		cxxcall = "*" + name;
		tpl.daopar = daopar_user;
		tpl.ctxput = ctxput_refcdata;
		tpl.getres = getres_user;
		tpl.dao2cxx = dao2cxx_user2;
		tpl.cxx2dao = cxx2dao_user;
		if( daodefault == "0" || daodefault == "NULL" ){
			daodefault = "none";
			isNullable = true;
		}
	}else{
		return 1;
	}
	map<string,string> kvmap;
	tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
	return 0;
}
int CDaoVariable::GenerateForArray( int daopar_index, int cxxpar_index )
{
	string size;
	QualType canotype = qualtype.getCanonicalType();
	const ArrayType *type = (ArrayType*)canotype.getTypePtr();
	if( canotype->isConstantArrayType() ){
		const ConstantArrayType *at = (ConstantArrayType*)canotype.getTypePtr();
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
		return GenerateForArray2( type3->getElementType(), size, size2, daopar_index, cxxpar_index );
	}
	CDaoVarTemplates tpl;
	map<string,string> kvmap;
	kvmap[ "dao" ] = "";
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
	if( size.size() == 0 ) size = "0";
	kvmap[ "size" ] = size;
	tpl.Generate( this, kvmap, daopar_index, cxxpar_index );
	return 0;
}
int CDaoVariable::GenerateForArray( QualType elemtype, string size, string size2, int dpid, int cpid )
{
	const Type *type2 = elemtype.getTypePtr();
	string ctypename2 = elemtype.getAsString();
	CDaoVarTemplates tpl;
	map<string,string> kvmap;
	kvmap[ "dao" ] = "";
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
	if( size.size() == 0 ) size = "0";
	if( size2.size() == 0 ) size2 = "0";
	kvmap[ "size" ] = size;
	kvmap[ "size2" ] = size2;
	tpl.Generate( this, kvmap, dpid, cpid );
	return 0;
}
int CDaoVariable::GenerateForArray2( QualType elemtype, string size, string size2, int dpid, int cpid )
{
	const Type *type2 = elemtype.getTypePtr();
	string ctypename2 = elemtype.getAsString();
	CDaoVarTemplates tpl;
	map<string,string> kvmap;
	kvmap[ "dao" ] = "";
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
			tpl.dao2cxx = dao2cxx_bmat2;
			tpl.cxx2dao = cxx2dao_bmat2;
			tpl.ctxput = ctxput_bytes;
			tpl.parset = parset_bytes;
			tpl.getres = getres_bytes;
			tpl.setter = setter_string;
			break;
		case BuiltinType::Bool :
		case BuiltinType::Char_U :
		case BuiltinType::UChar :
			tpl.dao2cxx = dao2cxx_ubmat2;
			tpl.cxx2dao = cxx2dao_ubmat2;
			tpl.ctxput = ctxput_bytes;
			tpl.parset = parset_ubytes;
			tpl.getres = getres_ubytes;
			tpl.setter = setter_string;
			break;
		case BuiltinType::UShort :
			tpl.dao2cxx = dao2cxx_usmat2;
			tpl.cxx2dao = cxx2dao_usmat2;
			tpl.ctxput = ctxput_shorts;
			tpl.parset = parset_ushorts;
			tpl.getres = getres_ushorts;
			tpl.setter = setter_shorts;
			break;
		case BuiltinType::Char16 :
		case BuiltinType::Short :
			tpl.dao2cxx = dao2cxx_smat2;
			tpl.cxx2dao = cxx2dao_smat2;
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
			tpl.dao2cxx = dao2cxx_uimat2;
			tpl.cxx2dao = cxx2dao_uimat2;
			break;
		case BuiltinType::WChar_S :
		case BuiltinType::Char32 :
		case BuiltinType::Int :
		case BuiltinType::Long : // FIXME: check size
		case BuiltinType::LongLong : // FIXME
		case BuiltinType::Int128 : // FIXME
			tpl.dao2cxx = dao2cxx_imat2;
			tpl.cxx2dao = cxx2dao_imat2;
			break;
		case BuiltinType::Float :
			daotype = "array<float>";
			dao_itemtype = "float";
			tpl.daopar = daopar_floats;
			tpl.dao2cxx = dao2cxx_fmat2;
			tpl.cxx2dao = cxx2dao_fmat2;
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
			tpl.dao2cxx = dao2cxx_dmat2;
			tpl.cxx2dao = cxx2dao_dmat2;
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
	if( size.size() == 0 ) size = "0";
	if( size2.size() == 0 ) size2 = "0";
	kvmap[ "size" ] = size;
	kvmap[ "size2" ] = size2;
	tpl.Generate( this, kvmap, dpid, cpid );
	return 0;
}
void CDaoVariable::MakeCxxParameter( string & prefix, string & suffix )
{
	const Type *type = qualtype.getTypePtr();
	prefix = "";
	if( const TypedefType *TDT = dyn_cast<TypedefType>( type ) ){
		DeclContext *DC = TDT->getDecl()->getDeclContext();
		if( DC->isNamespace() ){
			NamespaceDecl *ND = dyn_cast<NamespaceDecl>( DC );
			prefix = ND->getQualifiedNameAsString() + "::";
		}else if( DC->isRecord() ){
			RecordDecl *RD = dyn_cast<RecordDecl>( DC )->getDefinition();
			CDaoUserType *UT = module->GetUserType( RD );
			//outs() << RD->getQualifiedNameAsString() << " " << UT << " " << (void*)RD << "\n";
			if( hostype && hostype->decl ){
				// For vector( size_type __n, ... ), the canonical type name of size_type is
				// std::vector::size_type, even for vector<bool>. And RD will be a specialization
				// different from vector<bool>!
				RecordDecl *RD2 = hostype->decl;
				ClassTemplateSpecializationDecl *CTS = dyn_cast<ClassTemplateSpecializationDecl>( RD2 );
				ClassTemplateSpecializationDecl *CTS2 = dyn_cast<ClassTemplateSpecializationDecl>( RD );
				CXXRecordDecl *CRD1 = CTS ? CTS->getSpecializedTemplate()->getTemplatedDecl() : NULL;
				CXXRecordDecl *CRD2 = CTS2 ? CTS2->getSpecializedTemplate()->getTemplatedDecl() : NULL;
				if( CRD1 && CRD2 && CRD1 == CRD2 ) UT = hostype;
				if( CTS && RD == CTS->getSpecializedTemplate()->getTemplatedDecl() ) UT = hostype;
			}
			if( UT ){
				prefix = UT->qname + "::";
			}else{
				prefix = RD->getQualifiedNameAsString() + "::";
			}
			//outs() << ">>>>>>>>>>>>>>> " << name <<  " " << prefix << " " << (void*)this << "\n";
		}
		prefix += qualtype.getUnqualifiedType().getAsString();
	}else{
		QualType canotype = qualtype.getCanonicalType();
		MakeCxxParameter( canotype, prefix, suffix );
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
	if( type->isBooleanType() ){
		prefix = "bool" + prefix;
	}else if( type->isBuiltinType() ){
		prefix = qtype.getUnqualifiedType().getAsString() + prefix;
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
		prefix = normalize_type_name( qtype.getUnqualifiedType().getAsString() ) + prefix;
	}else if( decl ){
		// const C & other: const is part of the name, not a qualifier.
		prefix = normalize_type_name(qtype.getUnqualifiedType().getAsString()) + prefix;
	}
	if( qtype.getCVRQualifiers() & Qualifiers::Const ) prefix = "const " + prefix;
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
string CDaoVariable::GetStrippedTypeName( QualType qtype )
{
	QualType QT = GetStrippedType( qtype );
	if( QT->isBooleanType() ) return "bool"; // clang always produces "_Bool"
	return QT.getAsString();
}
