#include "delete.h"
#include "../type.h"

//////////////////////
// Delete statement //
//////////////////////
void DeleteNode::semant( Environ *e ){
	expr=expr->semant( e );
	StructType *st = expr->sem_type->structType();
	if( st==0 ) ex( "Can't delete non-Newtype" );

	// Look for destructor: classname_destructor
	std::string dtor_name = st->ident + "_destructor";
	dtor_decl = e->findFunc( dtor_name );
	// Destructor is optional, so don't error if not found
}

void DeleteNode::translate( Codegen *g ){
	TNode *t=expr->translate( g );

	// Call destructor if it exists
	if( dtor_decl ){
		StructType *st = expr->sem_type->structType();
		TNode *l = global( "_f" + st->ident + "_destructor" );
		TNode *args = d_new TNode( IR_ARG, t, 0, 0 );
		g->code( d_new TNode( IR_CALL, l, args, 4 ) );
		// Re-translate expr for the delete call (t was consumed)
		t = expr->translate( g );
	}

	g->code( call( "__bbObjDelete",t ) );
}

#ifdef USE_LLVM
void DeleteNode::translate2( Codegen_LLVM *g ){
	llvm::Value *obj = expr->translate2( g );

	// Call destructor if it exists
	if( dtor_decl ){
		FuncType *f = dtor_decl->type->funcType();
		auto func = f->llvmFunction( dtor_decl->name, g );

		std::vector<llvm::Value*> args;
		args.push_back( obj );
		g->builder->CreateCall( func, args );
	}

	auto ty=Type::void_type->llvmType( g->context.get() );
	auto t=g->CastToObjPtr( obj );
	g->CallIntrinsic( "_bbObjDelete",ty,1,t );
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

void DeleteNode::translate3( Codegen_C *g ){
	std::string obj = expr->translate3( g );

	// Call destructor if it exists
	if( dtor_decl ){
		StructType *st = expr->sem_type->structType();
		std::string dtorName = g->toCSafeName( "f" + st->ident + "_destructor" );
		g->emitLine( dtorName + "(" + obj + ");" );
	}

	g->emitLine( "_bbObjDelete((bb_obj_t)" + obj + ");" );
}
#endif
