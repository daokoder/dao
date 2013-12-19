/*
// Dao Profiler
//
// Copyright (c) 2013, Limin Fu
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <time.h>
#include <stdlib.h>

#ifdef UNIX
#include<unistd.h>
#include<sys/time.h>
#endif

#ifdef MAC_OSX
#  include <crt_externs.h>
#  define environ (*_NSGetEnviron())
#endif


#include "daoValue.h"
#include "daoStream.h"
#include "daoRoutine.h"
#include "daoProcess.h"
#include "daoNamespace.h"
#include "daoVmspace.h"


void DaoProfiler_EnterFrame( DaoProfiler *self, DaoProcess *proc, DaoStackFrame *frame, int );
void DaoProfiler_LeaveFrame( DaoProfiler *self, DaoProcess *proc, DaoStackFrame *frame, int );
void DaoProfiler_Report( DaoProfiler *self, DaoStream *stream );

typedef struct DaoxProfiler DaoxProfiler;

struct DaoxProfiler
{
	DaoProfiler base;

	DMutex  mutex;
	DMap   *profile; /* map<DaoRoutine*,map<DaoRoutine*,DaoComplex*>> */
	DMap   *one;     /* map<DaoRoutine*,DaoComplex*> */
};

DaoxProfiler* DaoxProfiler_New()
{
	DaoxProfiler *self = (DaoxProfiler*) dao_calloc(1,sizeof(DaoxProfiler));
	self->profile = DHash_New(0,D_MAP);
	self->one = DHash_New(0,D_VALUE);
	self->base.EnterFrame = DaoProfiler_EnterFrame;
	self->base.LeaveFrame = DaoProfiler_LeaveFrame;
	self->base.Report = DaoProfiler_Report;
	DMutex_Init( & self->mutex );
	return self;
}
void DaoxProfiler_Delete( DaoxProfiler *self )
{
	DMutex_Destroy( & self->mutex );
	DMap_Delete( self->profile );
	DMap_Delete( self->one );
	dao_free( self );
}
void DaoxProfiler_Update( DaoxProfiler *self, DaoStackFrame *frame, double time )
{
	DaoComplex com = {DAO_COMPLEX,0,0,0,1,{0.0,0.0}};
	DaoRoutine *caller = frame->prev ? frame->prev->routine : NULL;
	DaoRoutine *callee = frame->routine;
	DNode *it, *it2;

	if( frame->returning == -1 ) caller = NULL;
	while( caller && caller->original ) caller = caller->original;
	while( callee && callee->original ) callee = callee->original;

	DMutex_Lock( & self->mutex );
	{
		it = DMap_Find( self->profile, callee );
		if( it == NULL ) it = DMap_Insert( self->profile, callee, self->one );

		it2 = DMap_Find( it->value.pMap, caller );
		if( it2 == NULL ) it2 = DMap_Insert( it->value.pMap, caller, & com );
		it2->value.pValue->xComplex.value.real += time;
		it2->value.pValue->xComplex.value.imag += 1;
	}
	DMutex_Unlock( & self->mutex );
}


double dao_clock()
{
	return ((double)clock())/CLOCKS_PER_SEC;
}

void DMap_DeleteTimeMap( DMap *self )
{
	DMap_Delete( self );
}

DMap* DaoProcess_GetTimeMap( DaoProcess *self )
{
	DMap *mapTime = (DMap*) DaoProcess_GetAuxData( self, DMap_DeleteTimeMap );
	if( mapTime ) return mapTime;
	mapTime = DHash_New(0,D_VALUE);
	DaoProcess_SetAuxData( self, DMap_DeleteTimeMap, mapTime );
	return mapTime;
}
DaoComplex* DaoProcess_GetTimeData( DaoProcess *self, DaoStackFrame *frame )
{
	DaoComplex com = {DAO_COMPLEX,0,0,0,1,{0.0,0.0}};
	DMap *mapTime = DaoProcess_GetTimeMap( self );
	DNode *it = DMap_Find( mapTime, frame );
	if( it == NULL ) it = DMap_Insert( mapTime, frame, & com );
	return (DaoComplex*) it->value.pValue;
}

void DaoProfiler_EnterFrame( DaoProfiler *self, DaoProcess *proc, DaoStackFrame *frame, int start )
{
	DaoComplex *tmdata;
	if( frame->routine == NULL ) return;
	//printf( "DaoProfiler_EnterFrame: %-20s %s\n", frame->routine->routName->mbs, start ? "start":"" );

	tmdata = DaoProcess_GetTimeData( proc, frame );
	if( start ) tmdata->value.real = 0.0;
	tmdata->value.imag = dao_clock();
}
void DaoProfiler_LeaveFrame( DaoProfiler *self, DaoProcess *proc, DaoStackFrame *frame, int end )
{
	DaoComplex *tmdata;
	if( frame->routine == NULL ) return;
	//printf( "DaoProfiler_LeaveFrame: %-20s %s\n", frame->routine->routName->mbs, end?"end":"" );

	tmdata = DaoProcess_GetTimeData( proc, frame );
	tmdata->value.real += dao_clock() - tmdata->value.imag;
	tmdata->value.imag = dao_clock();
	if( end ) DaoxProfiler_Update( (DaoxProfiler*) self, frame, tmdata->value.real );
}
complex16 DaoProfiler_Sum( DMap *profile )
{
	DNode *it2;
	complex16 com = {0.0, 0.0};
	for(it2=DMap_First(profile); it2; it2=DMap_Next(profile,it2)){
		DaoRoutine *caller = (DaoRoutine*) it2->key.pValue;
		DaoComplex *data = (DaoComplex*) it2->value.pValue;
		com.real += data->value.real;
		com.imag += data->value.imag;
	}
	return com;
}
void DaoProfiler_Report( DaoProfiler *self0, DaoStream *stream )
{
	DaoComplex com = {DAO_COMPLEX,0,0,0,1,{0.0,0.0}};
	DaoxProfiler *self = (DaoxProfiler*) self0;
	DMap *summary = DMap_New(D_VALUE,0);
	DMap *summary2 = DMap_New(D_VALUE,0);
	DNode *it, *it2;
	int count, max = 20;
	char buf1[20];
	char buf2[20];
	char buf[100];

	for(it=DMap_First(self->profile); it; it=DMap_Next(self->profile,it)){
		DaoRoutine *callee = (DaoRoutine*) it->key.pValue;
		com.value = DaoProfiler_Sum( it->value.pMap );
		com.value.real = - com.value.real;
		com.value.imag = - com.value.imag;
		DMap_Insert( summary, & com, it );
	}

	DaoStream_WriteMBS( stream, "\n=============== Program Profile ==============\n" );
	for(count=max,it=DMap_First(summary); it; it=DMap_Next(summary,it),--count){
		DNode *it2 = (DNode*) it->value.pVoid;
		DaoComplex *data = (DaoComplex*) it->key.pValue;
		DaoRoutine *callee = (DaoRoutine*) it2->key.pValue;
		const char *name = callee->routName->mbs;
		snprintf( buf1, sizeof(buf1)-2, "%s()", name );
		snprintf( buf, sizeof(buf), "%-20s : #calls %9i times,  CPU time: %9.3f seconds;\n",
				buf1, (int) -data->value.imag, -data->value.real );
		DaoStream_WriteMBS( stream, buf );
		if( count == 0 ) break;
	}

	DaoStream_WriteMBS( stream, "\n" );
	for(count=max,it=DMap_First(summary); it; it=DMap_Next(summary,it),--count){
		DNode *it2 = (DNode*) it->value.pVoid;
		DaoRoutine *callee = (DaoRoutine*) it2->key.pValue;
		DMap *profile = it2->value.pMap;
		const char *name = callee->routName->mbs;
		snprintf( buf1, sizeof(buf1)-2, "%s()", name );

		DMap_Reset( summary2 );
		for(it2=DMap_First(profile); it2; it2=DMap_Next(profile,it2)){
			DaoRoutine *caller = (DaoRoutine*) it2->key.pValue;
			DaoComplex *data = (DaoComplex*) it2->value.pValue;
			com.value.real = - data->value.real;
			com.value.imag = - data->value.imag;
			DMap_Insert( summary2, & com, caller );
		}
		for(it2=DMap_First(summary2); it2; it2=DMap_Next(summary2,it2)){
			DaoRoutine *caller = (DaoRoutine*) it2->value.pValue;
			DaoComplex *data = (DaoComplex*) it2->key.pValue;
			const char *name2 = caller ? caller->routName->mbs : "";
			snprintf( buf2, sizeof(buf2)-2, "%s()", name2 );
			snprintf( buf, sizeof(buf), "%-20s (called by  %-20s %9i times),  CPU time: %9.3f\n",
					buf1, buf2, (int) -data->value.imag, -data->value.real );
			DaoStream_WriteMBS( stream, buf );
			buf1[0] = '\0';
		}
		if( count == 0 ) break;
	}
}

DAO_DLL int DaoProfiler_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoxProfiler *profiler = DaoxProfiler_New();
	DaoVmSpace_SetUserProfiler( vmSpace, (DaoProfiler*) profiler );
	return 0;
}
