/*
// Dao Virtual Machine
// http://daoscript.org
//
// Copyright (c) 2006-2017, Limin Fu
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

#ifndef DAO_INTERFACE_H
#define DAO_INTERFACE_H

#include"daoType.h"


/*
// Abstract Interface:
*/
struct DaoInterface
{
	DAO_VALUE_COMMON;

	DaoNamespace  *nameSpace;  /* Definition namespace; */
	DaoType       *abtype;     /* Type object for this interface; */
	DList         *bases;      /* Base interfaces; */
	DMap          *methods;    /* DHash<DString*,DaoRoutine*>; */
	DMap          *concretes;  /* Concrete interfaces for the abstract interfaces; */
	int            derived;
};

DaoInterface* DaoInterface_New( DaoNamespace *nspace, const char *name );
void DaoInterface_Delete( DaoInterface *self );

void DaoInterface_DeriveMethods( DaoInterface *self );

/*
// DaoInterface_BindTo(): Bind an interface to a type;
// Return NULL on success, otherwise return the first interface method
// that is not compatible with the type;
*/
DaoRoutine* DaoInterface_BindTo( DaoInterface *self, DaoType *type, DMap *binds );
DaoCinType* DaoInterface_GetConcrete( DaoInterface *self, DaoType *type );

int DaoType_MatchInterface( DaoType *self, DaoInterface *inter, DMap *binds );



/*
// Concrete Interface Type:
//
// interface SomeInterface for SomeTargetType {
//     routine SomeMethod(){
//     }
// }
//
// -- SomeInterface:
//    Abstract interface type whose methods must be implemented here.
//    Only methods that have no matching implementations in SomeTargetType
//    need to be implemented in the concrete interface;
//
// -- SomeTargetType:
//    Target type which can be any type;
//
// -- Type Names:
//    interface<SomeInterface<SomeTargetType>>: class  type for the concrete interface;
//    SomeInterface<SomeTargetType>           : object type for the concrete interface;
//
// -- Type Matching:
//    SomeTargetType can match to both SomeInterface and SomeInterface<SomeTargetType>;
//    SomeInterface<SomeTargetType> can match to SomeInterface;
//
// -- Type Casting:
//    Assignment and moving between SomeTargetType and SomeInterface will convert
//    one to the other automatically with implicit type casting.
//
// -- Inheritance:
//    interface SubInterface for SomeTargetType : SomeInterface<SomeTargetType> {
//        routine SomeMethod2(){
//        }
//    }
//    The abstract interface SubInterface must be derived from SomeInterface.
//    The target types must be the same.
*/
struct DaoCinType
{
	DAO_VALUE_COMMON;

	int       derived;
	DaoType  *citype;  /* Concrete interface type type; */
	DaoType  *vatype;  /* Concrete interface value type; */
	DList    *bases;   /* Parent interface classes; */
	DMap     *methods; /* DHash<DString*,DaoRoutine*>; */

	DaoType       *target;    /* target type; */
	DaoInterface  *abstract;  /* abstract interface; */
};

DaoCinType* DaoCinType_New( DaoInterface *inter, DaoType *target );
void DaoCinType_Delete( DaoCinType *self );

void DaoCinType_DeriveMethods( DaoCinType *self );



/*
// Concrete Interface Value:
*/
struct DaoCinValue
{
	DAO_VALUE_COMMON;

	DaoCinType  *cintype;
	DaoValue    *value;
};
DaoCinValue* DaoCinValue_New( DaoCinType *cintype, DaoValue *value );
DaoCinValue* DaoCinValue_Copy( DaoCinValue *self );
void DaoCinValue_Delete( DaoCinValue *self );

#define DAO_MT_CIV  DAO_MT_SUB


#endif
