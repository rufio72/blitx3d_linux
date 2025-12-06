#include "method_call.h"
#include "../type.h"

////////////////////
// Method call    //
////////////////////
ExprNode *MethodCallNode::semant( Environ *e ){
	// First, get the type of the object
	obj = obj->semant( e );
	Type *obj_type = obj->sem_type;

	// Object must be a struct/class type
	StructType *st = obj_type->structType();
	if( !st ) ex( "Method call on non-object type" );

	// Build the function name: ClassName_MethodName
	resolved_ident = st->ident + "_" + method;

	// Find the function
	Type *t = e->findType( tag );
	sem_decl = e->findFunc( resolved_ident );
	if( !sem_decl || !(sem_decl->kind & DECL_FUNC) ) {
		ex( "Method '" + method + "' not found in class '" + st->ident + "'" );
	}

	FuncType *f = sem_decl->type->funcType();
	if( t && f->returnType != t ) ex( "incorrect method return type" );

	// Semantic analysis on arguments
	exprs->semant( e );

	// The method's first parameter is 'self', we need to account for that
	// Create a temporary param list without the first 'self' parameter for casting
	DeclSeq *params_without_self = d_new DeclSeq();
	for( int i = 1; i < f->params->size(); ++i ) {
		params_without_self->decls.push_back( f->params->decls[i] );
	}
	exprs->castTo( params_without_self, e, f->cfunc );
	// Clear the vector before delete to avoid double-free (we don't own these Decls)
	params_without_self->decls.clear();
	delete params_without_self;

	sem_type = f->returnType;
	return this;
}

TNode *MethodCallNode::translate( Codegen *g ){
	FuncType *f = sem_decl->type->funcType();

	TNode *t;
	TNode *l = global( "_f" + resolved_ident );

	// Build args: first the object (self), then the other args
	TNode *obj_node = obj->translate( g );
	TNode *args_node = exprs->translate( g, f->cfunc );

	// Prepend object to args
	TNode *r = d_new TNode( IR_ARG, obj_node, args_node, 0 );

	if( sem_type == Type::float_type ){
		t = d_new TNode( IR_FCALL, l, r, (exprs->size() + 1) * 4 );
	}else{
		t = d_new TNode( IR_CALL, l, r, (exprs->size() + 1) * 4 );
	}

	if( f->returnType->stringType() ){
		if( f->cfunc ){
			t = call( "__bbCStrToStr", t );
		}
	}
	return t;
}

#ifdef USE_LLVM
#include <llvm/IR/Attributes.h>

llvm::Value *MethodCallNode::translate2( Codegen_LLVM *g ){
	FuncType *f = sem_decl->type->funcType();
	auto func = f->llvmFunction( sem_decl->name, g );

	std::vector<llvm::Value*> args;

	// First argument is the object (self)
	args.push_back( obj->translate2( g ) );

	// Then the rest of the arguments
	for( int i = 0; i < exprs->size(); i++ ){
		args.push_back( exprs->exprs[i]->translate2( g ) );
	}

	return g->builder->CreateCall( func, args );
}
#endif

json MethodCallNode::toJSON( Environ *e ){
	json tree;
	tree["@class"] = "MethodCallNode";
	tree["sem_type"] = sem_type->toJSON();
	tree["method"] = method;
	tree["resolved_ident"] = resolved_ident;
	tree["tag"] = tag;
	tree["sem_decl"] = sem_decl->toJSON();
	tree["obj"] = obj->toJSON( e );
	tree["exprs"] = exprs->toJSON( e );
	return tree;
}
