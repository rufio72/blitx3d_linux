#include "read.h"

///////////////
// Read data //
///////////////
void ReadNode::semant( Environ *e ){
	var->semant( e );
	if( var->sem_type->constType() ) ex( "Constants can not be modified" );
	if( var->sem_type->structType() ) ex( "Data can not be read into an object" );
}

void ReadNode::translate( Codegen *g ){
	TNode *t;
	if( var->sem_type==Type::int_type ) t=call( "__bbReadInt" );
	else if( var->sem_type==Type::float_type ) t=fcall( "__bbReadFloat" );
	else t=call( "__bbReadStr" );
	g->code( var->store( g,t ) );
}

#ifdef USE_LLVM
void ReadNode::translate2( Codegen_LLVM *g ){
	auto int_ty=Type::int_type->llvmType( g->context.get() );
	auto float_ty=Type::float_type->llvmType( g->context.get() );
	auto string_ty=Type::string_type->llvmType( g->context.get() );

	llvm::Value *t=0;
	if( var->sem_type==Type::int_type ) t=g->CallIntrinsic( "_bbReadInt",int_ty,0 );
	else if( var->sem_type==Type::float_type ) t=g->CallIntrinsic( "_bbReadFloat",float_ty,0 );
	else t=g->CallIntrinsic( "_bbReadStr",string_ty,0 );
	var->store2( g,t );
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"
void ReadNode::translate3( Codegen_C *g ){
	std::string func;
	if( var->sem_type==Type::int_type ) func = "_bbReadInt";
	else if( var->sem_type==Type::float_type ) func = "_bbReadFloat";
	else func = "_bbReadStr";

	std::string val = func + "()";
	var->store3( g, val );
}
#endif
