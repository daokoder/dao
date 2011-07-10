
#include <clang/Lex/Preprocessor.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Sema/DeclSpec.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/Template.h>
#include <map>

#include "cdaoFunction.hpp"
#include "cdaoUserType.hpp"
#include "cdaoModule.hpp"

const string daoqt_object_class =
"#include<QHash>\n\
\n\
#ifndef DAOQT_MAX_VALUE\n\
#define DAOQT_MAX_VALUE 16\n\
#endif\n\
class DaoQtMessage\n\
{\n\
  void Init( int n ){\n\
    count = n;\n\
    memset( p1, 0, DAOQT_MAX_VALUE * sizeof(DValue) );\n\
    for(int i=0; i<DAOQT_MAX_VALUE; i++) p2[i] = p1 + i;\n\
  }\n\
  \n\
  public:\n\
  int     count;\n\
  DValue  p1[DAOQT_MAX_VALUE];\n\
  DValue *p2[DAOQT_MAX_VALUE];\n\
  \n\
  DaoQtMessage( int n=0 ){ Init( n ); }\n\
  DaoQtMessage( const DaoQtMessage & other ){\n\
    Init( other.count );\n\
    for(int i=0; i<count; i++) DValue_Copy( p2[i], other.p1[i] );\n\
  }\n\
  ~DaoQtMessage(){ DValue_ClearAll( p1, count ); }\n\
};\n\
Q_DECLARE_METATYPE( DaoQtMessage );\n\
\n\
class DaoQtSlot : public QObject\n\
{ Q_OBJECT\n\
\n\
public:\n\
	DaoQtSlot(){\n\
		anySender = NULL;\n\
		daoReceiver = NULL;\n\
		daoSignal = NULL;\n\
		memset( & daoSlot, 0, sizeof(DValue) );\n\
	}\n\
	DaoQtSlot( void *p, DaoCData *rev, const QString & sig, const DValue & slot ){\n\
		anySender = p;\n\
		daoReceiver = rev;\n\
		daoSignal = NULL;\n\
		qtSignal = sig;\n\
		daoSlot = slot;\n\
	}\n\
	DaoQtSlot( void *p, DaoCData *rev, void *sig, const DValue & slot ){\n\
		anySender = p;\n\
		daoReceiver = rev;\n\
		daoSignal = sig;\n\
		daoSlot = slot;\n\
	}\n\
	bool Match( void *sender, const QString & signal, DValue slot ){\n\
		return anySender == sender && qtSignal == signal && daoSlot.v.p == slot.v.p;\n\
	}\n\
	bool Match( void *sender, void *signal, DValue slot ){\n\
		return anySender == sender && daoSignal == signal && daoSlot.v.p == slot.v.p;\n\
	}\n\
	\n\
	void      *anySender;\n\
	void      *daoSignal;\n\
	QString    qtSignal;\n\
	\n\
	DaoCData  *daoReceiver;\n\
	DValue     daoSlot;\n\
	\n\
public slots:\n\
	void slotDaoQt( void*, const QString&, const DaoQtMessage &m ){ slotDao( m ); }\n\
	void slotDaoDao( void *o, void *s, const DaoQtMessage &m ){\n\
		if( o == anySender && s == daoSignal ) slotDao( m );\n\
	}\n\
	void slotDao( const DaoQtMessage &m ){\n\
		DaoVmProcess *vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );\n\
		DValue obj = {DAO_OBJECT,0,0,0,{0}};\n\
		if( daoSlot.t < DAO_METAROUTINE || daoSlot.t > DAO_FUNCTION ) return;\n\
		obj.v.object = DaoCData_GetObject( daoReceiver );\n\
		if( obj.v.object == NULL ) obj.t = 0;\n\
		DaoVmProcess_Call( vmp, (DaoMethod*)daoSlot.v.p, &obj, (DValue**)m.p2, m.count );\n\
	}\n\
};\n\
\n\
class DaoQtObject : public QObjectUserData\n\
{\n\
	public:\n\
	~DaoQtObject(){ for(int i=0,n=daoSlots.size(); i<n; i++) delete daoSlots[i]; }\n\
	\n\
	QList<DaoQtSlot*>  daoSlots;\n\
	\n\
	DaoCData  *cdata;\n\
	\n\
	void Init( DaoCData *d ){ cdata = d; }\n\
	static unsigned int RotatingHash( const QByteArray & key ){\n\
		int i, n = key.size();\n\
		unsigned long long hash = n;\n\
		for(i=0; i<n; i++) hash = ((hash<<4)^(hash>>28)^key[i])&0x7fffffff;\n\
		return hash % 997;\n\
	}\n\
	DaoQtSlot* Find( void *sender, const QString & signal, DValue & slot ){\n\
		int i, n = daoSlots.size();\n\
		for(i=0; i<n; i++){\n\
			DaoQtSlot *daoslot = daoSlots[i];\n\
			if( daoslot->Match( sender, signal, slot ) ) return daoslot;\n\
		}\n\
		return NULL;\n\
	}\n\
	DaoQtSlot* Find( void *sender, void *signal, DValue & slot ){\n\
		int i, n = daoSlots.size();\n\
		for(i=0; i<n; i++){\n\
			DaoQtSlot *daoslot = daoSlots[i];\n\
			if( daoslot->Match( sender, signal, slot ) ) return daoslot;\n\
		}\n\
		return NULL;\n\
	}\n\
	DaoQtSlot *Add( void *sender, const QString & signal, DValue & slot ){\n\
		DaoQtSlot *daoslot = new DaoQtSlot( sender, cdata, signal, slot );\n\
		daoSlots.append( daoslot );\n\
		return daoslot;\n\
	}\n\
	DaoQtSlot *Add( void *sender, void *signal, DValue & slot ){\n\
		DaoQtSlot *daoslot = new DaoQtSlot( sender, cdata, signal, slot );\n\
		daoSlots.append( daoslot );\n\
		return daoslot;\n\
	}\n\
};\n";


const string daoss_class =
"\n\
class DAO_DLL_$(module) DaoSS_$(class) : $(ss_parents)\n\
{ Q_OBJECT\n\
public:\n\
	DaoSS_$(class)() $(init_parents) {}\n\
\n\
$(Emit)\n\
\n\
public slots:\n\
$(slots)\n\
\n\
signals:\n\
$(signalDao)\n\
$(signals)\n\
};\n";

const string qt_Emit =
"	void Emit( void *o, void *s, const DaoQtMessage &m ){ emit signalDao( o, s, m ); }\n";

const string qt_signalDao =
"	void signalDao( void *o, void *s, const DaoQtMessage &m );\n\
	void signalDaoQt( void*, const QString&, const DaoQtMessage &m );\n";

const string qt_init = "";

const string qt_make_linker =
"   DaoSS_$(class) *linker = new DaoSS_$(class)();\n\
   setUserData( 0, linker );\n\
   linker->Init( cdata );\n";

const string qt_make_linker3 =
"  DaoSS_$(class) *linker = new DaoSS_$(class)();\n\
  object->setUserData( 0, linker );\n\
  linker->Init( NULL );\n";

const string qt_virt_emit =
"	virtual void Emit( void *o, void *s, const DaoQtMessage &m ){}\n";

const string qt_connect_decl =
"static void dao_QObject_emit( DaoContext *_ctx, DValue *_p[], int _n );\n\
static void dao_QObject_connect_dao( DaoContext *_ctx, DValue *_p[], int _n );\n";

const string qt_connect_dao =
"  { dao_QObject_emit, \"emit( self : QObject, signal : any, ... )\" },\n\
  { dao_QObject_connect_dao, \"connect( sender : QObject, signal : any, receiver : QObject, slot : any )\" },\n";

const string qt_connect_func =
"static void dao_QObject_emit( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	QObject* self = (QObject*) DValue_CastCData(_p[0], dao_QObject_Typer );\n\
	DaoSS_QObject *linker = (DaoSS_QObject*) self->userData(0);\n\
	DValue *signal = _p[1];\n\
	if( self == NULL || linker == NULL ) return;\n\
  DaoQtMessage msg( _n-2 );\n\
  for(int i=0; i<_n-2; i++) DValue_Copy( msg.p2[i], *_p[i+2] );\n\
	linker->Emit( linker, signal->v.p, msg );\n\
}\n\
static void dao_QObject_connect_dao( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	QObject *sender = (QObject*) DValue_CastCData(_p[0], dao_QObject_Typer );\n\
	QObject *receiver = (QObject*) DValue_CastCData(_p[2], dao_QObject_Typer);\n\
	DaoSS_QObject *senderSS = (DaoSS_QObject*) sender->userData(0);\n\
	DaoSS_QObject *receiverSS = (DaoSS_QObject*) receiver->userData(0);\n\
	DValue signal = *_p[1];\n\
	DValue slot = *_p[3];\n\
	QByteArray s1( \"1\" );\n\
	QByteArray s2( \"2\" );\n\
	QByteArray s3;\n\
	if( sender == NULL || receiver == NULL ) return;\n\
	if( signal.t != DAO_STRING || slot.t != DAO_STRING ){\n\
		if( senderSS == NULL || receiverSS == NULL ) return;\n\
	}\n\
	if( signal.t == DAO_STRING && slot.t == DAO_STRING ){ /* Qt -> Qt */\n\
		s1 += DString_GetMBS(slot.v.s);\n\
		s2 += DString_GetMBS(signal.v.s);\n\
		QObject::connect( sender, s2.data(), receiver, s1.data() );\n\
	}else if( signal.t == DAO_STRING ){ /* Qt -> Dao */\n\
		QByteArray name( DString_GetMBS(signal.v.s) );\n\
		QByteArray size = QString::number( DaoQtObject::RotatingHash( name ) ).toLocal8Bit();\n\
		QByteArray ssname( name ); \n\
		int i = name.indexOf( \'(\' );\n\
		if( i>=0 ) ssname = name.mid( 0, i ) + size;\n\
		s2 += name;\n\
		s1 += \"slot_\" + ssname + name.mid(i);\n\
		s3 += \"2signal_\" + ssname + \"(void*,const QString&,const DaoQtMessage&)\";\n\
		/* signal -> daoqt_signal -> slotDaoQt -> dao */\n\
		QObject::connect( sender, s2.data(), senderSS, s1.data() );\n\
		DaoQtSlot *daoSlot = receiverSS->Find( senderSS, name, slot );\n\
		if( daoSlot == NULL ){\n\
			daoSlot = receiverSS->Add( senderSS, name, slot );\n\
			s3 = \"2signal_\" + ssname + \"(const DaoQtMessage&)\";\n\
			QObject::connect( senderSS, s3.data(), daoSlot, SLOT( slotDao(const DaoQtMessage&) ) );\n\
		}\n\
	}else if( slot.t == DAO_STRING ){ /* Dao -> Qt */\n\
		QByteArray name( DString_GetMBS(slot.v.s) );\n\
		QByteArray size = QString::number( DaoQtObject::RotatingHash( name ) ).toLocal8Bit();\n\
		QByteArray ssname( name ); \n\
		int i = name.indexOf( \'(\' );\n\
		if( i>=0 ) ssname = name.mid( 0, i ) + size;\n\
		s1 += name;\n\
		s2 += \"signal_\" + ssname + name.mid(i);\n\
		s3 += \"1slot_\" + ssname + \"(void*,void*,const DaoQtMessage&)\";\n\
		/* signalDaoQt -> daoqt_slot -> slot */\n\
		QObject::connect( senderSS, SIGNAL( signalDao(void*,void*,const DaoQtMessage&) ),\n\
				receiverSS, s3.data() );\n\
		QObject::connect( receiverSS, s2.data(), receiver, s1.data() );\n\
	}else{ /* Dao -> Dao */\n\
		DaoQtSlot *daoSlot = receiverSS->Find( senderSS, signal.v.p, slot );\n\
		if( daoSlot == NULL ){\n\
			daoSlot = receiverSS->Add( senderSS, signal.v.p, slot );\n\
			QObject::connect( senderSS, SIGNAL( signalDao(void*,void*,const DaoQtMessage&) ),\n\
					daoSlot, SLOT( slotDaoDao(void*,void*,const DaoQtMessage&) ) );\n\
		}\n\
	}\n\
}\n";

const string qt_qstringlist_decl =
"static void dao_QStringList_fromDaoList( DaoContext *_ctx, DValue *_p[], int _n );\n\
static void dao_QStringList_toDaoList( DaoContext *_ctx, DValue *_p[], int _n );\n";

const string qt_qstringlist_dao =
"  { dao_QStringList_fromDaoList, \"QStringList( dslist : list<string> )=>QStringList\" },\n\
  { dao_QStringList_toDaoList, \"toDaoList( self : QStringList )=>list<string>\" },\n";

const string qt_qstringlist_func =
"static void dao_QStringList_fromDaoList( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	QStringList *_self = new QStringList();\n\
	DaoList *dlist = _p[0]->v.list;\n\
	int i, m = DaoList_Size( dlist );\n\
	DaoContext_PutCData( _ctx, _self, dao_QStringList_Typer );\n\
	for(i=0; i<m; i++){\n\
		DValue it = DaoList_GetItem( dlist, i );\n\
		if( it.t != DAO_STRING ) continue;\n\
		_self->append( QString( DString_GetMBS( it.v.s ) ) );\n\
	}\n\
}\n\
static void dao_QStringList_toDaoList( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	QStringList* self= (QStringList*) DValue_CastCData( _p[0], dao_QStringList_Typer );\n\
	DaoList *dlist = DaoContext_PutList( _ctx );\n\
	DValue it = DValue_NewMBString( \"\", 0 );\n\
	int i, m = self->size();\n\
	for(i=0; i<m; i++){\n\
		DString_SetMBS( it.v.s, (*self)[i].toLocal8Bit().data() );\n\
		DaoList_PushBack( dlist, it );\n\
	}\n\
	DValue_Clear( & it );\n\
}\n";

const string qt_qobject_cast_decl =
"static void dao_$(host)_qobject_cast( DaoContext *_ctx, DValue *_p[], int _n );\n";

const string qt_qobject_cast_dao =
"  { dao_$(host)_qobject_cast, \"qobject_cast( object : QObject )=>$(host)\" },\n";

const string qt_qobject_cast_func =
"static void dao_$(host)_qobject_cast( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
  QObject *from = (QObject*)DValue_CastCData( _p[0], dao_QObject_Typer );\n\
  $(host) *to = qobject_cast<$(host)*>( from );\n\
  DaoContext_WrapCData( _ctx, to, dao_$(host)_Typer );\n\
}\n";

const string qt_qobject_cast_func2 =
"static void dao_$(host)_qobject_cast( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
  QObject *from = (QObject*)DValue_CastCData(_p[0], dao_QObject_Typer);\n\
  DaoBase *to = DaoQt_Get_Wrapper( from );\n\
  if( to ){\n\
    DaoContext_PutResult( _ctx, to );\n\
    return;\n\
  }\n\
  $(host) *to2 = qobject_cast<$(host)*>( from );\n\
  DaoContext_WrapCData( _ctx, to2, dao_$(host)_Typer );\n\
}\n";

const string qt_application_decl =
"static void dao_QApplication_DaoApp( DaoContext *_ctx, DValue *_p[], int _n );\n";

const string qt_application_dao =
"  { dao_QApplication_DaoApp, \"QApplication( name : string )=>QApplication\" },\n";

const string qt_application_func =
"static void dao_QApplication_DaoApp( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
  QApplication *app = (QApplication*) QApplication::instance();\n\
  if( app ){\n\
    DaoContext_WrapCData( _ctx, app, dao_QApplication_Typer );\n\
    return;\n\
  }\n\
  static int argc = 1;\n\
  static DString *str = DString_New(1);\n\
  static char* argv = (char*)DValue_GetMBString( _p[0] );\n\
  DString_SetMBS( str, argv );\n\
  argv = DString_GetMBS( str );\n\
  DaoCxx_QApplication *_self = DaoCxx_QApplication_New( argc, & argv, QT_VERSION );\n\
  DaoContext_PutResult( _ctx, (DaoBase*) _self->cdata );\n\
}\n";

const string qt_qstream_decl =
"static void dao_QTextStream_Write1( DaoContext *_ctx, DValue *_p[], int _n );\n\
static void dao_QTextStream_Write2( DaoContext *_ctx, DValue *_p[], int _n );\n\
static void dao_QTextStream_Write3( DaoContext *_ctx, DValue *_p[], int _n );\n\
static void dao_QTextStream_Write4( DaoContext *_ctx, DValue *_p[], int _n );\n\
static void dao_QTextStream_Write5( DaoContext *_ctx, DValue *_p[], int _n );\n";

const string qt_qstream_dao =
"  { dao_QTextStream_Write1, \"write( self : QTextStream, data : int )=>QTextStream\" },\n\
  { dao_QTextStream_Write2, \"write( self : QTextStream, data : float )=>QTextStream\" },\n\
  { dao_QTextStream_Write3, \"write( self : QTextStream, data : double )=>QTextStream\" },\n\
  { dao_QTextStream_Write4, \"write( self : QTextStream, data : string )=>QTextStream\" },\n\
  { dao_QTextStream_Write5, \"write( self : QTextStream, data : any )=>QTextStream\" },\n";

const string qt_qstream_func =
"static void dao_QTextStream_Write1( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
  QTextStream *self = (QTextStream*) DValue_CastCData( _p[0], dao_QTextStream_Typer );\n\
  *self << _p[1]->v.i;\n\
  DaoContext_PutResult( _ctx, (DaoBase*) _p[0]->v.cdata );\n\
}\n\
static void dao_QTextStream_Write2( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
  QTextStream *self = (QTextStream*) DValue_CastCData( _p[0], dao_QTextStream_Typer );\n\
  *self << _p[1]->v.f;\n\
  DaoContext_PutResult( _ctx, (DaoBase*) _p[0]->v.cdata );\n\
}\n\
static void dao_QTextStream_Write3( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
  QTextStream *self = (QTextStream*) DValue_CastCData( _p[0], dao_QTextStream_Typer );\n\
  *self << _p[1]->v.d;\n\
  DaoContext_PutResult( _ctx, (DaoBase*) _p[0]->v.cdata );\n\
}\n\
static void dao_QTextStream_Write4( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
  QTextStream *self = (QTextStream*) DValue_CastCData( _p[0], dao_QTextStream_Typer );\n\
  *self << DValue_GetMBString( _p[1] );\n\
  DaoContext_PutResult( _ctx, (DaoBase*) _p[0]->v.cdata );\n\
}\n\
static void dao_QTextStream_Write5( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
  QTextStream *self = (QTextStream*) DValue_CastCData( _p[0], dao_QTextStream_Typer );\n\
  *self << _p[1]->v.p;\n\
  DaoContext_PutResult( _ctx, (DaoBase*) _p[0]->v.cdata );\n\
}\n";

const string dao_add_extrc_decl =
"static void dao_$(host)_add_extrc( DaoContext *_ctx, DValue *_p[], int _n );\n";

const string dao_add_extrc_dao =
"  { dao_$(host)_add_extrc, \"dao_add_external_reference( self : $(host) )\" },\n";

const string dao_add_extrc_func =
"static void dao_$(host)_add_extrc( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	DaoCxx_$(host) *self = (DaoCxx_$(host)*)DValue_GetCData(_p[0]);\n\
	self->DaoAddExternalReference();\n\
}\n";

const string dao_proto =
"  { dao_$(host_idname)_$(cxxname)$(overload), \"$(daoname)( $(parlist) )$(retype)\" },\n";

const string cxx_wrap_proto 
= "static void dao_$(host_idname)_$(cxxname)$(overload)( DaoContext *_ctx, DValue *_p[], int _n )";

const string tpl_struct = "$(qname)* DAO_DLL_$(module) Dao_$(idname)_New();\n";

const string tpl_struct_alloc =
"$(qname)* Dao_$(idname)_New()\n{\n\
	$(qname) *self = calloc( 1, sizeof($(idname)) );\n\
	return self;\n\
}\n";
const string tpl_struct_alloc2 =
"$(host_qname)* Dao_$(host_idname)_New()\n{\n\
	$(host_qname) *self = new $(host_qname)();\n\
	return self;\n\
}\n";

const string tpl_struct_daoc = 
"typedef struct Dao_$(idname) Dao_$(idname);\n\
struct DAO_DLL_$(module) Dao_$(idname)\n\
{\n\
	$(qname)  nested;\n\
	$(qname) *object;\n\
	DaoCData *cdata;\n\
};\n\
Dao_$(idname)* DAO_DLL_$(module) Dao_$(idname)_New();\n";

const string tpl_struct_daoc_alloc =
"Dao_$(idname)* Dao_$(idname)_New()\n\
{\n\
	Dao_$(idname) *wrap = calloc( 1, sizeof(Dao_$(idname)) );\n\
	$(name) *self = ($(name)*) wrap;\n\
	wrap->cdata = DaoCData_New( dao_$(idname)_Typer, wrap );\n\
	wrap->object = self;\n\
$(callbacks)$(selfields)\treturn wrap;\n\
}\n";

const string tpl_set_callback = "\tself->$(callback) = Dao_$(name)_$(callback);\n";
const string tpl_set_selfield = "\tself->$(field) = wrap;\n";

const string c_wrap_new = 
"static void dao_$(host_idname)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	Dao_$(host_idname) *self = Dao_$(host_idname)_New();\n\
	DaoContext_PutResult( _ctx, (DaoBase*) self->cdata );\n\
}\n";
const string cxx_wrap_new = 
"static void dao_$(host_idname)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	DaoCxx_$(host_idname) *self = DaoCxx_$(host_idname)_New();\n\
	DaoContext_PutResult( _ctx, (DaoBase*) self->cdata );\n\
}\n";
const string cxx_wrap_alloc2 = 
"static void dao_$(host_idname)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	$(host_qname) *self = Dao_$(host_idname)_New();\n\
	DaoContext_PutCData( _ctx, self, dao_$(host_idname)_Typer );\n\
}\n";

const string tpl_class_def = 
"class DAO_DLL_$(module) DaoCxxVirt_$(idname) $(virt_supers)\n{\n\
	public:\n\
	DaoCxxVirt_$(idname)(){ self = 0; cdata = 0; }\n\
	void DaoInitWrapper( $(qname) *self, DaoCData *d );\n\n\
	$(qname) *self;\n\
	DaoCData *cdata;\n\
\n$(virtuals)\n\
$(qt_virt_emit)\n\
};\n\
class DAO_DLL_$(module) DaoCxx_$(idname) : public $(qname), public DaoCxxVirt_$(idname)\n\
{ $(Q_OBJECT)\n\n\
\tpublic:\n\
$(constructors)\n\
	~DaoCxx_$(idname)();\n\
	void DaoInitWrapper();\n\n\
$(methods)\n\
};\n";

const string tpl_class2 = 
tpl_class_def + "$(class)* Dao_$(idname)_Copy( const $(qname) &p );\n";

const string tpl_class_init =
"void DaoCxxVirt_$(idname)::DaoInitWrapper( $(qname) *s, DaoCData *d )\n\
{\n\
	self = s;\n\
	cdata = d;\n\
$(init_supers)\n\
$(qt_init)\n\
}\n\
DaoCxx_$(idname)::~DaoCxx_$(idname)()\n\
{\n\
	if( cdata ){\n\
		DaoCData_SetData( cdata, NULL );\n\
		DaoCData_SetExtReference( cdata, 0 );\n\
	} \n\
}\n\
void DaoCxx_$(idname)::DaoInitWrapper()\n\
{\n\
	cdata = DaoCData_New( dao_$(idname)_Typer, this );\n\
	DaoCxxVirt_$(idname)::DaoInitWrapper( this, cdata );\n\
$(qt_make_linker)\n\
}\n";
const string tpl_class_init_qtss = 
"void DAO_DLL_$(module) Dao_$(idname)_InitSS( $(qname) *p )\n\
{\n\
   if( p->userData(0) == NULL ){\n\
		DaoSS_$(idname) *linker = new DaoSS_$(idname)();\n\
		p->setUserData( 0, linker );\n\
		linker->Init( NULL );\n\
	}\n\
}\n";
const string tpl_class_copy = tpl_class_init +
"$(qname)* DAO_DLL_$(module) Dao_$(idname)_Copy( const $(qname) &p )\n\
{\n\
	$(qname) *object = new $(qname)( p );\n\
$(qt_make_linker3)\n\
	return object;\n\
}\n";
const string tpl_class_decl_constru = 
"	DaoCxx_$(idname)( $(parlist) ) : $(qname)( $(parcall) ){}\n";

const string tpl_class_new =
"\nDaoCxx_$(idname)* DAO_DLL_$(module) DaoCxx_$(idname)_New( $(parlist) );\n";
const string tpl_class_new_novirt =
"\n$(qname)* DAO_DLL_$(module) Dao_$(idname)_New( $(parlist) );\n";
const string tpl_class_init_qtss_decl =
"\nvoid DAO_DLL_$(module) Dao_$(idname)_InitSS( $(qname) *p );\n";

const string tpl_class_noderive =
"\n$(qname)* DAO_DLL_$(module) Dao_$(idname)_New( $(parlist) )\n\
{\n\
	$(qname) *object = new $(qname)( $(parcall) );\n\
$(qt_make_linker3)\n\
	return object;\n\
}\n";
const string tpl_class_init2 =
"\nDaoCxx_$(idname)* DAO_DLL_$(module) DaoCxx_$(idname)_New( $(parlist) )\n\
{\n\
	DaoCxx_$(idname) *self = new DaoCxx_$(idname)( $(parcall) );\n\
	self->DaoInitWrapper();\n\
	return self;\n\
}\n";

const string tpl_init_super = "\tDaoCxxVirt_$(super)::DaoInitWrapper( s, d );\n";

const string tpl_meth_decl =
"\t$(retype) $(name)( int &_cs$(comma) $(parlist) )$(extra);\n";

const string tpl_meth_decl2 =
"\t$(retype) $(name)( $(parlist) )$(extra);\n";

const string tpl_meth_decl3 =
"$(retype) DaoWrap_$(name)( $(parlist) )$(extra){ $(return)$(type)::$(cxxname)( $(parcall) ); }\n";

const string tpl_raise_call_protected =
"  if( _p[0]->t == DAO_CDATA && DaoCData_GetObject( _p[0]->v.cdata ) == NULL ){\n\
    DaoContext_RaiseException( _ctx, DAO_ERROR, \"call to protected method\" );\n\
    return;\n\
  }\n";

const string numlist_code = 
"\n\nstatic DaoNumItem dao_$(typer)_Nums[] = \n\
{\n$(nums)  { NULL, 0, 0 }\n};\n";

const string methlist_code = numlist_code +
"\n\n$(decls)\n\
static DaoFuncItem dao_$(typer)_Meths[] = \n\
{\n$(meths)  { NULL, NULL }\n};\n";

const string methlist_code2 = 
"\n\n$(decls)\n";

const string delete_struct = 
"static void Dao_$(typer)_Delete( void *self )\n\
{\n\
	free( self );\n\
}\n";

const string delete_class = 
"static void Dao_$(typer)_Delete( void *self )\n\
{\n\
	$(comment)delete ($(qname)*) self;\n\
}\n";

const string delete_test =
"static int Dao_$(typer)_DelTest( void *self0 )\n\
{\n\
	$(qname) *self = ($(qname)*) self0;\n\
	return ($(condition));\n\
}\n";

const string cast_to_parent = 
"void* dao_cast_$(typer)_to_$(parent2)( void *data )\n\
{\n\
	return ($(parent)*)($(child)*)data;\n\
}\n";

const string usertype_code =
"$(cast_funcs)\n\
static DaoTypeBase $(typer)_Typer = \n\
{ \"$(type)\", NULL,\n\
  dao_$(typer)_Nums,\n\
  dao_$(typer)_Meths,\n\
  { $(parents)NULL },\n\
  { $(casts)NULL },\n\
  $(delete),\n\
  $(deltest)\n\
};\n\
DaoTypeBase DAO_DLL_$(module) *dao_$(typer)_Typer = & $(typer)_Typer;\n";

const string usertype_code_none =
"static DaoTypeBase $(typer)_Typer = \n\
{ \"$(type)\", NULL, NULL, NULL, { NULL }, { NULL }, NULL, NULL };\n\
DaoTypeBase DAO_DLL_$(module) *dao_$(typer)_Typer = & $(typer)_Typer;\n";

//const string usertype_code_none = methlist_code + usertype_code;
const string usertype_code_struct = methlist_code + delete_struct + usertype_code;
const string usertype_code_class = methlist_code + delete_class + usertype_code;
const string usertype_code_class2 = methlist_code + delete_class + delete_test + usertype_code;

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );
extern string normalize_type_name( const string & name );

CDaoUserType::CDaoUserType( CDaoModule *mod, RecordDecl *decl )
{
	module = mod;
	isRedundant = true;
	isQObject = isQObjectBase = false;
	wrapType = CDAO_WRAP_TYPE_NONE;
	alloc_default = "NULL";
	this->decl = NULL;
	SetDeclaration( decl );
}
void CDaoUserType::SetDeclaration( RecordDecl *decl )
{
	this->decl = decl;
	qname = CDaoModule::GetQName( decl );
	idname = CDaoModule::GetIdName( decl );
	name = decl->getNameAsString();
}
// For template class type:
// The canonical types of some template classes has quite long qualified names,
// to make the generated codes a little bit more pleasant, we use the written
// name in source codes and the canonical name to make a simplified qualified
// name that can still uniquely identify the type.
void CDaoUserType::UpdateName( const string & writtenName )
{
	if( writtenName.size() ){
		outs() << writtenName << ":  " << name << " " << qname << " " << idname << "\n";
		qname.replace( qname.rfind( name ), name.size(), writtenName );
		name = normalize_type_name( writtenName );
		qname = normalize_type_name( qname );
		idname = cdao_qname_to_idname( qname );
		outs() << writtenName << ":  " << name << " " << qname << " " << idname << "\n";
	}

}
bool CDaoUserType::IsFromMainModule()
{
	if( decl == NULL ) return false;
	RecordDecl *dd = decl->getDefinition();
	if( dd == NULL ) return module->IsFromMainModule( decl->getLocation() );
	return module->IsFromMainModule( dd->getLocation() );
}
string CDaoUserType::GetInputFile()const
{
	if( decl == NULL ) return "";
	RecordDecl *dd = decl->getDefinition();
	if( dd == NULL ) return module->GetFileName( decl->getLocation() );
	return module->GetFileName( dd->getLocation() );
}
void CDaoUserType::Clear()
{
	type_decls.clear();
	type_codes.clear();
	meth_decls.clear();
	meth_codes.clear();
	dao_meths.clear();
	alloc_default.clear();
	cxxWrapperVirt.clear();
	typer_codes.clear();
	pureVirtuals.clear();
}
int CDaoUserType::GenerateSimpleTyper()
{
	map<string,string> kvmap;
	kvmap[ "module" ] = UppercaseString( module->moduleInfo.name );
	kvmap[ "typer" ] = idname;
	kvmap[ "type" ] = name;
	typer_codes = cdao_string_fill( usertype_code_none, kvmap );
	wrapType = CDAO_WRAP_TYPE_OPAQUE;
	return 0;
}
int CDaoUserType::Generate()
{
	RecordDecl *cc = (RecordDecl*) decl->getCanonicalDecl();
	RecordDecl *dd = decl->getDefinition();
	SourceLocation loc = decl->getLocation();
	if( cc ) loc = cc->getLocation();
	if( dd ) loc = dd->getLocation();

	ClassTemplateSpecializationDecl *SD, *DE;
	if( (SD = dyn_cast<ClassTemplateSpecializationDecl>(decl)) ){
		DE = cast_or_null<ClassTemplateSpecializationDecl>( SD->getDefinition());
		if( DE ) loc = DE->getPointOfInstantiation();
		dd = DE;
	}
	if( module->finalGenerating == false && dd == NULL ) return 0;

	// ignore redundant declarations:
	isRedundant = dd != NULL && dd != decl;
	if( isRedundant ) return 0;
	if( dd == NULL && wrapType == CDAO_WRAP_TYPE_OPAQUE ) return 0;
	if( dd == decl && wrapType >= CDAO_WRAP_TYPE_MEMBER ) return 0;
	Clear();
	// simplest wrapping for types declared or defined outsided of the modules:
	if( not module->IsFromModules( loc ) ) return GenerateSimpleTyper();

	// simplest wrapping for types declared but not defined:
	if( dd == NULL ) return GenerateSimpleTyper();
	if (CXXRecordDecl *record = dyn_cast<CXXRecordDecl>(decl)) return Generate( record );
	return Generate( decl );
}
void CDaoUserType::SetupDefaultMapping( map<string,string> & kvmap )
{
	kvmap[ "module" ] = UppercaseString( module->moduleInfo.name );
	kvmap[ "host_qname" ] = qname;
	kvmap[ "host_idname" ] = idname;
	kvmap[ "qname" ] = qname;
	kvmap[ "idname" ] = idname;
	kvmap[ "cxxname" ] = name;
	kvmap[ "daoname" ] = name;
	kvmap[ "name" ] = name;
	kvmap[ "class" ] = name;
	kvmap[ "type" ] = name;

	kvmap[ "retype" ] = "=>" + name;
	kvmap[ "overload" ] = "";
	kvmap[ "virt_supers" ] = "";
	kvmap[ "supers" ] = "";
	kvmap[ "virtuals" ] = "";
	kvmap[ "methods" ] = "";
	kvmap[ "init_supers" ] = "";
	kvmap[ "parlist" ] = "";
	kvmap[ "parcall" ] = "";

	kvmap[ "Q_OBJECT" ] = "";
	kvmap[ "qt_signal_slot" ] = "";
	kvmap[ "qt_init" ] = "";
	kvmap[ "ss_parents" ] = "public QObject";
	kvmap[ "init_parents" ] = "";
	kvmap[ "qt_make_linker" ] = "";
	kvmap[ "qt_virt_emit" ] = "";
	kvmap[ "qt_make_linker3" ] = "";

	kvmap["signals"] = "";
	kvmap["slots"] = "";
	kvmap["member_wrap"] = "";
	kvmap["set_wrap"] = "";
	kvmap["Emit"] = "";
	kvmap["slotDaoQt"] = "";
	kvmap["signalDao"] = "";

	kvmap[ "typer" ] = idname;
	kvmap[ "child" ] = qname;
	kvmap[ "parent" ] = "";
	kvmap[ "parent2" ] = "";
	kvmap[ "decls" ] = "";
	kvmap[ "nums" ] = "";
	kvmap[ "meths" ] = "";
	kvmap[ "parents" ] = "";
	kvmap[ "casts" ] = "";
	kvmap[ "cast_funcs" ] = "";
	kvmap[ "alloc" ] = alloc_default;
	kvmap[ "delete" ] = "NULL";
	kvmap[ "if_parent" ] = "";
	kvmap[ "deltest" ] = "NULL";
	kvmap[ "comment" ] = "";
	kvmap["delete"] = "Dao_" + idname + "_Delete";

}
int CDaoUserType::Generate( RecordDecl *decl )
{
	int i, j, n, m;
	vector<CDaoFunction> callbacks;
	map<string,string> kvmap;

	wrapType = CDAO_WRAP_TYPE_STRUCT;

	SetupDefaultMapping( kvmap );

	outs() << name << " CDaoUserType::Generate( RecordDecl *decl )\n";
	RecordDecl::field_iterator fit, fend;
	for(fit=decl->field_begin(),fend=decl->field_end(); fit!=fend; fit++){
		const Type *type = fit->getTypeSourceInfo()->getType().getTypePtr();
		const PointerType *pt = dyn_cast<PointerType>( type );
		if( pt == NULL ) continue;
		const Type *pt2 = pt->getPointeeType().getTypePtr();
		const ParenType *pt3 = dyn_cast<ParenType>( pt2 );
		if( pt3 == NULL ) continue;
		const Type *pt4 = pt3->getInnerType().getTypePtr();
		const FunctionProtoType *ft = dyn_cast<FunctionProtoType>( pt4 );
		if( ft == NULL ) continue;
		callbacks.push_back( CDaoFunction( module ) );
		callbacks.back().SetCallback( (FunctionProtoType*) ft, *fit );
	}
	kvmap[ "parlist" ] = "";
	kvmap[ "retype" ] = "=>" + name;
	kvmap[ "overload" ] = "";

	dao_meths += cdao_string_fill( dao_proto, kvmap );
	meth_decls += cdao_string_fill( cxx_wrap_proto, kvmap ) + ";\n";
	meth_codes += cdao_string_fill( c_wrap_new, kvmap );
	if( callbacks.size() == 0 ){
		type_decls = cdao_string_fill( tpl_struct, kvmap );
		type_codes = cdao_string_fill( tpl_struct_alloc, kvmap );
		return 0;
	}

	string set_callbacks;
	string set_fields;
	kvmap[ "callback" ] = "";
	for(i=0,n=callbacks.size(); i<n; i++){
		CDaoFunction & meth = callbacks[i];
		meth.Generate();
		if( meth.excluded ) continue;
		kvmap[ "callback" ] = meth.cxxName;
		set_callbacks += cdao_string_fill( tpl_set_callback, kvmap );
		type_decls += meth.cxxWrapperVirtProto;
		type_codes += meth.cxxWrapperVirt;
		CDaoProxyFunction::Use( meth.signature2 );
	}
	kvmap[ "field" ] = "";
#if 0
	for( f in selfields.keys() ){
		kvmap[ 'field' ] = f;
		setfields += tpl_set_selfield.expand( kvmap );
	}
	kvmap = { 'name'=>name, 'callbacks'=>callbacks, 'selfields'=>setfields };
#endif
	kvmap[ "callbacks" ] = set_callbacks;
	kvmap[ "selfields" ] = set_fields;
	type_decls += cdao_string_fill( tpl_struct_daoc, kvmap );
	type_codes += cdao_string_fill( tpl_struct_daoc_alloc, kvmap );


	kvmap[ "decls" ] = meth_decls;
	kvmap[ "meths" ] = dao_meths;
#if 0
	if( del_tests.has( utp.condType ) ){
		kvmap["condition"] = del_tests[ utp.condType ];
		kvmap["deltest"] = "Dao_" + utp.name + "_DelTest";
		return usertype_code_class2.expand( kvmap );
	}
#endif
	typer_codes = cdao_string_fill( usertype_code_class, kvmap );
	return 0;
}

int CDaoUserType::Generate( CXXRecordDecl *decl )
{
	int i, j, n, m;
	bool hasVirtual = false; // protected or public virtual function;
	bool hasProtected = false;
	Preprocessor & pp = module->compiler->getPreprocessor();
	SourceManager & sm = module->compiler->getSourceManager();
	map<string,string> kvmap;
	map<string,int> overloads;

	wrapType = CDAO_WRAP_TYPE_MEMBER;

	vector<VarDecl*>      vars;
	vector<EnumDecl*>     enums;
	vector<CDaoUserType*> bases;
	vector<CDaoFunction>  methods;
	vector<CDaoFunction>  constructors;

	vector<CDaoFunction*> pubslots;
	vector<CDaoFunction*> protsignals;

	string daoc_supers;
	string virt_supers;
	string init_supers;
	string ss_supers;
	string ss_init_sup;
	string class_new;
	string class_decl;

	map<CXXMethodDecl*,int>  pvmeths;
	map<CXXMethodDecl*,int>::iterator pvit, pvend;
	map<const RecordDecl*,CDaoUserType*>::iterator find;
	CXXRecordDecl::base_class_iterator baseit, baseend = decl->bases_end();
	for(baseit=decl->bases_begin(); baseit != baseend; baseit++){
		CXXRecordDecl *p = baseit->getType().getTypePtr()->getAsCXXRecordDecl();
		find = module->allUsertypes.find( p );
		if( find == module->allUsertypes.end() ) continue;

		CDaoUserType & sup = *find->second;
		string supname = sup.GetName();
		bases.push_back( & sup );
		//outs() << "parent: " << p->getNameAsString() << "\n";

		if( sup.wrapType != CDAO_WRAP_TYPE_STRUCT ) continue;
		for(i=0,n=sup.pureVirtuals.size(); i<n; i++) pvmeths[ sup.pureVirtuals[i] ] = 1;
		kvmap[ "super" ] = supname;
		if( virt_supers.size() ){
			daoc_supers += ',';
			virt_supers += ',';
		}
		daoc_supers += " public DaoCxx_" + supname;
		virt_supers += " public DaoCxxVirt_" + supname;
		init_supers += cdao_string_fill( tpl_init_super, kvmap );
		if( sup.isQObject ){
			if( ss_supers.size() ){
				ss_supers += ',';
				ss_init_sup += ',';
			}
			ss_supers += " public DaoSS_" + supname;
			ss_init_sup += " DaoSS_" + supname + "()";
		}
	}
	CXXRecordDecl::decl_iterator dit, dend;
	for(dit=decl->decls_begin(),dend=decl->decls_end(); dit!=dend; dit++){
		EnumDecl *edec = dyn_cast<EnumDecl>( *dit );
		if( edec == NULL || dit->getAccess() != AS_public ) continue;
		enums.push_back( edec );
	}
	CXXRecordDecl::field_iterator fit, fend;
	for(fit=decl->field_begin(),fend=decl->field_end(); fit!=fend; fit++){
		const Type *type = fit->getTypeSourceInfo()->getType().getTypePtr();
		const PointerType *pt = dyn_cast<PointerType>( type );
		if( pt == NULL ) continue;
		const Type *pt2 = pt->getPointeeType().getTypePtr();
		const ParenType *pt3 = dyn_cast<ParenType>( pt2 );
		if( pt3 == NULL ) continue;
		const Type *pt4 = pt3->getInnerType().getTypePtr();
		const FunctionProtoType *ft = dyn_cast<FunctionProtoType>( pt4 );
		if( ft == NULL ) continue;
		// XXX
	}
	CXXRecordDecl::method_iterator methit, methend = decl->method_end();
	for(methit=decl->method_begin(); methit!=methend; methit++){
		string name = methit->getNameAsString();
		//outs() << name << ": " << methit->getAccess() << " ......................\n";
		//outs() << methit->getType().getAsString() << "\n";
		if( methit->isImplicit() ) continue;
		if( methit->getAccess() == AS_private && ! methit->isPure() ) continue;
		if( methit->isVirtual() ) hasVirtual = true;
		if( dyn_cast<CXXDestructorDecl>( *methit ) ) continue;
		if( methit->getAccess() == AS_protected ) hasProtected = true;
		methods.push_back( CDaoFunction( module, *methit, ++overloads[name] ) );
		if( methit->isPure() ) pvmeths[ *methit ] = 1;
		if( methit->isVirtual() ){
			CXXMethodDecl::method_iterator it2, end2 = methit->end_overridden_methods();
			for(it2=methit->begin_overridden_methods(); it2!=end2; it2++)
				pvmeths.erase( (CXXMethodDecl*)*it2 );
		}
	}

	bool has_ctor = false;
	bool has_public_ctor = false;
	bool has_protected_ctor = false;
	bool has_private_ctor = false;
	bool has_private_ctor_only = false;
	bool has_explicit_default_ctor = false;
	bool has_implicit_default_ctor = true;
	bool has_public_destroctor = true;
	bool has_non_public_copy_ctor = false;
	CXXRecordDecl::ctor_iterator ctorit, ctorend = decl->ctor_end();
	for(ctorit=decl->ctor_begin(); ctorit!=ctorend; ctorit++){
		string name = ctorit->getNameAsString();
		//outs() << name << ": " << ctorit->getAccess() << " ......................\n";
		//outs() << ctorit->getType().getAsString() << "\n";
		if( ctorit->isImplicit() ) continue;
		if( not ctorit->isImplicitlyDefined() ){
			has_ctor = true;
			has_implicit_default_ctor = false;
			if( ctorit->getAccess() == AS_private ) has_private_ctor = true;
			if( ctorit->getAccess() == AS_protected ) has_protected_ctor = true;
			if( ctorit->getAccess() == AS_public ) has_public_ctor = true;
			if( ctorit->isCopyConstructor() && ctorit->getAccess() != AS_public )
				has_non_public_copy_ctor = true;
			if( ctorit->param_size() == 0 ){
				has_explicit_default_ctor = true;
				if( ctorit->getAccess() == AS_private ) has_private_ctor_only = true;
			}
		}
		if( ctorit->getAccess() == AS_private ) continue;
		constructors.push_back( CDaoFunction( module, *ctorit, ++overloads[name] ) );
	}
	CXXDestructorDecl *destr = decl->getDestructor();
	if( destr && destr->getAccess() != AS_public ) has_public_destroctor = false;
	if( has_public_ctor || has_protected_ctor ) has_private_ctor_only = false;
	if( has_implicit_default_ctor ) has_public_ctor = true;

	if( isQObject ){
		map<const CXXMethodDecl*,CDaoFunction*> dec2meth;
		for(i=0, n = methods.size(); i<n; i++){
			CDaoFunction & meth = methods[i];
			const CXXMethodDecl *mdec = dyn_cast<CXXMethodDecl>( meth.funcDecl );
			dec2meth[mdec] = & meth;
		}
		bool is_pubslot = false;
		bool is_signal = false;
		for(dit=decl->decls_begin(),dend=decl->decls_end(); dit!=dend; dit++){
			AccessSpecDecl *asdec = dyn_cast<AccessSpecDecl>( *dit );
			if( asdec == NULL ){
				CXXMethodDecl *mdec = dyn_cast<CXXMethodDecl>( *dit );
				if( mdec == NULL ) continue;
				if( dec2meth.find( mdec ) == dec2meth.end() ) continue;
				if( is_pubslot ) pubslots.push_back( dec2meth[mdec] );
				if( is_signal ) protsignals.push_back( dec2meth[mdec] );
				continue;
			}
			is_pubslot = is_signal = false;
			string as = module->ExtractSource( asdec->getSourceRange() );
			is_pubslot = as.find( "public" ) != string::npos and as.find( "slots" ) != string::npos;
			is_signal = as.find( "signals" ) != string::npos;
			//outs() << as << " " << is_pubslot << is_signal << " access\n";
		}
	}
	// Wrapping as derivable type when there is no private-only constructor, and:
	// 1. there is a virtual function, so that it can be re-implemented by derived Dao class;
	// 2. or, there is a protected function, so that it can be accessed by derived Dao class;
	// 3. or, there is a Qt signal/slot, so that such signal/slot can be connected with Dao.
	bool structWrapping = has_private_ctor_only == false;
	structWrapping &= hasVirtual || hasProtected || pubslots.size() || protsignals.size();

	wrapType = structWrapping ? CDAO_WRAP_TYPE_STRUCT : CDAO_WRAP_TYPE_MEMBER;
	if( name == "Fl" ) 
		outs() << name << ": " << (int)wrapType << " " << has_private_ctor_only << "\n";

	SetupDefaultMapping( kvmap );

	for(i=0,n=constructors.size(); i<n; i++){
		CDaoFunction & meth = constructors[i];
		const CXXConstructorDecl *ctor = dyn_cast<CXXConstructorDecl>( meth.funcDecl );
		meth.Generate();
		if( meth.excluded || ctor->getAccess() != AS_public ) continue;
		dao_meths += meth.daoProtoCodes;
		meth_decls += meth.cxxProtoCodes + (meth.cxxProtoCodes.size() ? ";\n" : "");
		meth_codes += meth.cxxWrapper;
	}
	for(i=0,n=methods.size(); i<n; i++){
		CDaoFunction & meth = methods[i];
		const CXXMethodDecl *mdec = dyn_cast<CXXMethodDecl>( meth.funcDecl );
		meth.Generate();
		if( meth.excluded ) continue;
		if( mdec->getAccess() != AS_public ) continue;
		dao_meths += meth.daoProtoCodes;
		meth_decls += meth.cxxProtoCodes + (meth.cxxProtoCodes.size() ? ";\n" : "");
		meth_codes += meth.cxxWrapper;
	}
	if( isQObject ){
		string qt_signals;
		string qt_slots;
		for(i=0, n = pubslots.size(); i<n; i++){
			CDaoFunction *meth = pubslots[i];
			qt_signals += meth->qtSlotSignalDecl;
			qt_slots += meth->qtSlotSlotDecl;
			type_codes += meth->qtSlotSlotCode;
		}
		for(i=0, n = protsignals.size(); i<n; i++){
			CDaoFunction *meth = pubslots[i];
			qt_signals += meth->qtSignalSignalDecl;
			qt_slots += meth->qtSignalSlotDecl;
			type_codes += meth->qtSignalSlotCode;
		}
		kvmap["signals"] = qt_signals;
		kvmap["slots"] = qt_slots;
		kvmap["ss_parents"] = ss_supers;
		if( ss_init_sup.size() ) kvmap["init_parents"] = ":"+ ss_init_sup;
		if( isQObjectBase ){
			kvmap["member_wrap"] = "   " + name + " *wrap;\n";
			kvmap["set_wrap"] = "wrap = w;";
			kvmap["Emit"] = qt_Emit;
			kvmap["signalDao"] = qt_signalDao;
		}
		type_decls += cdao_string_fill( daoss_class, kvmap );
	} // isQObject

	if( has_implicit_default_ctor ){
		dao_meths = cdao_string_fill( dao_proto, kvmap ) + dao_meths;
		meth_decls = cdao_string_fill( cxx_wrap_proto, kvmap ) + ";\n" + meth_decls;
		if( wrapType == CDAO_WRAP_TYPE_MEMBER ){
			meth_codes = cdao_string_fill( cxx_wrap_alloc2, kvmap ) + meth_codes;
			type_decls += cdao_string_fill( tpl_struct, kvmap );
			type_codes += cdao_string_fill( tpl_struct_alloc2, kvmap );
		}else{
			meth_codes = cdao_string_fill( cxx_wrap_new, kvmap ) + meth_codes;
			class_new += cdao_string_fill( tpl_class_new, kvmap );
			type_codes += cdao_string_fill( tpl_class_init2, kvmap );
		}
	}
	kvmap[ "nums" ] = module->MakeConstantItems( enums, vars, qname );
	kvmap[ "decls" ] = meth_decls;
	kvmap[ "meths" ] = dao_meths;
	kvmap["constructors"] = "";
	//outs()<<qname<<": "<<wrapType<<" "<<has_private_ctor_only<<" "<<hasVirtual<<"\n";
	if( wrapType == CDAO_WRAP_TYPE_MEMBER ){
		kvmap[ "class" ] = name;
		kvmap[ "name" ] = name;
		kvmap[ "qt_make_linker3" ] = "";
		if( isQObject ) kvmap["qt_make_linker3"] = cdao_string_fill( qt_make_linker3, kvmap );
		for(i=0, n = constructors.size(); i<n; i++){
			CDaoFunction & func = constructors[i];
			const CXXConstructorDecl *ctor = dyn_cast<CXXConstructorDecl>( func.funcDecl );
			if( func.excluded ) continue;
			if( ctor->getAccess() == AS_protected ) continue;
			kvmap["parlist"] = func.cxxProtoParam;
			kvmap["parcall"] = func.cxxCallParamV;
			type_codes += cdao_string_fill( tpl_class_noderive, kvmap );
			type_decls += cdao_string_fill( tpl_class_new_novirt, kvmap );
		}
		typer_codes = cdao_string_fill( usertype_code_class, kvmap );
		return 0;
	}

	string kmethods;
	string kvirtuals;
	for(pvit=pvmeths.begin(),pvend=pvmeths.end(); pvit!=pvend; pvit++){
		CXXMethodDecl *mdec = pvit->first;
		string name = mdec->getNameAsString();
		bool isconst = mdec->getTypeQualifiers() & DeclSpec::TQ_const;
		pureVirtuals.push_back( mdec );
		if( mdec->getParent() == decl ) continue;

		CDaoFunction func( module, mdec, ++overloads[name] );
		func.Generate();
		if( func.excluded ) continue;
		kvmap[ "name" ] = mdec->getNameAsString();
		kvmap[ "retype" ] = func.retype.cxxtype;
		kvmap[ "parlist" ] = func.cxxProtoParamDecl;
		kvmap[ "extra" ] = isconst ? "const" : "";
		kmethods += cdao_string_fill( tpl_meth_decl2, kvmap );
		string wrapper = func.cxxWrapperVirt2;
		string from = "DaoCxx_" + mdec->getParent()->getNameAsString() + "::";
		string to = "DaoCxx_" + decl->getNameAsString() + "::";
		size_t pos = wrapper.find( from );
		if( pos != string::npos ) wrapper.replace( pos, from.size(), to );
		cxxWrapperVirt += wrapper;
	}
	for(i=0, n=methods.size(); i<n; i++){
		CDaoFunction & meth = methods[i];
		const CXXMethodDecl *mdec = dyn_cast<CXXMethodDecl>( meth.funcDecl );
		bool isconst = mdec->getTypeQualifiers() & DeclSpec::TQ_const;
		if( meth.excluded ){
			if( mdec->isPure() ){
				TypeLoc typeloc = mdec->getTypeSourceInfo()->getTypeLoc();
				string source = module->ExtractSource( typeloc.getSourceRange(), true );
				//module->ExtractSource( mdec->getSourceRange(), true ); //with no return type
				if( isconst ) source += "const";
				kmethods += "\t" + source + "{/*XXX 1*/}\n";
			}
			continue;
		}
		if( dyn_cast<CXXConstructorDecl>( mdec ) ) continue;
		kvmap[ "name" ] = mdec->getNameAsString();
		kvmap[ "retype" ] = meth.retype.cxxtype;
		kvmap[ "parlist" ] = meth.cxxProtoParamDecl;
		kvmap[ "extra" ] = isconst ? "const" : "";
		if( mdec->isVirtual() ){
			kmethods += cdao_string_fill( tpl_meth_decl2, kvmap );
			kvmap[ "parlist" ] = meth.cxxProtoParamVirt;
			kvmap[ "comma" ] = meth.cxxProtoParamVirt.size() ? "," : "";
			kvirtuals += cdao_string_fill( tpl_meth_decl, kvmap );
			cxxWrapperVirt += meth.cxxWrapperVirt2;
		}
		if( mdec->getAccess() == AS_protected && not mdec->isPure() ){
			kvmap[ "type" ] = name;
			kvmap[ "cxxname" ] = mdec->getNameAsString();
			kvmap[ "parlist" ] = meth.cxxProtoParamDecl;
			kvmap[ "parcall" ] = meth.cxxCallParamV;
			kvmap[ "return" ] = kvmap[ "retype" ] == "void" ? "" : "return ";
			string mcode = cdao_string_fill( tpl_meth_decl3, kvmap );
			if( mdec->isStatic() ) mcode = "static " + mcode;
			kmethods += "\t" + mcode;
		}
	}

	if( isQObjectBase ){
		ss_supers += "public QObject, public DaoQtObject";
	}
	if( daoc_supers.size() ) daoc_supers = " :" + daoc_supers;
	if( virt_supers.size() ) virt_supers = " :" + virt_supers;
	if( init_supers.size() ) init_supers += "\n";

	kvmap[ "virt_supers" ] = virt_supers;
	kvmap[ "supers" ] = daoc_supers;
	kvmap[ "virtuals" ] = kvirtuals;
	kvmap[ "methods" ] = kmethods;
	kvmap[ "init_supers" ] = init_supers;
	if( isQObject ){
		kvmap["Q_OBJECT"] = "Q_OBJECT";
		kvmap["qt_init"] = qt_init;
		kvmap["qt_make_linker"] = cdao_string_fill( qt_make_linker, kvmap );
		kvmap["qt_make_linker3"] = cdao_string_fill( qt_make_linker3, kvmap );
		kvmap["qt_virt_emit"] = qt_virt_emit;
	}
	//if( has_ctor ){
		for(i=0, n = constructors.size(); i<n; i++){
			CDaoFunction & ctor = constructors[i];
			const CXXConstructorDecl *cdec = dyn_cast<CXXConstructorDecl>( ctor.funcDecl );
			if( ctor.excluded ) continue;
			//if( cdec->getAccess() == AS_protected ) continue;
			kvmap["parlist"] = ctor.cxxProtoParamDecl;
			kvmap["parcall"] = ctor.cxxCallParamV;
			class_decl += cdao_string_fill( tpl_class_decl_constru, kvmap );
			kvmap["parlist"] = ctor.cxxProtoParam;
			class_new += cdao_string_fill( tpl_class_new, kvmap );
			type_codes += cdao_string_fill( tpl_class_init2, kvmap );
		}
#if 0
	}else if( not has_private_ctor_only and not has_ctor ){
		// class has no virtual methods but has protected constructor
		// should also be wrapped, so that it can be derived by Dao class:
		class_new += cdao_string_fill( tpl_class_new, kvmap );
		type_codes += cdao_string_fill( tpl_class_init2, kvmap );
	}else if( not has_private_ctor_only ){
		type_codes += cdao_string_fill( tpl_class_noderive, kvmap );
	}
#endif
	kvmap["constructors"] = class_decl;
	for(i=0, n = methods.size(); i<n; i++){
		CDaoFunction & meth = methods[i];
		const CXXMethodDecl *mdec = dyn_cast<CXXMethodDecl>( meth.funcDecl );
		if( meth.excluded ) continue;
		if( mdec->getAccess() == AS_protected && not mdec->isPure() /* && not meth.nowrap */ ){
			dao_meths += meth.daoProtoCodes;
			meth_decls += meth.cxxProtoCodes + ";\n";
			string wrapper = meth.cxxWrapper;
			string name22 = "DaoCxx_" + name;
			string from = name + "* self= (" + name + "*)";
			string to = name22 + "* self= (" + name22 + "*)";
			string from2 = "self->" + mdec->getNameAsString() + "(";
			string to2 = "self->DaoWrap_" + mdec->getNameAsString() + "(";
			string from3 = name + "::" + mdec->getNameAsString() + "(";
			string to3 = name22 + "::DaoWrap_" + mdec->getNameAsString() + "(";
			size_t pos;
			if( (pos = wrapper.find( from )) != string::npos )
				wrapper.replace( pos, from.size(), to );
			if( (pos = wrapper.find( from2 )) != string::npos )
				wrapper.replace( pos, from2.size(), to2 );
			if( (pos = wrapper.find( from3 )) != string::npos )
				wrapper.replace( pos, from3.size(), to3 );
			meth_codes += wrapper;
		}
		if( mdec->isVirtual() ){
			type_decls += meth.cxxWrapperVirtProto;
			type_codes += meth.cxxWrapperVirt;
			CDaoProxyFunction::Use( meth.signature2 );
		}
	}
	//if( not hasVirtual and not has_protected_ctor ){
		//}
	//}else{
		type_decls += cdao_string_fill( tpl_class_def, kvmap ) + class_new;
		type_codes += cdao_string_fill( tpl_class_init, kvmap );
	//}
	//if( not noWrapping ) 
		type_codes += cxxWrapperVirt;

	string parents, casts, cast_funcs;
	for(i=0,n=bases.size(); i<n; i++){
		CDaoUserType *sup = bases[i];
		string supname = sup->GetName();
		string supname2 = sup->GetIdName();
		parents += "dao_" + supname2 + "_Typer, ";
		casts += "dao_cast_" + idname + "_to_" + supname2 + ",";
		kvmap[ "parent" ] = supname;
		kvmap[ "parent2" ] = supname2;
		cast_funcs += cdao_string_fill( cast_to_parent, kvmap );
	}
	kvmap[ "parents" ] = parents;
	kvmap[ "casts" ] = casts;
	kvmap[ "cast_funcs" ] = cast_funcs;
	kvmap[ "alloc" ] = alloc_default;
	kvmap[ "comment" ] = has_public_destroctor ? "" : "//";
	//if( utp.nested != 2 or utp.noDestructor ) return usertype_code_none.expand( kvmap );
	kvmap["delete"] = "Dao_" + idname + "_Delete";
	if( name == "QApplication" or name == "QCoreApplication" ) kvmap[ "comment" ] = "//";
	//if( isQWidget ) kvmap["if_parent"] = "if( ((" + utp.name2 + "*)self)->parentWidget() == NULL )";
#if 0
	if( del_tests.has( utp.condType ) ){
		kvmap["condition"] = del_tests[ utp.condType ];
		kvmap["deltest"] = "Dao_" + utp.name + "_DelTest";
		return usertype_code_class2.expand( kvmap );
	}
#endif
	typer_codes = cdao_string_fill( usertype_code_class, kvmap );
	return 0;
}
