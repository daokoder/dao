
#include <clang/Lex/Preprocessor.h>
#include <clang/Sema/DeclSpec.h>
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
class DAO_DLL_$(module) DaoSS_$(class) : $(parents)\n\
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
"  { dao_$(host_typer)_$(cxxname)$(overload), \"$(daoname)( $(parlist) )$(retype)\" },\n";

const string cxx_wrap_proto 
= "static void dao_$(host_typer)_$(cxxname)$(overload)( DaoContext *_ctx, DValue *_p[], int _n )";

const string tpl_struct = "$(name)* DAO_DLL_$(module) Dao_$(name)_New();\n";

const string tpl_struct_alloc =
"$(name)* Dao_$(name)_New()\n{\n\
	$(name) *self = calloc( 1, sizeof($(name)) );\n\
	return self;\n\
}\n";
const string tpl_struct_alloc2 =
"$(name)* Dao_$(name)_New()\n{\n\
	$(name) *self = new $(name)();\n\
	return self;\n\
}\n";

const string tpl_struct_daoc = 
"typedef struct Dao_$(name) Dao_$(name);\n\
struct DAO_DLL_$(module) Dao_$(name)\n\
{\n\
	$(name)  nested;\n\
	$(name) *object;\n\
	DaoCData *cdata;\n\
};\n\
Dao_$(name)* DAO_DLL_$(module) Dao_$(name)_New();\n";

const string tpl_struct_daoc_alloc =
"Dao_$(name)* Dao_$(name)_New()\n\
{\n\
	Dao_$(name) *wrap = calloc( 1, sizeof(Dao_$(name)) );\n\
	$(name) *self = ($(name)*) wrap;\n\
	wrap->cdata = DaoCData_New( dao_$(name)_Typer, wrap );\n\
	wrap->object = self;\n\
$(callbacks)$(selfields)\treturn wrap;\n\
}\n";

const string tpl_set_callback = "\tself->$(callback) = Dao_$(name)_$(callback);\n";
const string tpl_set_selfield = "\tself->$(field) = wrap;\n";

const string c_wrap_new = 
"static void dao_$(host)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	Dao_$(host) *self = Dao_$(host)_New();\n\
	DaoContext_PutResult( _ctx, (DaoBase*) self->cdata );\n\
}\n";
const string cxx_wrap_new = 
"static void dao_$(host)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	DaoCxx_$(host) *self = DaoCxx_$(host)_New();\n\
	DaoContext_PutResult( _ctx, (DaoBase*) self->cdata );\n\
}\n";
const string cxx_wrap_alloc2 = 
"static void dao_$(host)_$(cxxname)( DaoContext *_ctx, DValue *_p[], int _n )\n\
{\n\
	$(host) *self = Dao_$(host)_New();\n\
	DaoContext_PutCData( _ctx, self, dao_$(host)_Typer );\n\
}\n";

const string tpl_class_def = 
"class DAO_DLL_$(module) DaoCxxVirt_$(class) $(virt_supers)\n{\n\
	public:\n\
	DaoCxxVirt_$(class)(){ self = 0; cdata = 0; }\n\
	void DaoInitWrapper( $(class) *self, DaoCData *d );\n\n\
	$(class) *self;\n\
	DaoCData *cdata;\n\
\n$(virtuals)\n\
$(qt_virt_emit)\n\
};\n\
class DAO_DLL_$(module) DaoCxx_$(class) : public $(class), public DaoCxxVirt_$(class)\n\
{ $(Q_OBJECT)\n\n\
\tpublic:\n\
$(constructors)\n\
	~DaoCxx_$(class)();\n\
	void DaoInitWrapper();\n\n\
$(methods)\n\
};\n";

const string tpl_class2 = 
tpl_class_def + "$(class)* Dao_$(class)_Copy( const $(class) &p );\n";

const string tpl_class_init =
"void DaoCxxVirt_$(class)::DaoInitWrapper( $(class) *s, DaoCData *d )\n\
{\n\
	self = s;\n\
	cdata = d;\n\
$(init_supers)\n\
$(qt_init)\n\
}\n\
DaoCxx_$(class)::~DaoCxx_$(class)()\n\
{\n\
	if( cdata ){\n\
		DaoCData_SetData( cdata, NULL );\n\
		DaoCData_SetExtReference( cdata, 0 );\n\
	} \n\
}\n\
void DaoCxx_$(class)::DaoInitWrapper()\n\
{\n\
	cdata = DaoCData_New( dao_$(class)_Typer, this );\n\
	DaoCxxVirt_$(class)::DaoInitWrapper( this, cdata );\n\
$(qt_make_linker)\n\
}\n";
const string tpl_class_init_qtss = 
"void DAO_DLL_$(module) Dao_$(class)_InitSS( $(class) *p )\n\
{\n\
   if( p->userData(0) == NULL ){\n\
		DaoSS_$(class) *linker = new DaoSS_$(class)();\n\
		p->setUserData( 0, linker );\n\
		linker->Init( NULL );\n\
	}\n\
}\n";
const string tpl_class_copy = tpl_class_init +
"$(class)* DAO_DLL_$(module) Dao_$(class)_Copy( const $(class) &p )\n\
{\n\
	$(class) *object = new $(class)( p );\n\
$(qt_make_linker3)\n\
	return object;\n\
}\n";
const string tpl_class_decl_constru = 
"	DaoCxx_$(class)( $(parlist) ) : $(class)( $(parcall) ){}\n";

const string tpl_class_new =
"\nDaoCxx_$(class)* DAO_DLL_$(module) DaoCxx_$(class)_New( $(parlist) );\n";
const string tpl_class_new_novirt =
"\n$(class)* DAO_DLL_$(module) Dao_$(class)_New( $(parlist) );\n";
const string tpl_class_init_qtss_decl =
"\nvoid DAO_DLL_$(module) Dao_$(class)_InitSS( $(class) *p );\n";

const string tpl_class_noderive =
"\n$(class)* DAO_DLL_$(module) Dao_$(class)_New( $(parlist) )\n\
{\n\
	$(class) *object = new $(class)( $(parcall) );\n\
$(qt_make_linker3)\n\
	return object;\n\
}\n";
const string tpl_class_init2 =
"\nDaoCxx_$(class)* DAO_DLL_$(module) DaoCxx_$(class)_New( $(parlist) )\n\
{\n\
	DaoCxx_$(class) *self = new DaoCxx_$(class)( $(parcall) );\n\
	self->DaoInitWrapper();\n\
	return self;\n\
}\n";

const string tpl_init_super = "\tDaoCxxVirt_$(super)::DaoInitWrapper( s, d );\n";

const string tpl_meth_decl =
"\t$(retype) $(name)( int &_cs$(comma) $(parlist) )$(extra);\n";

const string tpl_meth_decl2 =
"\t$(retype) $(name)( $(parlist) )$(extra);\n";

const string tpl_meth_decl3 =
"\t$(retype) DaoWrap_$(name)( $(parlist) )$(extra){ $(return)$(type)::$(cxxname)( $(parcall) ); }\n";

const string tpl_raise_call_protected =
"  if( _p[0]->t == DAO_CDATA && DaoCData_GetObject( _p[0]->v.cdata ) == NULL ){\n\
    DaoContext_RaiseException( _ctx, DAO_ERROR, \"call to protected method\" );\n\
    return;\n\
  }\n";

const string methlist_code = 
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
  { $(parents)0 },\n\
  { $(casts)0 },\n\
  $(delete),\n\
  $(deltest)\n\
};\n\
DaoTypeBase DAO_DLL_$(module) *dao_$(typer)_Typer = & $(typer)_Typer;\n";

const string usertype_code_none = methlist_code + usertype_code;
const string usertype_code_struct = methlist_code + delete_struct + usertype_code;
const string usertype_code_class = methlist_code + delete_class + usertype_code;
const string usertype_code_class2 = methlist_code + delete_class + delete_test + usertype_code;

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );

CDaoUserType::CDaoUserType( CDaoModule *mod, RecordDecl *decl )
{
	module = mod;
	noWrapping = hasVirtual = noConstructor = false;
	isQObject = isQObjectBase = false;
	isRedundant = true;
	alloc_default = "NULL";
	this->decl = NULL;
	SetDeclaration( decl );
}
void CDaoUserType::SetDeclaration( RecordDecl *decl )
{
	this->decl = decl;
}
bool CDaoUserType::IsFromMainModule()
{
	if( decl == NULL ) return false;
	RecordDecl *dd = decl->getDefinition();
	if( dd == NULL ) return false;
	return module->IsFromMainModule( dd->getLocation() );
}
string CDaoUserType::GetInputFile()const
{
	if( decl == NULL ) return false;
	RecordDecl *dd = decl->getDefinition();
	if( dd == NULL ) return false;
	return module->GetFileName( dd->getLocation() );
}
int CDaoUserType::Generate()
{
	RecordDecl *dd = decl->getDefinition();
	outs() << decl->getNameAsString() << ": " << (void*)decl <<" " << (void*)dd << "\n";
	outs() << decl->getQualifiedNameAsString() << "\n";
	isRedundant = dd != NULL && dd != decl;
	if( isRedundant ) return 0;
	if (CXXRecordDecl *record = dyn_cast<CXXRecordDecl>(decl)) return Generate( record );
	return 1;
}
int CDaoUserType::Generate( CXXRecordDecl *decl )
{
	Preprocessor & pp = module->compiler->getPreprocessor();
	SourceManager & sm = module->compiler->getSourceManager();
	string name = decl->getNameAsString();
	map<string,string> kvmap;
	map<string,int> overloads;
	bool has_pure_virtual = false;
	noWrapping = false;
	hasVirtual = false; // protected or public virtual function;

	vector<CDaoUserType*> bases;
	vector<CDaoFunction>  methods;
	vector<CDaoFunction>  constructors;

	kvmap[ "class" ] = name;

	string daoc_supers;
	string virt_supers;
	string init_supers;
	string ss_supers;
	string ss_init_sup;
	string class_new;
	string class_decl;

	map<const RecordDecl*,CDaoUserType*>::iterator find;
	CXXRecordDecl::base_class_iterator baseit, baseend = decl->bases_end();
	for(baseit=decl->bases_begin(); baseit != baseend; baseit++){
		CXXRecordDecl *p = baseit->getType().getTypePtr()->getAsCXXRecordDecl();
		find = module->allUsertypes2.find( p );
		if( find == module->allUsertypes2.end() ) continue;

		CDaoUserType & sup = *find->second;
		string supname = sup.GetName();
		bases.push_back( & sup );
		outs() << "parent: " << p->getNameAsString() << "\n";

		if( sup.noWrapping or not sup.hasVirtual ) continue;
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
	CXXRecordDecl::method_iterator methit, methend = decl->method_end();
	for(methit=decl->method_begin(); methit!=methend; methit++){
		string name = methit->getNameAsString();
		outs() << name << ": " << methit->getAccess() << " ......................\n";
		outs() << methit->getType().getAsString() << "\n";
		if( methit->isImplicit() ) continue;
		if( methit->isPure() ) has_pure_virtual = true;
		if( methit->getAccess() == AS_private ) continue;
		if( methit->isVirtual() ) hasVirtual = true;
		methods.push_back( CDaoFunction( module, *methit, ++overloads[name] ) );
		methods.back().Generate();
	}

	bool has_ctor = false;
	bool has_public_ctor = false;
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
			if( ctorit->getAccess() == AS_public ) has_public_ctor = true;
			if( ctorit->param_size() == 0 ){
				has_explicit_default_ctor = true;
				if( ctorit->getAccess() == AS_private ) has_private_default_ctor = true;
			}
		}
		if( ctorit->getAccess() == AS_private ) continue;
		constructors.push_back( CDaoFunction( module, *ctorit, ++overloads[name] ) );
		constructors.back().Generate();
		if( ctorit->getAccess() == AS_protected && not hasVirtual ) continue;
		CDaoFunction & meth = constructors.back();
		dao_meths += meth.daoProtoCodes;
		meth_decls += meth.cxxProtoCodes + ";\n";
		meth_codes += meth.cxxWrapper;
	}
	// no explicit default constructor and explicit private constructor
	// imply private default constructor:
	if( has_private_ctor && not has_explicit_default_ctor ) has_private_default_ctor = true;
	if( has_implicit_default_ctor ) has_public_ctor = true;
	noConstructor = noWrapping || (has_public_ctor == false && has_protected_ctor == false);

	kvmap[ "module" ] = UppercaseString( module->moduleInfo.name );
	kvmap[ "host_typer" ] = name;
	kvmap[ "cxxname" ] = name;
	kvmap[ "daoname" ] = name;
	kvmap[ "retype" ] = "=>" + name;

	bool implement_default_ctor = hasVirtual && not noWrapping;
	implement_default_ctor = implement_default_ctor && has_implicit_default_ctor;
	if( implement_default_ctor ){
		if( hasVirtual ){
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
	if( not hasVirtual and not has_protected_ctor and not noWrapping ){
		kvmap[ "class" ] = name;
		kvmap[ "name" ] = name;
		kvmap[ "qt_make_linker3" ] = "";
		if( isQObject ) kvmap["qt_make_linker3"] = cdao_string_fill( qt_make_linker3, kvmap );
		if( has_ctor ){
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
		}else if( not has_private_ctor ){
			type_decls = cdao_string_fill( tpl_struct, kvmap );
			type_codes = cdao_string_fill( tpl_struct_alloc2, kvmap );
		}
	}
	string kmethods;
	string kvirtuals;
	for(i=0, n = methods.size(); i<n; i++){
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
		if( mdec->getAccess() == AS_protected && not noWrapping ){
			kvmap[ "parlist" ] = meth.cxxProtoParamDecl;
			kvmap[ "parcall" ] = meth.cxxCallParamV;
			kvmap[ "return" ] = kvmap[ "retype" ] == "void" ? "" : "return ";
			kmethods += cdao_string_fill( tpl_meth_decl3, kvmap );
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
	kvmap[ "parlist" ] = "";
	kvmap[ "parcall" ] = "";
	kvmap[ "Q_OBJECT" ] = "";
	kvmap[ "qt_signal_slot" ] = "";
	kvmap[ "qt_init" ] = "";
	kvmap[ "parents" ] = "public QObject";
	kvmap[ "init_parents" ] = "";
	kvmap[ "qt_make_linker" ] = "";
	kvmap[ "qt_virt_emit" ] = "";
	kvmap[ "qt_make_linker3" ] = "";
	if( isQObject ){
		kvmap["Q_OBJECT"] = "Q_OBJECT";
		kvmap["qt_init"] = qt_init;
		kvmap["qt_make_linker"] = cdao_string_fill( qt_make_linker, kvmap );
		kvmap["qt_make_linker3"] = cdao_string_fill( qt_make_linker3, kvmap );
		kvmap["qt_virt_emit"] = qt_virt_emit;
	}
	if( not noWrapping and has_ctor ){
		for(i=0, n = constructors.size(); i<n; i++){
			CDaoFunction & ctor = constructors[i];
			const CXXConstructorDecl *cdec = dyn_cast<CXXConstructorDecl>( ctor.funcDecl );
			if( cdec->getAccess() == AS_protected ) continue;
			kvmap["parlist"] = ctor.cxxProtoParamDecl;
			kvmap["parcall"] = ctor.cxxCallParamV;
			class_decl += cdao_string_fill( tpl_class_decl_constru, kvmap );
			kvmap["parlist"] = ctor.cxxProtoParam;
			class_new += cdao_string_fill( tpl_class_new, kvmap );
			type_codes += cdao_string_fill( tpl_class_init2, kvmap );
		}
	}else if( not noWrapping and not has_private_default_ctor and (not has_ctor or hasVirtual) ){
		// class has no virtual methods but has protected constructor
		// should also be wrapped, so that it can be derived by Dao class:
		class_new += cdao_string_fill( tpl_class_new, kvmap );
		type_codes += cdao_string_fill( tpl_class_init2, kvmap );
	}else if( not hasVirtual ){
		type_codes += cdao_string_fill( tpl_class_noderive, kvmap );
	}
	if( isQObject ){
		map<const CXXMethodDecl*,CDaoFunction*> dec2meth;
		for(i=0, n = methods.size(); i<n; i++){
			CDaoFunction & meth = methods[i];
			const CXXMethodDecl *mdec = dyn_cast<CXXMethodDecl>( meth.funcDecl );
			dec2meth[mdec] = & meth;
		}
		string qt_signals;
		string qt_slots;
		vector<CDaoFunction*> pubslots;
		vector<CDaoFunction*> protsignals;
		CXXRecordDecl::decl_iterator dit, dend;
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
			outs() << as << " " << is_pubslot << is_signal << " access\n";
		}
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
		kvmap["parents"] = ss_supers;
		kvmap["member_wrap"] = "";
		kvmap["set_wrap"] = "";
		kvmap["Emit"] = "";
		kvmap["slotDaoQt"] = "";
		kvmap["signalDao"] = "";
		if( ss_init_sup.size() ) kvmap["init_parents"] = ":"+ ss_init_sup;
		if( isQObjectBase ){
			kvmap["member_wrap"] = "   " + name + " *wrap;\n";
			kvmap["set_wrap"] = "wrap = w;";
			kvmap["Emit"] = qt_Emit;
			kvmap["signalDao"] = qt_signalDao;
		}
		type_decls += cdao_string_fill( daoss_class, kvmap );
	} // isQObject
	kvmap["constructors"] = class_decl;
	if( not noWrapping ){
		bool isPureVirt = has_pure_virtual;
		bool noCopyConstructor = false; //XXX
		if( isPureVirt || noCopyConstructor ){
			type_decls += cdao_string_fill( tpl_class_def, kvmap ) + class_new;
			type_codes += cdao_string_fill( tpl_class_init, kvmap );
		}else{
			type_decls += cdao_string_fill( tpl_class2, kvmap ) + class_new;
			type_codes += cdao_string_fill( tpl_class_copy, kvmap );
		}
	}
	for(i=0, n = methods.size(); i<n; i++){
		CDaoFunction & meth = methods[i];
		const CXXMethodDecl *mdec = dyn_cast<CXXMethodDecl>( meth.funcDecl );
		if( meth.excluded ) continue;
		if( mdec->getAccess() == AS_public ){
#if 0
			if( meth.cxxWrapper.find( "self->" + name + "::" ) >=0 ){
				if( $FUNC_VIRTUAL in meth.attribs ){
					meth_decls += meth.cxxProtoCodes + ";\n";
					meth_codes += meth.cxxWrapper;
					meth.daoProtoCodes.change( "__" + name + ", (%s*) \"" + name + "::(%w+%b())", ",%1\"%2" );
					meth.cxxProtoCodes.change( "__" + name + "(%b())", "%1" );
					meth.cxxWrapper.change( "%s ((%w+)__" + name + ")(%b()%s* %{)", 
							" %2%3\n  if( DaoCData_OwnData( _p[0]->v.cdata ) ){\n    %1( _ctx, _p, _n );\n    return;\n  }" );
					meth.cxxWrapper.replace( "self->" + name + "::", "self->" );
				}else{
					meth.daoProtoCodes.change( "__" + name + ", (%s*) \"" + name + "::(%w+%b())", ",%1\"%2" );
					meth.cxxProtoCodes.change( "__" + name + "(%b())", "%1" );
					meth.cxxWrapper.change( "__" + name + "(%b()%s*%b{})", "%1" );
				}
			}
#endif
			dao_meths += meth.daoProtoCodes;
			meth_decls += meth.cxxProtoCodes + ";\n";
			meth_codes += meth.cxxWrapper;
		}else if( hasVirtual && not noWrapping /* && not meth.nowrap */ ){
			dao_meths += meth.daoProtoCodes;
			meth_decls += meth.cxxProtoCodes + ";\n";
#if 0
			wrapper = meth.cxxWrapper;
			name22 = "DaoCxx_" + name;
			subst = name22 + " *self = (" + name22 + "*)";
			wrapper.change( name + "%s* %* %s* self %s* = %s* %b()", subst );
			wrapper.change( "self%-%>(%w+)%(", "self->DaoWrap_%1(" );
			lines = wrapper.split( "\n" );
			lines.insert( tpl_raise_call_protected, 3 );
			meth_codes += lines.join( "\n" );
#endif
		}
		if( not noWrapping && mdec->isVirtual() ){
			type_decls += meth.cxxWrapperVirtProto;
			type_codes += meth.cxxWrapperVirt;
			CDaoProxyFunction::Use( meth.signature2 );
		}
	}
	if( not noWrapping ) type_codes += cxxWrapperVirt;

	string parents, casts, cast_funcs;
	string typer_name = GetIdName();
	string qname = decl->getQualifiedNameAsString();
	kvmap[ "typer" ] = typer_name;
	kvmap[ "child" ] = qname;
	kvmap[ "parent" ] = "";
	kvmap[ "parent2" ] = "";
	for(i=0,n=bases.size(); i<n; i++){
		CDaoUserType *sup = bases[i];
		string supname = sup->GetName();
		string supname2 = sup->GetIdName();
		parents += "dao_" + typer_name + "_Typer, ";
		casts += "dao_cast_" + typer_name + "_to_" + supname2 + ",";
		kvmap[ "parent" ] = supname;
		kvmap[ "parent2" ] = supname2;
		cast_funcs += cdao_string_fill( cast_to_parent, kvmap );
	}
	kvmap[ "type" ] = name;
	kvmap[ "qname" ] = qname;
	kvmap[ "decls" ] = meth_decls;
	kvmap[ "meths" ] = dao_meths;
	kvmap[ "parents" ] = parents;
	kvmap[ "casts" ] = casts;
	kvmap[ "cast_funcs" ] = cast_funcs;
	kvmap[ "alloc" ] = alloc_default;
	kvmap[ "delete" ] = "NULL";
	kvmap[ "if_parent" ] = "";
	kvmap[ "deltest" ] = "NULL";
	kvmap[ "comment" ] = "";
	//if( utp.nested != 2 or utp.noDestructor ) return usertype_code_none.expand( kvmap );
	kvmap["delete"] = "Dao_" + typer_name + "_Delete";
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
	return 1;
}
