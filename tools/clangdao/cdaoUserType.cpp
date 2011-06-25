
#include<map>

#include "cdaoFunction.hpp"
#include "cdaoUserType.hpp"
#include "cdaoModule.hpp"

const string dao_proto =
"  { dao_$(host_typer)_$(cxxname)$(overload), \"$(daoname)( $(parlist) )$(retype)\" },\n";

const string cxx_wrap_proto 
= "static void dao_$(host_typer)_$(cxxname)$(overload)( DaoContext *_ctx, DValue *_p[], int _n )";

const string tpl_struct = "$(name)* DAO_DLL_$(module) Dao_$(name)_New();\n";

const string tpl_struct_alloc =
"$(name)* Dao_$(name)_New()\n{\n"
"	$(name) *self = calloc( 1, sizeof($(name)) );\n"
"	return self;\n"
"}\n";
const string tpl_struct_alloc2 =
"$(name)* Dao_$(name)_New()\n{\n"
"	$(name) *self = new $(name)();\n"
"	return self;\n"
"}\n";

const string tpl_struct_daoc = 
"typedef struct Dao_$(name) Dao_$(name);\n"
"struct DAO_DLL_$(module) Dao_$(name)\n"
"{\n"
"	$(name)  nested;\n"
"	$(name) *object;\n"
"	DaoCData *cdata;\n"
"};\n"
"Dao_$(name)* DAO_DLL_$(module) Dao_$(name)_New();\n";

const string tpl_struct_daoc_alloc =
"Dao_$(name)* Dao_$(name)_New()\n"
"{\n"
"	Dao_$(name) *wrap = calloc( 1, sizeof(Dao_$(name)) );\n"
"	$(name) *self = ($(name)*) wrap;\n"
"	wrap->cdata = DaoCData_New( dao_$(name)_Typer, wrap );\n"
"	wrap->object = self;\n"
"$(callbacks)$(selfields)\treturn wrap;\n"
"}\n";

const string tpl_set_callback = "\tself->$(callback) = Dao_$(name)_$(callback);\n";
const string tpl_set_selfield = "\tself->$(field) = wrap;\n";

const string c_wrap_new = 
"static void dao_$(host)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n"
"{\n"
"	$(namespace)Dao_$(host) *self = $(namespace)Dao_$(host)_New();\n"
"	DaoContext_PutResult( _ctx, (DaoBase*) self->cdata );\n"
"}\n";
const string cxx_wrap_new = 
"static void dao_$(host)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n"
"{\n"
"	$(namespace)DaoCxx_$(host) *self = $(namespace)DaoCxx_$(host)_New();\n"
"	DaoContext_PutResult( _ctx, (DaoBase*) self->cdata );\n"
"}\n";
const string cxx_wrap_alloc2 = 
"static void dao_$(host)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n"
"{\n"
"	$(namespace)$(host) *self = $(namespace)Dao_$(host)_New();\n"
"	DaoContext_PutCData( _ctx, self, dao_$(host)_Typer );\n"
"}\n";

const string tpl_class_def = 
"class DAO_DLL_$(module) DaoCxxVirt_$(class) $(virt_supers)\n{\n"
"	public:\n"
"	DaoCxxVirt_$(class)(){ self = 0; cdata = 0; }\n"
"	void DaoInitWrapper( $(class) *self, DaoCData *d );\n\n"
"	$(class) *self;\n"
"	DaoCData *cdata;\n"
"\n$(virtuals)\n"
"$(qt_virt_emit)\n"
"};\n"
"class DAO_DLL_$(module) DaoCxx_$(class) : public $(class), public DaoCxxVirt_$(class)\n"
"{ $(Q_OBJECT)\n\n"
"\tpublic:\n"
"$(constructors)\n"
"	~DaoCxx_$(class)();\n"
"	void DaoInitWrapper();\n\n"
"$(methods)\n"
"};\n";

const string tpl_class2 = 
tpl_class_def + "$(class)* Dao_$(class)_Copy( const $(class) &p );\n";

const string tpl_class_init =
"void DaoCxxVirt_$(class)::DaoInitWrapper( $(class) *s, DaoCData *d )\n"
"{\n"
"	self = s;\n"
"	cdata = d;\n"
"$(init_supers)\n"
"$(qt_init)\n"
"}\n"
"DaoCxx_$(class)::~DaoCxx_$(class)()\n"
"{\n"
"	if( cdata ){\n"
"		DaoCData_SetData( cdata, NULL );\n"
"		DaoCData_SetExtReference( cdata, 0 );\n"
"	} \n"
"}\n"
"void DaoCxx_$(class)::DaoInitWrapper()\n"
"{\n"
"	cdata = DaoCData_New( dao_$(class)_Typer, this );\n"
"	DaoCxxVirt_$(class)::DaoInitWrapper( this, cdata );\n"
"$(qt_make_linker)\n"
"}\n";
const string tpl_class_init_qtss = 
"void DAO_DLL_$(module) Dao_$(class)_InitSS( $(class) *p )\n"
"{\n"
"   if( p->userData(0) == NULL ){\n"
"		DaoSS_$(class) *linker = new DaoSS_$(class)();\n"
"		p->setUserData( 0, linker );\n"
"		linker->Init( NULL );\n"
"	}\n"
"}\n";
const string tpl_class_copy = tpl_class_init +
"$(class)* DAO_DLL_$(module) Dao_$(class)_Copy( const $(class) &p )\n"
"{\n"
"	$(class) *object = new $(class)( p );\n"
"$(qt_make_linker3)\n"
"	return object;\n"
"}\n";
const string tpl_class_decl_constru = 
"   DaoCxx_$(class)( $(parlist) ) : $(class)( $(parcall) ){}\n";

const string tpl_class_new =
"\nDaoCxx_$(class)* DAO_DLL_$(module) DaoCxx_$(class)_New( $(parlist) );\n";
const string tpl_class_new_novirt =
"\n$(class)* DAO_DLL_$(module) Dao_$(class)_New( $(parlist) );\n";
const string tpl_class_init_qtss_decl =
"\nvoid DAO_DLL_$(module) Dao_$(class)_InitSS( $(class) *p );\n";

const string tpl_class_noderive =
"\n$(class)* DAO_DLL_$(module) Dao_$(class)_New( $(parlist) )\n"
"{\n"
"	$(class) *object = new $(class)( $(parcall) );\n"
"$(qt_make_linker3)\n"
"	return object;\n"
"}\n";
const string tpl_class_init2 =
"\nDaoCxx_$(class)* DAO_DLL_$(module) DaoCxx_$(class)_New( $(parlist) )\n"
"{\n"
"	DaoCxx_$(class) *self = new DaoCxx_$(class)( $(parcall) );\n"
"	self->DaoInitWrapper();\n"
"	return self;\n"
"}\n";

const string tpl_init_super = "\tDaoCxxVirt_$(super)::DaoInitWrapper( s, d );\n";

const string tpl_meth_decl =
"\t$(retype) $(name)( int &_cs$(comma) $(parlist) )$(extra);\n";

const string tpl_meth_decl2 =
"\t$(retype) $(name)( $(parlist) )$(extra);\n";

const string tpl_meth_decl3 =
"\t$(retype) DaoWrap_$(name)( $(parlist) )$(extra)";

const string tpl_raise_call_protected =
"  if( _p[0]->t == DAO_CDATA && DaoCData_GetObject( _p[0]->v.cdata ) == NULL ){\n"
"    DaoContext_RaiseException( _ctx, DAO_ERROR, \"call to protected method\" );\n"
"    return;\n"
"  }\n";


extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );

CDaoUserType::CDaoUserType( CDaoModule *mod, RecordDecl *decl )
{
	module = mod;
	alloc_default = "NULL";
	SetDeclaration( decl );
}
void CDaoUserType::SetDeclaration( RecordDecl *decl )
{
	this->decl = decl;
}
int CDaoUserType::Generate()
{
	RecordDecl *dd = decl->getDefinition();
	outs() << decl->getNameAsString() << ": " << (void*)decl <<" " << (void*)dd << "\n";
	if( dd and dd != decl ) return 0;
	if (CXXRecordDecl *record = dyn_cast<CXXRecordDecl>(decl)) return Generate( record );
	return 1;
}
int CDaoUserType::Generate( CXXRecordDecl *decl )
{
	string name = decl->getNameAsString();
	map<string,int> overloads;
	bool no_wrapping = false;
	bool has_virtual = false; // protected or public virtual function;
	bool has_pure_virtual = false;

	vector<CDaoUserType*> bases;
	vector<CDaoFunction>  methods;
	vector<CDaoFunction>  constructors;
	
	map<RecordDecl*,int>::iterator find;
	CXXRecordDecl::base_class_iterator baseit, baseend = decl->bases_end();
	for(baseit=decl->bases_begin(); baseit != baseend; baseit++){
		CXXRecordDecl *p = baseit->getType().getTypePtr()->getAsCXXRecordDecl();
		find = module->usertypes2.find( p );
		if( find == module->usertypes2.end() ) continue;
		bases.push_back( & module->usertypes[ find->second ] );
		outs() << "parent: " << p->getNameAsString() << "\n";
	}
	CXXRecordDecl::method_iterator methit, methend = decl->method_end();
	for(methit=decl->method_begin(); methit!=methend; methit++){
		string name = methit->getNameAsString();
		outs() << name << ": " << methit->getAccess() << " ......................\n";
		outs() << methit->getType().getAsString() << "\n";
		if( methit->isImplicit() ) continue;
		if( methit->isPure() ) has_pure_virtual = true;
		if( methit->getAccess() == AS_private ) continue;
		if( methit->isVirtual() ) has_virtual = true;
		methods.push_back( CDaoFunction( module, *methit, ++overloads[name] ) );
		methods.back().Generate();
	}

	bool has_ctor = false;
	bool has_protected_ctor = false;
	bool has_private_ctor = false;
	bool has_private_default_ctor = false;
	bool has_explicit_default_ctor = false;
	bool has_implicit_default_ctor = true;
	CXXRecordDecl::ctor_iterator ctorit, ctorend = decl->ctor_end();
	for(ctorit=decl->ctor_begin(); ctorit!=ctorend; ctorit++){
		string name = ctorit->getNameAsString();
		outs() << name << ": " << ctorit->getAccess() << " ......................\n";
		outs() << ctorit->getType().getAsString() << "\n";
		if( ctorit->isImplicit() ) continue;
		if( not ctorit->isImplicitlyDefined() ){
			has_ctor = true;
			has_implicit_default_ctor = false;
			if( ctorit->getAccess() == AS_private ) has_private_ctor = true;
			if( ctorit->getAccess() == AS_protected ) has_protected_ctor = true;
			if( ctorit->param_size() == 0 ){
				has_explicit_default_ctor = true;
				if( ctorit->getAccess() == AS_private ) has_private_default_ctor = true;
			}
		}
		constructors.push_back( CDaoFunction( module, *ctorit, ++overloads[name] ) );
		constructors.back().Generate();
		if( ctorit->getAccess() == AS_private ) continue;
		if( ctorit->getAccess() == AS_protected && not has_virtual ) continue;
		CDaoFunction & meth = constructors.back();
		dao_meths += meth.daoProtoCodes;
		meth_decls += meth.cxxProtoCodes + ";\n";
		meth_codes += meth.cxxWrapper;
	}
	// no explicit default constructor and explicit private constructor
	// imply private default constructor:
	if( has_private_ctor && not has_explicit_default_ctor ) has_private_default_ctor = true;

	map<string,string> kvmap;
	kvmap[ "module" ] = UppercaseString( module->moduleInfo.name );
	kvmap[ "host_typer" ] = name;
	kvmap[ "cxxname" ] = name;
	kvmap[ "daoname" ] = name;
	kvmap[ "retype" ] = "=>" + name;

	bool implement_default_ctor = has_virtual && not no_wrapping;
	implement_default_ctor = implement_default_ctor && has_implicit_default_ctor;
	if( implement_default_ctor ){
		if( has_virtual ){
			//if( not isPureVirt ){
			dao_meths += cdao_string_fill( dao_proto, kvmap );
			meth_decls += cdao_string_fill( cxx_wrap_proto, kvmap ) + ";\n";
			meth_codes += cdao_string_fill( cxx_wrap_new, kvmap );
			//}
		}else if( not has_protected_ctor and not has_private_ctor ){
			dao_meths += cdao_string_fill( dao_proto, kvmap );
			meth_decls += cdao_string_fill( cxx_wrap_proto, kvmap ) + ";\n";
			meth_codes += cdao_string_fill( cxx_wrap_alloc2, kvmap );
		}
	}
	int i, j, n, m;
	if( not has_virtual and not has_protected_ctor and not no_wrapping ){
		//kvmap = { 'name'=>name, 'class'=>name, 'qt_make_linker3'=>'' };
		//if( isQObject ) kvmap['qt_make_linker3'] = qt_make_linker3.expand( kvmap );
		kvmap[ "name" ] = name;
		if( has_ctor ){
			for(i=0, n = constructors.size(); i<n; i++){
				CDaoFunction & func = constructors[i];
				const CXXConstructorDecl *ctor = dyn_cast<CXXConstructorDecl>( func.funcDecl );
				//if( alloc.excluded ) skip;
				if( ctor->getAccess() == AS_protected ) continue;
				kvmap["parlist"] = func.cxxProtoParam;
				kvmap["parcall"] = func.cxxCallParamV;
				type_codes += cdao_string_fill( tpl_class_noderive, kvmap );
				type_decls += cdao_string_fill( tpl_class_new_novirt, kvmap );
			}
		}else if( not has_private_ctor ){
			type_decls = cdao_string_fill( tpl_struct, kvmap );
			type_codes = cdao_string_fill( tpl_struct_alloc2, kvmap );
		}
	}
	map<string,int> mapnonvirt;
	map<string,int> mapprivate;
	for(i=0, n = methods.size(); i<n; i++){
		//implemented_meth_names[ meth.cxxName ] = 1;
		CDaoFunction & meth = methods[i];
		const CXXMethodDecl *mdec = dyn_cast<CXXMethodDecl>( meth.funcDecl );
		if( mdec->getAccess() == AS_private ){
			mapprivate[ meth.signature ] = 1;
			continue;
		}
		string proto, wrapper;
		kvmap[ "name" ] = mdec->getNameAsString();
		kvmap[ "retype" ] = meth.retype.cxxtype;
		kvmap[ "extra" ] = "";
		//if( $FUNC_CONST in meth.attribs ) kvmap['extra'] = 'const';
		if( mdec->isVirtual() ){
			kvmap[ "parlist" ] = meth.cxxProtoParamDecl;
			// TODO if excluded
			proto = cdao_string_fill( tpl_meth_decl2, kvmap );
			declmeths[ meth.signature ] = CDaoMethodDecl( mdec, proto );
			kvmap[ "parlist" ] = meth.cxxProtoParamVirt;
			kvmap[ "comma" ] = meth.cxxProtoParamVirt.size() ? "," : "";
			proto = cdao_string_fill( tpl_meth_decl, kvmap );
			wrapper = meth.cxxWrapperVirt2;
			declvirts[ meth.signature ] = CDaoMethodDecl( mdec, proto, wrapper );
		}else{
			mapnonvirt[ meth.signature ] = 1;
		}
	}
	map<string,CDaoMethodDecl>::iterator mdit, mdit2, mdit3;
	for(i=0, n = bases.size(); i<n; i++){
		CDaoUserType *base = bases[i];
		for(mdit=base->declmeths.begin(), mdit2=base->declmeths.end(); mdit!=mdit2; mdit++){
			const CXXMethodDecl *md = mdit->second.decl;
			if( mapprivate.find( mdit->first ) != mapprivate.end() ) continue;
			if( mapnonvirt.find( mdit->first ) != mapnonvirt.end() ) continue;
			if( md->getParent() != decl && not md->isPure() ) continue;
			if( declmeths.find( mdit->first ) != declmeths.end() ) continue;
			declmeths[ mdit->first ] = mdit->second;
		}
		for(mdit=base->declvirts.begin(), mdit2=base->declvirts.end(); mdit!=mdit2; mdit++){
			const CXXMethodDecl *md = mdit->second.decl;
			if( mapprivate.find( mdit->first ) != mapprivate.end() ) continue;
			if( mapnonvirt.find( mdit->first ) != mapnonvirt.end() ) continue;
			if( md->getParent() != decl && not md->isPure() ) continue;
			if( declvirts.find( mdit->first ) != declvirts.end() ) continue;
			declvirts[ mdit->first ] = mdit->second;
			//kvmap = { 'sub' => name };
			//if( meth.excluded ==0 ) cxxWrapperVirt += meth.cxxWrapperVirt3.expand( kvmap );
		}
	}
#if 0
		for( meth in virtNonSupport ){
			if( $FUNC_VIRTUAL in meth.attribs ){
				declmeths[ meth.signature ] = meth;
				declvirts[ meth.signature ] = meth;
			}
			if( $FUNC_VIRTUAL not in meth.attribs ) mapnonvirt[ meth.signature ] = 1;
		}
		//io.writeln( name, mapnonvirt, methods );
		for( sup in supers ){
			for( s in sup.restPureVirts ){
				//if( s[0] == sup.name ) 
				restPureVirts.append( s );
			}
		}
		//io.writeln( declmeths, declvirts, '\n' );
		kmethods = '';
		for( meth in declmeths.values() ){
			kvmap = { 'name'=>meth.cxxName, 'retype'=>meth.retype.cxxtype,
			'parlist'=>meth.cxxProtoParamDecl, 'extra'=>'' };
			if( $FUNC_CONST in meth.attribs ) kvmap['extra'] = 'const';
			if( meth.excluded ){
				if( $FUNC_PURE_VIRTUAL in meth.attribs )
					kmethods += '  ' + meth.source + '{/*XXX 1*/}\n';
			}else{
				kmethods += tpl_meth_decl2.expand( kvmap );
			}
		}
		tmp = '{ $(return)$(type)::$(cxxname)( $(parcall) ); }\n';
		kvm = { 'type'=>name };
		kvirtuals = '';
		for( meth in declvirts.values() ){
			if( meth.excluded ) skip;
			//{
			      cxxProtoParamVirt for DaoCxxVirt_XXX:
			      where the virtual functions may use XXX::YYY as default value,
			      which should be stripped from the method declaration in DaoCxxVirt_XXX.
			//}
			kvmap = { 'name'=>meth.cxxName, 'retype'=>meth.retype.cxxtype,
			'parlist'=>meth.cxxProtoParamVirt, 'extra'=>'' };
			kvmap[ 'comma' ] = meth.cxxProtoParamDecl ? ',' : '';
			if( $FUNC_CONST in meth.attribs) kvmap['extra'] = 'const';
			kvirtuals += tpl_meth_decl.expand( kvmap );
			if( $FUNC_VIRTUAL in meth.attribs ){
				tmp = meth.cxxWrapperVirt2;
				// for virtual function from parent class:
				//tmp.change( meth.hostType + '::', name + '::' ); 2010-03-20
				/*
				        if( implemented_meth_names.has( meth.cxxName ) ){
				          tmp.change( '_' + meth.hostType + '::', '_' + name + '::' );
				        }else{
				          tmp.change( meth.hostType + '::', name + '::' );
				        }
				*/
				tmp.change( '_' + meth.hostType + '::', '_' + name + '::' );
				cxxWrapperVirt += tmp;
			}
		}
		for( pvirt in restPureVirts ){
			//kvm[ 'cxxname' ] = pvirt.cxxName;
			//kvm[ 'parcall' ] = pvirt.cxxCallParamV;
			//cd = pvirt.retype.typeid == CT_VOID ? '{}\n' : tmp.expand( kvm );
			// 2010-01-19, 03:04
			kmethods += '\t' + pvirt[1] + '{ /*XXX 2*/ }\n';
		}
		tmp = '{ $(return)DaoCxxVirt_$(type)::$(cxxname)( $(parcall) ); }\n';
		tmp = '{ $(return)$(type)::$(cxxname)( $(parcall) ); }\n';
		for( meth in methods ){
			if( meth.reimplemented != '' and not meth.nowrap ){
				kvmap = { 'name'=>meth.cxxName, 'retype'=>meth.retype.cxxtype,
				'parlist'=>meth.cxxProtoParam, 'extra'=>'' };
				if( $FUNC_CONST in meth.attribs ) kvmap['extra'] = 'const';
				kmethods += tpl_meth_decl2.expand( kvmap );
				tmp2 = meth.cxxWrapperVirt2;
				tmp2.change( meth.hostType + '::(%w+ %( %s+ _cs%W )', meth.reimplemented + '::%1' );
				cxxWrapperVirt += tmp2;
			}
			if( not noWrapping && not meth.excluded && ($FUNC_PROTECTED in meth.attribs) ){
				kvmap = { 'name'=>meth.cxxName, 'retype'=>meth.retype.cxxtype,
				'parlist'=>meth.cxxProtoParamDecl, 'extra'=>'' };
				if( $FUNC_CONST in meth.attribs ) kvmap['extra'] = 'const';
				rc = meth.retype.typeid == $CT_VOID ? '' : 'return ';
				kvm[ 'cxxname' ] = meth.cxxName;
				kvm[ 'parcall' ] = meth.cxxCallParamV;
				kvm[ 'return' ] = rc;
				kmethods += tpl_meth_decl3.expand( kvmap ) + tmp.expand( kvm );
			}
		}
		kvmap = { 'class'=>name };
		daoc_supers = '';
		virt_supers = '';
		get_supers = '';
		init_supers = '';
		ss_supers = '';
		ss_init_sup = '';
		for( sup in supers ){
			if( sup.noWrapping or not sup.hasVirtual ) skip;
			kvmap[ 'super' ] = sup.name;
			if( virt_supers ){
				daoc_supers += ',';
				virt_supers += ',';
			}
			daoc_supers += ' public DaoCxx_' + sup.name;
			virt_supers += ' public DaoCxxVirt_' + sup.name;
			init_supers += tpl_init_super.expand( kvmap );
			if( sup.isQObject ){
				if( ss_supers ){
					ss_supers += ',';
					ss_init_sup += ',';
				}
				ss_supers += ' public DaoSS_' + sup.name;
				ss_init_sup += ' DaoSS_' + sup.name + '()';
			}
		}
		if( isQObjectBase ){
			ss_supers += 'public QObject, public DaoQtObject';
		}
		if( daoc_supers ) daoc_supers = ' :' + daoc_supers;
		if( virt_supers ) virt_supers = ' :' + virt_supers;
		if( init_supers ) init_supers += '\n';
		kvmap = { 'class'=>name, 'virt_supers'=>virt_supers,
		'supers'=>daoc_supers, 'get_supers'=>get_supers,
		'virtuals'=>kvirtuals, 'methods'=>kmethods,
		'init_supers'=>init_supers, 'parlist'=>'', 'parcall'=>'', 
		'Q_OBJECT'=>'', 'qt_signal_slot'=>'', 
		'qt_init'=>'', 'parents'=>'public QObject',
		'init_parents'=>'', 'qt_make_linker'=>'', 'qt_virt_emit'=>'',
		'qt_make_linker3'=>''};
		if( isQObject ){
			kvmap['Q_OBJECT'] = 'Q_OBJECT';
			kvmap['qt_init'] = qt_init;
			kvmap['qt_make_linker'] = qt_make_linker.expand( kvmap );
			kvmap['qt_make_linker3'] = qt_make_linker3.expand( kvmap );
			kvmap['qt_virt_emit'] = qt_virt_emit;
		}
		if( not noWrapping and allocators.size() ){
			for( alloc in allocators ){
				if( $FUNC_PRIVATE in alloc.attribs ) skip;
				if( ($FUNC_PROTECTED in alloc.attribs) and noWrapping ) skip;
				kvmap['parlist'] = alloc.cxxProtoParamDecl;
				kvmap['parcall'] = alloc.cxxCallParamV;
				class_decl += tpl_class_decl_constru.expand( kvmap );
				kvmap['parlist'] = alloc.cxxProtoParam;
				class_new += tpl_class_new.expand( kvmap );
				type_codes += tpl_class_init2.expand( kvmap );
			}
		}else if( not noWrapping and not private_default and (not noConstructor or hasVirtual) ){
			// class has no virtual methods but has protected constructor
			// should also be wrapped, so that it can be derived by Dao class:
			class_new += tpl_class_new.expand( kvmap );
			type_codes += tpl_class_init2.expand( kvmap );
		}else if( not hasVirtual ){
			type_codes += tpl_class_noderive.expand( kvmap );
		}
		if( isQObject ){
			qt_signals = '';
			qt_slots = '';
			for( meth in pubslots ){
				qt_signals += meth.qtSlotSignalDecl;
				qt_slots += meth.qtSlotSlotDecl;
				type_codes += meth.qtSlotSlotCode;
			}
			for( meth in protsignals ){
				qt_signals += meth.qtSignalSignalDecl;
				qt_slots += meth.qtSignalSlotDecl;
				type_codes += meth.qtSignalSlotCode;
			}
			kvmap['signals'] = qt_signals;
			kvmap['slots'] = qt_slots;
			if( ss_init_sup ) kvmap['init_parents'] = ':'+ ss_init_sup;
			kvmap['parents'] = ss_supers;
			kvmap['member_wrap'] = '';
			kvmap['set_wrap'] = '';
			kvmap['Emit'] = '';
			kvmap['slotDaoQt'] = '';
			kvmap['signalDao'] = '';
			if( isQObjectBase ){
				kvmap['member_wrap'] = '   ' + name + ' *wrap;\n';
				kvmap['set_wrap'] = 'wrap = w;';
				kvmap['Emit'] = qt_Emit;
				kvmap['slotDaoQt'] = qt_slotDaoQt.expand( kvmap );
				kvmap['signalDao'] = qt_signalDao;
			}
			type_decls += daoss_class.expand( kvmap );
		} // isQObject
		kvmap['constructors'] = class_decl;
		if( not noWrapping ){
			if( isPureVirt || noCopyConstructor ){
				type_decls += tpl_class_def.expand( kvmap ) + class_new;
				type_codes += tpl_class_init.expand( kvmap );
			}else{
				type_decls += tpl_class2.expand( kvmap ) + class_new;
				type_codes += tpl_class_copy.expand( kvmap );
			}
		}
#endif
	outs() << "class wrapping\n";
	outs() << type_decls << "\n";
	outs() << type_codes << "\n";
	outs() << dao_meths << "\n";
	outs() << meth_decls << "\n";
	outs() << meth_codes << "\n";
	return 1;
}
