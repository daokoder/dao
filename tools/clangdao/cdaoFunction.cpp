
#include <llvm/ADT/StringExtras.h>
#include <clang/Sema/DeclSpec.h>
#include "cdaoFunction.hpp"
#include "cdaoModule.hpp"

const string cxx_wrap_proto 
= "static void dao_$(func_idname)$(overload)( DaoContext *_ctx, DValue *_p[], int _n )";

const string cxx_call_proto = 
"  $(retype) $(name) = $(func_call)( $(parlist) );\n";

const string cxx_call_proto_d1 = 
"  $(retype) $(name);\n\
  if(_n<=$(n1)) $(name) = $(func_call)( $(parlist1) );\n\
  else $(name) = $(func_call)( $(parlist) );\n"
;
const string cxx_call_proto_d2 = 
"  $(retype) $(name);\n\
  if(_n<=$(n1)) $(name) = $(func_call)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(name) = $(func_call)( $(parlist2) );\n\
  else $(name) = $(func_call)( $(parlist) );\n"
;
const string cxx_call_proto_d3 = 
"  $(retype) $(name);\n\
  if(_n<=$(n1)) $(name) = $(func_call)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(name) = $(func_call)( $(parlist2) );\n\
  else if(_n<=$(n3)) $(name) = $(func_call)( $(parlist3) );\n\
  else $(name) = $(func_call)( $(parlist) );\n"
;
const string cxx_call_proto_d4 = 
"  $(retype) $(name);\n\
  if(_n<=$(n1)) $(name) = $(func_call)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(name) = $(func_call)( $(parlist2) );\n\
  else if(_n<=$(n3)) $(name) = $(func_call)( $(parlist3) );\n\
  else if(_n<=$(n4)) $(name) = $(func_call)( $(parlist4) );\n\
  else $(name) = $(func_call)( $(parlist) );\n"
;
const string cxx_call_proto_list[] = 
{
	cxx_call_proto,
	cxx_call_proto_d1,
	cxx_call_proto_d2,
	cxx_call_proto_d3,
	cxx_call_proto_d4
};

const string cxx_call_proto2 = 
"  $(func_call)( $(parlist) );\n";

const string cxx_call_proto2_d1 = 
"  if(_n<=$(n1)) $(func_call)( $(parlist1) );\n\
  else $(func_call)( $(parlist) );\n"
;
const string cxx_call_proto2_d2 = 
"  if(_n<=$(n1)) $(func_call)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(func_call)( $(parlist2) );\n\
  else $(func_call)( $(parlist) );\n"
;
const string cxx_call_proto2_d3 = 
"  if(_n<=$(n1)) $(func_call)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(func_call)( $(parlist2) );\n\
  else if(_n<=$(n3)) $(func_call)( $(parlist3) );\n\
  else $(func_call)( $(parlist) );\n"
;
const string cxx_call_proto2_d4 = 
"  if(_n<=$(n1)) $(func_call)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(func_call)( $(parlist2) );\n\
  else if(_n<=$(n3)) $(func_call)( $(parlist3) );\n\
  else if(_n<=$(n4)) $(func_call)( $(parlist4) );\n\
  else $(func_call)( $(parlist) );\n"
;

const string cxx_call_proto2_list[] = 
{
	cxx_call_proto2,
	cxx_call_proto2_d1,
	cxx_call_proto2_d2,
	cxx_call_proto2_d3,
	cxx_call_proto2_d4
};

const string cxx_call_static = 
"  $(retype) $(name) = $(host_qname)::$(cxxname)( $(parlist) );\n";

const string cxx_call_static_d1 = 
"  $(retype) $(name);\n\
  if(_n<=$(n1)) $(name) = $(host_qname)::$(cxxname)( $(parlist1) );\n\
  else $(name) = $(host_qname)::$(cxxname)( $(parlist) );\n"
;
const string cxx_call_static_d2 = 
"  $(retype) $(name);\n\
  if(_n<=$(n1)) $(name) = $(host_qname)::$(cxxname)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(name) = $(host_qname)::$(cxxname)( $(parlist2) );\n\
  else $(name) = $(host_qname)::$(cxxname)( $(parlist) );\n"
;
const string cxx_call_static_d3 = 
"  $(retype) $(name);\n\
  if(_n<=$(n1)) $(name) = $(host_qname)::$(cxxname)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(name) = $(host_qname)::$(cxxname)( $(parlist2) );\n\
  else if(_n<=$(n3)) $(name) = $(host_qname)::$(cxxname)( $(parlist3) );\n\
  else $(name) = $(host_qname)::$(cxxname)( $(parlist) );\n"
;
const string cxx_call_static_d4 = 
"  $(retype) $(name);\n\
  if(_n<=$(n1)) $(name) = $(host_qname)::$(cxxname)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(name) = $(host_qname)::$(cxxname)( $(parlist2) );\n\
  else if(_n<=$(n3)) $(name) = $(host_qname)::$(cxxname)( $(parlist3) );\n\
  else if(_n<=$(n4)) $(name) = $(host_qname)::$(cxxname)( $(parlist4) );\n\
  else $(name) = $(host_qname)::$(cxxname)( $(parlist) );\n"
;
const string cxx_call_static_list[] = 
{
	cxx_call_static,
	cxx_call_static_d1,
	cxx_call_static_d2,
	cxx_call_static_d3,
	cxx_call_static_d4
};

const string cxx_call_static2 = 
"  $(host_qname)::$(cxxname)( $(parlist) );\n";

const string cxx_call_static2_d1 = 
"  if(_n<=$(n1)) $(host_qname)::$(cxxname)( $(parlist1) );\n\
  else $(host_qname)::$(cxxname)( $(parlist) );\n"
;
const string cxx_call_static2_d2 = 
"  if(_n<=$(n1)) $(host_qname)::$(cxxname)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(host_qname)::$(cxxname)( $(parlist2) );\n\
  else $(host_qname)::$(cxxname)( $(parlist) );\n"
;
const string cxx_call_static2_d3 = 
"  if(_n<=$(n1)) $(host_qname)::$(cxxname)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(host_qname)::$(cxxname)( $(parlist2) );\n\
  else if(_n<=$(n3)) $(host_qname)::$(cxxname)( $(parlist3) );\n\
  else $(host_qname)::$(cxxname)( $(parlist) );\n"
;
const string cxx_call_static2_d4 = 
"  if(_n<=$(n1)) $(host_qname)::$(cxxname)( $(parlist1) );\n\
  else if(_n<=$(n2)) $(host_qname)::$(cxxname)( $(parlist2) );\n\
  else if(_n<=$(n3)) $(host_qname)::$(cxxname)( $(parlist3) );\n\
  else if(_n<=$(n4)) $(host_qname)::$(cxxname)( $(parlist4) );\n\
  else $(host_qname)::$(cxxname)( $(parlist) );\n"
;
const string cxx_call_static2_list[] = 
{
	cxx_call_static2,
	cxx_call_static2_d1,
	cxx_call_static2_d2,
	cxx_call_static2_d3,
	cxx_call_static2_d4
};

const string cxx_call_new = 
"	$(host_qname) *_self = Dao_$(host_idname)_New( $(parlist) );\n\
	DaoContext_PutCData( _ctx, _self, dao_$(host_idname)_Typer );\n";
const string cxx_call_new2 = 
"	DaoCxx_$(host_idname) *_self = DaoCxx_$(host_idname)_New( $(parlist) );\n\
	DaoContext_PutResult( _ctx, (DaoBase*) _self->cdata );\n";

const string dao_proto =
"  { dao_$(func_idname)$(overload), \"$(daoname)( $(parlist) )$(retype)\" },\n";

const string cxx_wrap = 
"$(proto)\n{\n$(dao2cxx)\n$(cxxcall)$(parset)$(return)}\n";

const string cxx_virt_proto =
"static $(retype) Dao_$(type)_$(cxxname)( $(parlist) );\n";

const string cxx_virt_struct =
"static $(retype) Dao_$(type)_$(cxxname)( $(parlist) )\n\
{\n\
  Dao_$(type) *self = (Dao_$(type)*) self0;\n\
  $(type) *self2 = self->object;\n\
  DaoCData *cdata = self->cdata;\n\
  DaoVmProcess *vmproc = DaoVmSpace_AcquireProcess( __daoVmSpace );\n";

const string cxx_virt_class =
"$(retype) DaoCxxVirt_$(type)::$(cxxname)( int &_cs$(comma) $(parlist) )$(const)\n{\n";

const string qt_get_wrapper1 =
"DaoBase* DaoQt_Get_Wrapper( const QObject *object );\n";
const string qt_get_wrapper2 =
"DaoBase* DaoQt_Get_Wrapper( const QObject *object )\n\
{\n\
  DaoQtObject *user_data = (DaoQtObject*) ((QObject*)object)->userData(0);\n\
  DaoBase *dbase = NULL;\n\
  // no need to map to DaoObject, because it will always be mapped back to\n\
  // DaoCData when passed to Dao codes.\n\
  if( user_data ) dbase = (DaoBase*) user_data->cdata;\n\
  return dbase;\n\
}\n";

const string cxx_virt_call_00 =
"  DValue _obj = {0,0,0,0,{0}};\n\
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, \"$(cxxname)\" );\n\
  if( _ro ==NULL || _obj.t != DAO_OBJECT ) return;\n\
  _ro = DaoMethod_Resolve( _ro, & _obj, NULL, 0 );\n\
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) return;\n\
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );\n\
  DaoVmProcess_Call( _vmp, _ro, & _obj, NULL, 0 );\n\
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );\n\
}\n";

const string cxx_virt_call_01 =
"  DValue _obj = {0,0,0,0,{0}};\n\
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, \"$(cxxname)\" );\n\
  if( _ro ==NULL || _obj.t != DAO_OBJECT ) return;\n\
  $(proxy_name)( & _cs, _ro, & _obj, $(parcall) );\n\
}\n";

const string cxx_virt_call_10 =
"  DValue _obj = {0,0,0,0,{0}};\n\
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, \"$(cxxname)\" );\n\
  $(vareturn)\n\
  if( _ro ==NULL || _obj.t != DAO_OBJECT ) return $(return);\n\
  return ($(retype))$(proxy_name)( & _cs, _ro, & _obj );\n\
}\n";

const string cxx_virt_call_11 =
"  DValue _obj = {0,0,0,0,{0}};\n\
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, \"$(cxxname)\" );\n\
  $(vareturn)\n\
  if( _ro ==NULL || _obj.t != DAO_OBJECT ) return $(return);\n\
  return ($(retype))$(proxy_name)( & _cs, _ro, & _obj, $(parcall) );\n\
}\n";

const string cxx_proxy_body00 =
"static void $(proxy_name)( int *_cs, DaoMethod *_ro, DValue *_ob )\n{\n\
  if( _ro == NULL ) return;\n\
  _ro = DaoMethod_Resolve( _ro, _ob, NULL, 0 );\n\
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) return;\n\
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );\n\
  *_cs = DaoVmProcess_Call( _vmp, _ro, _ob, NULL, 0 );\n\
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );\n\
}\n";

const string cxx_proxy_body01 =
"static void $(proxy_name)( int *_cs, DaoMethod *_ro, DValue *_ob, $(parlist) )\n{\n\
  const DValue _dao_nil = {0,0,0,0,{0}};\n\
  DValue _dp[$(count)] = { $(nils) };\n\
  DValue *_dp2[$(count)] = { $(refs) };\n\
  if( _ro == NULL ) return;\n\
$(cxx2dao)\n\
  _ro = DaoMethod_Resolve( _ro, _ob, _dp2, $(count) );\n\
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) return;\n\
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );\n\
  *_cs = DaoVmProcess_Call( _vmp, _ro, _ob, _dp2, $(count) );\n\
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );\n\
  DValue_ClearAll( _dp, $(count) );\n\
}\n";

const string cxx_proxy_body10 =
"static $(retype) $(proxy_name)( int *_cs, DaoMethod *_ro, DValue *_ob )\n{\n\
  DValue _res;\n\
  DaoCData *_cd;\n\
  DaoVmProcess *_vmp;\n\
  $(vareturn)\n\
  if( _ro == NULL ) goto EndCall;\n\
  _ro = DaoMethod_Resolve( _ro, _ob, NULL, 0 );\n\
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) goto EndCall;\n\
  _vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );\n\
  if( (*_cs = DaoVmProcess_Call( _vmp, _ro, _ob, NULL, 0 )) ==0 ) goto EndCall;\n\
  _res = DaoVmProcess_GetReturned( _vmp );\n\
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );\n\
$(getreturn)\n\
EndCall:\n\
  return $(return);\n\
}\n";

const string cxx_proxy_body11 =
"static $(retype) $(proxy_name)( int *_cs, DaoMethod *_ro, DValue *_ob, $(parlist) )\n{\n\
  const DValue _dao_nil = {0,0,0,0,{0}};\n\
  DValue _dp[$(count)] = { $(nils) };\n\
  DValue *_dp2[$(count)] = { $(refs) };\n\
  DValue _res;\n\
  DaoCData *_cd;\n\
  DaoVmProcess *_vmp;\n\
  $(vareturn)\n\
  if( _ro == NULL ) goto EndCall;\n\
$(cxx2dao)\n\
  _ro = DaoMethod_Resolve( _ro, _ob, _dp2, $(count) );\n\
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) goto EndCall;\n\
  _vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );\n\
  if( (*_cs = DaoVmProcess_Call( _vmp, _ro, _ob, _dp2, $(count) )) ==0 ) goto EndCall;\n\
  _res = DaoVmProcess_GetReturned( _vmp );\n\
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );\n\
$(getreturn)\n\
EndCall:\n\
  DValue_ClearAll( _dp, $(count) );\n\
  return $(return);\n\
}\n";

//XXX ((DaoCxxVirt_$(type)*)this)->DaoCxxVirt_$(type)::$(cxxname)
const string cxx_virt_class2 =
"$(retype) DaoCxx_$(type)::$(cxxname)( $(parlist) )$(const)\n{\n\
  int _cs = 0;\n\
  return ((DaoCxxVirt_$(type)*)this)->DaoCxxVirt_$(type)::$(cxxname)( _cs$(comma) $(parcall) );\n\
}\n";

const string cxx_virt_class3 =
"$(retype) DaoCxx_$(type)::$(cxxname)( $(parlist) )$(const)\n{\n\
  int _cs = 0;\n\
  DValue _obj = {0,0,0,0,{0}};\n\
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, \"$(cxxname)\" );\n\
  if( _ro && _obj.t == DAO_OBJECT ){\n\
    ((DaoCxxVirt_$(type)*)this)->DaoCxxVirt_$(type)::$(cxxname)( _cs$(comma) $(parcall) );\n\
    if( _cs ) return;\n\
  }\n\
  $(type)::$(cxxname)( $(parcall) );\n\
}\n";

const string cxx_virt_class4 =
"$(retype) DaoCxx_$(type)::$(cxxname)( $(parlist) )$(const)\n{\n\
  int _cs = 0;\n\
  DValue _obj = {0,0,0,0,{0}};\n\
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, \"$(cxxname)\" );\n\
  if( _ro && _obj.t == DAO_OBJECT ){\n\
    $(vareturn) = ((DaoCxxVirt_$(type)*)this)->DaoCxxVirt_$(type)::$(cxxname)( _cs$(comma) $(parcall) );\n\
    if( _cs ) return $(vareturn2);\n\
  }\n\
  return $(type)::$(cxxname)( $(parcall) );\n\
}\n";

const string cxx_virt_class5 =
"$(retype) DaoCxxVirt_$(sub)::$(cxxname)( int &_cs$(comma) $(parlist) )$(const)\n{\n\
  $(return) ((DaoCxxVirt_$(type)*)this)->DaoCxxVirt_$(type)::$(cxxname)( _cs$(comma) $(parcall) );\n\
}\n";

const string dao_callback_proto =
"$(retype) Dao_$(type)( $(parlist) );";

const string dao_callback_def =
"$(retype) Dao_$(type)( $(parlist) )\n\
{\n\
  DaoCallbackData *_daocallbackdata = (DaoCallbackData*) _ud;\n\
  DaoMethod *_ro = _daocallbackdata->callback;\n\
  DValueX userdata = _daocallbackdata->userdata;\n\
  int _cs = 0;\n\
  if( _ro ==NULL ) return;\n\
  $(proxy_name)( & _cs, _ro, NULL, $(parcall) );\n\
}\n";

const string daoqt_slot_slot_decl =
"   void slot_$(ssname)( void*, void*, const DaoQtMessage& );\n";

const string daoqt_slot_slot_code =
"void DaoSS_$(host_idname)::slot_$(ssname)( void*, void*, const DaoQtMessage &_msg )\n\
{\n\
  DValue **_p = (DValue**) _msg.p2;\n";

const string daoqt_sig_slot_decl =
"   void slot_$(ssname)( $(parlist) );\n";

const string daoqt_sig_slot_code =
"void DaoSS_$(host_idname)::slot_$(ssname)( $(parlist) )\n\
{\n\
  DaoQtMessage _message( $(count) );\n\
  assert( $(count) <= DAOQT_MAX_VALUE );\n\
  DValue *_dp = _message.p1;\n";

const string daoqt_sig_slot_emit = 
"  emit signal_$(ssname)( this, \"$(signature)\", _message );\n}\n";


extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );
extern string normalize_type_name( const string & name );

CDaoFunction::CDaoFunction( CDaoModule *mod, FunctionDecl *decl, int idx )
{
	module = mod;
	funcDecl = NULL;
	excluded = false;
	index = idx;
	if( decl ) SetDeclaration( decl );
}
void CDaoFunction::SetDeclaration( FunctionDecl *decl )
{
	int i, n;
	funcDecl = decl;
	parlist.clear();
	if( decl == NULL ) return;
	
	string sig = funcDecl->getNameAsString() + "(";
	for(i=0, n=decl->param_size(); i<n; i++){
		ParmVarDecl *pardecl = decl->getParamDecl( i );
		parlist.push_back( CDaoVariable( module, pardecl ) );
		if( i ) sig += ",";
		sig += pardecl->getTypeSourceInfo()->getType().getAsString();
	}
	retype.name = "_" + decl->getNameAsString();
	retype.qualType = funcDecl->getResultType();

	sig += ")";
	sig = normalize_type_name( sig );
	map<string,vector<string> >::iterator it = module->functionHints.find( sig );
	if( it == module->functionHints.end() ) return;
	outs() << "hints found for: " << sig << "\n";
	vector<string> & hints = it->second;
}
bool CDaoFunction::IsFromMainModule()
{
	if( funcDecl == NULL ) return false;
	return module->IsFromMainModule( funcDecl->getLocation() );
}
string CDaoFunction::GetInputFile()const
{
	if( funcDecl == NULL ) return false;
	return module->GetFileName( funcDecl->getLocation() );
}

struct IntString{ int i; string s; };

int CDaoFunction::Generate()
{
	bool isconst = false;
	const CXXMethodDecl *methdecl = dyn_cast<CXXMethodDecl>( funcDecl );
	const CXXRecordDecl *hostdecl = NULL;
	ASTContext & ctx = module->compiler->getASTContext();
	string host_name, host_qname, host_idname;
	int autoself = 0;
	if( methdecl ){
		const CXXConstructorDecl *ctor = dyn_cast<CXXConstructorDecl>( methdecl );
		hostdecl = methdecl->getParent();
		host_name = hostdecl->getName().str();
		host_qname = CDaoModule::GetQName( hostdecl );
		host_idname = CDaoModule::GetIdName( hostdecl );
		isconst = methdecl->getTypeQualifiers() & DeclSpec::TQ_const;
		if( methdecl->isInstance() && ctor == NULL ){
			parlist.insert( parlist.begin(), CDaoVariable( module ) );
			parlist.front().qualType = ctx.getPointerType( ctx.getRecordType( hostdecl ) );
			parlist.front().name = "self";
			autoself = 1;
		}
	}

	int retcode = 0;
	int i, n = parlist.size();
	for(i=0; i<n; i++) retcode |= parlist[i].Generate( i, i-autoself );
	retcode |= retype.Generate( VAR_INDEX_RETURN );
	excluded = retcode != 0;

#if 0
	if( retype.unsupport or retype.isCallback ) excluded = 1;
	pcount = parlist.size();
	hasCallback = 0;
	for( vo in parlist ){
		vo.Generate( cxx_no_self );
		if( vo.isCallback ) hasCallback = 1;
	}
#endif
	string daoprotpars, cxxprotpars, cxxcallpars;
	string dao2cxxcodes, cxx2daocodes, parsetcodes;
	string slot_dao2cxxcodes, cxxprotpars_decl;
	string nils, refs;

	string cxxName = funcDecl->getNameAsString();
	string daoName = cxxName;
	string cxxCallParamV, cxxProtoParamVirt;
	bool hasUserData = 0;

	signature = cxxName + "("; // exclude return type from signature
	signature2 = retype.cxxtype2 + "(";

	vector<IntString> calls_with_defaults;
	for(i=0; i<n; i++){
		CDaoVariable & vo = parlist[i];
		//outs() << vo.name << vo.unsupport << "-----------------\n";
		if( vo.unsupport ){
			excluded = true;
			return 1;
		}
		//if( vo.name == 'userdata' ) hasUserData = 1;
		if( i ) daoprotpars += ", ";
		daoprotpars += vo.daopar;
		//if( vo.sqsizes.size() <2 ) 
		dao2cxxcodes += vo.dao2cxx;
		parsetcodes += vo.parset;
#if 0
		if( vo.isconst and vo.refer == '&' and vo.vdefault != '' ){
			calls_with_defaults.append( (i, cxxcallpars) );
		}
#endif
		if( i < autoself ) continue;
		if( i > autoself ){
			nils += ", ";
			refs += ", ";
			cxxProtoParamVirt += ", ";
			cxxprotpars += ", ";
			cxxprotpars_decl += ", ";
			cxxcallpars += ", ";
			signature += ",";
			signature2 += ",";
			cxxCallParamV += ", ";
		}
		signature += vo.cxxtype;
		signature2 += vo.cxxtype2;
		//cxxProtoParamVirt += vo.cxxpar_enum_virt;
		cxxProtoParamVirt += vo.cxxpar;
		cxxprotpars += vo.cxxpar;
		cxxprotpars_decl += vo.cxxpar + vo.cxxdefault;
		cxxcallpars += vo.cxxcall;
		cxx2daocodes += vo.cxx2dao;
		cxxCallParamV += vo.name;
		nils += "_dao_nil";
		refs += "_dp+" + utostr(i-autoself);
#if 0
		if( $QT_SLOT in attribs ){
			-- vo.index;
			vo.Generate( cxx_no_self );
			if( i ) slot_dao2cxxcodes += vo.dao2cxx;
		}
#endif
	}
#if 0
	if( hasCallback and not hasUserData ) excluded = 1;
	nowrap = excluded;
	if( not nowrap and ($FUNC_PROTECTED in attribs) ){
		nowrap = ($FUNC_STATIC in attribs or $FUNC_PURE_VIRTUAL in attribs);
		if( retype.typename != retype.type_trans ) nowrap = 1;
		for( vo in parlist ){
			if( vo.typename != vo.type_trans ) nowrap = 1;
		}
	}
#endif
	signature += ")";
	signature2 += ")";
	if( isconst ){
		signature += "const";
		signature2 += "const";
	}
	outs() << signature << "\n";
	outs() << signature2 << "\n";
	outs() << funcDecl->getTypeSourceInfo()->getType().getAsString() << "\n";

	// because C/C++ parameter list is need for constructors!
	cxxProtoParam = cxxprotpars;
	cxxProtoParamDecl = cxxprotpars_decl;
	cxxCallParam = cxxcallpars;

	string overload;
	if( index > 1 ) overload = "_dao_" + utostr( index );

	cxxWrapName = "dao_" + host_idname + "_" + cxxName;

	map<string,string> kvmap;
	kvmap[ "host_qname" ] = host_qname;
	kvmap[ "host_idname" ] = host_idname;
	kvmap[ "func_idname" ] = CDaoModule::GetIdName( funcDecl );
	kvmap[ "func_call" ] = CDaoModule::GetQName( funcDecl );
	kvmap[ "cxxname" ] = cxxName;
	kvmap[ "daoname" ] = daoName;
	kvmap[ "parlist" ] = daoprotpars;
	kvmap[ "retype" ] = "";
	kvmap[ "self" ] = "";
	kvmap[ "count" ] = utostr( n-1 );
	kvmap[ "nils" ] = nils;
	kvmap[ "refs" ] = refs;
	kvmap[ "signature" ] = signature;
	kvmap[ "overload" ] = overload;

	if( retype.daotype != "" ) kvmap[ "retype" ] = "=>" + retype.daotype;

	daoProtoCodes = cdao_string_fill( dao_proto, kvmap );
	cxxProtoCodes = cdao_string_fill( cxx_wrap_proto, kvmap );

	kvmap[ "retype" ] = retype.cxxtype;
	kvmap[ "name" ] = retype.name;
	kvmap[ "parlist" ] = cxxCallParam;
	
	if( methdecl ){
		string ss = "self->";
		if( methdecl->getAccess() != AS_protected && ! methdecl->isPure() ){
			ss += host_name + "::";
		}
		kvmap[ "func_call" ] = ss + cxxName;
	}
	int dd = calls_with_defaults.size();
	for(i=1; i<=dd; i++){
		IntString & tup2 = calls_with_defaults[i-1];
		kvmap[ "n" + utostr(i) ] = utostr( tup2.i );
		kvmap[ "parlist" + utostr(i) ] = tup2.s;
	}
	CDaoUserType *usertype = module->GetUserType( hostdecl );
	if( usertype && host_name == cxxName and not excluded ){
		//kvmap[ 'parlist' ] = ''; // XXX disable parameters at the moment
		if( not usertype->noConstructor ){
			if( usertype->hasVirtual ){
				cxxCallCodes = cdao_string_fill( cxx_call_new2, kvmap );
			}else{
				cxxCallCodes = cdao_string_fill( cxx_call_new, kvmap );
			}
		}
		//if( parlist.size() ) cxxCallCodes = ''; // XXX maybe there is no default constru
	}else if( retype.daotype != "" ){
		if( methdecl && methdecl->isStatic() )
			cxxCallCodes = cdao_string_fill( cxx_call_static_list[dd], kvmap );
		else
			cxxCallCodes = cdao_string_fill( cxx_call_proto_list[dd], kvmap );
	}else{
		if( methdecl && methdecl->isStatic() )
			cxxCallCodes = cdao_string_fill( cxx_call_static2_list[dd], kvmap );
		else
			cxxCallCodes = cdao_string_fill( cxx_call_proto2_list[ dd ], kvmap );
	}
#if 0
	if( lib_name == LibType.LIB_QT and not excluded ){
		size = (string)RotatingHash( signature );
		ssname = cxxName + size;
		sig = '   void signal_' + signature + ';\n'
		sig.replace( cxxName+'(', cxxName+size+'(' );
		kvmap['ssname'] = ssname;
		if( $QT_SLOT in attribs ){
			qtSlotSignalDecl = sig;
			qtSlotSlotDecl = daoqt_slot_slot_decl.expand( kvmap );
			qtSlotSlotCode = daoqt_slot_slot_code.expand( kvmap );
			qtSlotSlotCode += slot_dao2cxxcodes;
			qtSlotSlotCode += '  emit signal_' + ssname + '( ' + cxxCallParam + ' );\n}\n';
		}else if( $QT_SIGNAL in attribs ){
			qtSignalSignalDecl = '   void signal_' + ssname + '(void*,const QString&,const DaoQtMessage&);\n'
			kvmap['parlist'] = cxxprotpars_decl;
			qtSignalSlotDecl = daoqt_sig_slot_decl.expand( kvmap );
			kvmap['parlist'] = cxxProtoParam;
			qtSignalSlotCode = daoqt_sig_slot_code.expand( kvmap );
			qtSignalSlotCode += cxx2daocodes;
			qtSignalSlotCode += daoqt_sig_slot_emit.expand( kvmap );
		}
	}
#endif
	
	map<string,string> kvmap2;
	kvmap2[ "proto" ] = cxxProtoCodes;
	kvmap2[ "dao2cxx" ] = dao2cxxcodes;
	kvmap2[ "cxxcall" ] = cxxCallCodes;
	kvmap2[ "parset" ] = parsetcodes;
	kvmap2[ "return" ] = retype.ctxput;
	
	if( retype.daotype.size() ==0 || host_name == cxxName ) kvmap2[ "return" ] = "";
	//if( hostType == cxxName ) kvmap2[ 'dao2cxx' ] =''; # XXX 
	cxxWrapper = cdao_string_fill( cxx_wrap, kvmap2 );
#if 0
	if( cxxWrapper.find( 'self->' + hostType + '::' ) >=0 ){
		daoProtoCodes.change( ', (%s*) \"(%w+%b())', '__' + hostType + ',%1\"' + hostType + '::%2' );
		cxxProtoCodes.change( '_(%w+)(%b())', '_%1__' + hostType + '%2' );
		cxxWrapper.change( '({{static void }}%w+)_(%w+)(%b())', '%1_%2__' + hostType + '%3' );
	}
#endif
	map<string,string> kvmap3;
	kvmap3[ "retype" ] = retype.cxxtype;
	kvmap3[ "type" ] = host_idname;
	kvmap3[ "cxxname" ] = cxxName;
	kvmap3[ "parlist" ] = cxxProtoParam;
	kvmap3[ "count" ] = utostr( parlist.size() );
	kvmap3[ "nils" ] = nils;
	kvmap3[ "refs" ] = refs;
	kvmap3[ "cxx2dao" ] = cxx2daocodes;
	kvmap3[ "vareturn" ] = retype.cxxpar + " = 0;";
	kvmap3[ "return" ] = retype.name;
	kvmap3[ "getreturn" ] = retype.getres;
	kvmap3[ "const" ] = "";
	
	string vareturn = retype.cxxpar + " = 0;";
#if 0
	if( retype.refer == "" and retype.typeid > $CT_USER ){
		tks = retype.dao2cxx.capture( "= %s* %b()" );
		if( tks ) vareturn = retype.cxxpar + tks[0] + "0;";
	}
	//if( retype.typename.capture( qt_container ) ) vareturn = retype.cxxpar + ";";
	if( retype.daotype.size() ==0 ){
		vareturn = "";
		kvmap3[ "getreturn" ] = "";
		kvmap3[ "return" ] = "";
	}else if( retype.varDecl->isPointerType() ){
		utp = map_user_types[ retype.typename ];
		if( utp.isCppClass ){
			if( retype.refer == "*" )
				vareturn = retype.cxxpar + " = NULL;";
			else
				vareturn = retype.cxxpar + ";";
		}
	}
#endif
	kvmap3[ "vareturn" ] = vareturn;
	if( isconst ) kvmap3[ "const" ] = "const";
	
	if( methdecl ){
		kvmap3[ "count" ] = utostr( parlist.size() - autoself );
		kvmap3[ "parlist" ] = cxxProtoParamVirt;
		kvmap3[ "comma" ] = cxxProtoParamVirt.size() ? "," : "";
		cxxWrapperVirt = cdao_string_fill( cxx_virt_class, kvmap3 );
	}else{
		cxxWrapperVirtProto = cdao_string_fill( cxx_virt_proto, kvmap3 );
		cxxWrapperVirt = cdao_string_fill( cxx_virt_struct, kvmap3 );
		cxxWrapperVirt += "  int _cs = 0;\n";
	}

	bool has_return = retype.daotype.size() != 0;
	bool has_param = (parlist.size() - autoself) > 0;

	//    signature2 = signature;
	//    signature2.change( '^%w+', '' );
	//    signature2 = retype.cxxtype + signature2;
	
	//io.writeln( signature2 );
	string proxy_name = "";
	if( CDaoProxyFunction::IsNotDefined( signature2 ) ){
		string proxy_codes = "";
		string proxy_name = CDaoProxyFunction::NewProxyFunctionName();
		kvmap3[ "proxy_name" ] = proxy_name;
		if( has_return and has_param ){
			proxy_codes = cdao_string_fill( cxx_proxy_body11, kvmap3 );
		}else if( has_return ){
			proxy_codes = cdao_string_fill( cxx_proxy_body10, kvmap3 );
		}else if( has_param ){
			proxy_codes = cdao_string_fill( cxx_proxy_body01, kvmap3 );
		}else{
			proxy_codes = cdao_string_fill( cxx_proxy_body00, kvmap3 );
		}
		CDaoProxyFunction::Add( signature2, proxy_name, proxy_codes );
		//outs() << proxy_name << "\n" << proxy_codes << "\n";
	}
	if( has_return or has_param ) proxy_name = CDaoProxyFunction::proxy_functions[ signature2 ].name;
	kvmap3[ "proxy_name" ] = proxy_name;
	kvmap3[ "parcall" ] = cxxCallParamV;
	
#if 0
	if( isCallback and not dao_callbacks2.has( self ) ){
		cbproto = dao_callback_proto.expand( kvmap3 );
		callback = dao_callback_def.expand( kvmap3 );
		cbproto.replace( 'DValue userdata', 'void *_ud' );
		callback.replace( 'DValue userdata', 'void *_ud' );
		callback.replace( 'DValueX userdata', 'DValue userdata' );
		proxy_functions[ signature2 ].used += 1;
		dao_callbacks.append( callback );
		dao_callbacks_proto.append( cbproto );
		dao_callbacks2[ self ] = 1;
	}
#endif
	
	if( has_return ==0 and has_param ==0 ){
		cxxWrapperVirt += cdao_string_fill( cxx_virt_call_00, kvmap3 );
	}else if( has_return ==0 and has_param ){
		cxxWrapperVirt += cdao_string_fill( cxx_virt_call_01, kvmap3 );
	}else if( has_return and has_param ==0 ){
		cxxWrapperVirt += cdao_string_fill( cxx_virt_call_10, kvmap3 );
	}else{
		cxxWrapperVirt += cdao_string_fill( cxx_virt_call_11, kvmap3 );
	}
	if( methdecl ){
		kvmap3[ "return" ] = "return";
		kvmap3[ "comma" ] = cxxCallParamV.size() ? "," : "";
		//XXX if( retype.cxxtype == "void" && retype.refer == "" ) kvmap3[ "return" ] = "";
		if( retype.cxxtype == "void" ) kvmap3[ "return" ] = "";
		kvmap3[ "parlist" ] = cxxProtoParam;
		if( methdecl->isPure() ){
			cxxWrapperVirt2 = cdao_string_fill( cxx_virt_class2, kvmap3 );
		}else if( retype.daotype.size() ==0 ){
			cxxWrapperVirt2 = cdao_string_fill( cxx_virt_class3, kvmap3 );
		}else{
			kvmap3["vareturn"] = retype.cxxpar;
			kvmap3["vareturn2"] = retype.name;
			cxxWrapperVirt2 = cdao_string_fill( cxx_virt_class4, kvmap3 );
		}
		kvmap3[ "parlist" ] = cxxProtoParamVirt;
		cxxWrapperVirt3 = cdao_string_fill( cxx_virt_class5, kvmap3 );
	}
	parlist.clear();
	return retcode;
}

int  CDaoProxyFunction::proxy_function_index = 0x10000;
map<string,CDaoProxyFunction>  CDaoProxyFunction::proxy_functions;
