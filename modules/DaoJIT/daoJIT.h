
#ifndef __DAO_JIT_H__
#define __DAO_JIT_H__

#include<vector>

#define DAO_DIRECT_API

extern "C"{
#include"daoArray.h"
#include"daoOpcode.h"
#include"daoStdtype.h"
#include"daoRoutine.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoVmspace.h"

void DaoJIT_Init( DaoVmSpace *vms );
void DaoJIT_Quit();

void DaoJIT_Free( DaoRoutine *routine );
void DaoJIT_Compile( DaoRoutine *routine );
void DaoJIT_Execute( DaoContext *context, int jitcode );
}


struct IndexRange
{
	int start;
	int end;
	IndexRange( int s=0, int e=0 ){ start = s; end = e; }

	static int Locate( const std::vector<IndexRange> & ranges, int index ){
		for(int i=0, n=ranges.size(); i<n; i++){
			const IndexRange & range = ranges[i];
			if( range.start <= index and index <= range.end ) return i;
		}
		return -1;
	}
};

#endif
