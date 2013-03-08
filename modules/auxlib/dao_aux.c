/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2011,2012, Limin Fu
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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include<stdlib.h>
#include<string.h>
#include<math.h>
#include"daoString.h"
#include"daoValue.h"
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoGC.h"
#include"dao_aux.h"


static void AUX_Tokenize( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *source = p[0]->xString.data;
	DaoList *list = DaoProcess_PutList( proc );
	DArray *tokens = DArray_New(D_TOKEN);
	int i, rc = 0;
	DString_ToMBS( source );
	rc = DaoTokens_Tokenize( tokens, source->mbs, DAO_LEX_COMMENT|DAO_LEX_SPACE, NULL );
	if( rc ){
		DaoString *str = DaoString_New(1);
		for(i=0; i<tokens->size; i++){
			DString_Assign( str->data, & tokens->items.pToken[i]->string );
			DArray_Append( & list->items, (DaoValue*) str );
		}
		DaoString_Delete( str );
	}
	DArray_Delete( tokens );
}
static void AUX_Log( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *info = p[0]->xString.data;
	FILE *fout = fopen( "dao.log", "a" );
	DString_ToMBS( info );
	fprintf( fout, "%s\n", info->mbs );
	fclose( fout );
}

static DaoFuncItem auxMeths[]=
{
	{ AUX_Tokenize,    "tokenize( source :string )=>list<string>" },
	{ AUX_Log,         "log( info='' )" },
	{ NULL, NULL }
};

DAO_DLL int DaoAux_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	ns = DaoVmSpace_GetNamespace( vmSpace, "std" );
	DaoNamespace_WrapFunctions( ns, auxMeths );
	return 0;
}

