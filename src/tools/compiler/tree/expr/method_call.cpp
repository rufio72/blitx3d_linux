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

	// Search for the method in the class hierarchy
	// Start with the object's class, then walk up the inheritance chain
	StructType *search_class = st;
	Type *t = e->findType( tag );

	while( search_class ){
		resolved_ident = search_class->ident + "_" + method;
		sem_decl = e->findFunc( resolved_ident );
		if( sem_decl && (sem_decl->kind & DECL_FUNC) ){
			// Found the method
			break;
		}
		// Not found, try parent class
		search_class = search_class->superType;
	}

	if( !sem_decl || !(sem_decl->kind & DECL_FUNC) ) {
		ex( "Method '" + method + "' not found in class '" + st->ident + "' or its parents" );
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

////////////////////////////
// Super method call      //
////////////////////////////
ExprNode *SuperMethodCallNode::semant( Environ *e ){
	// Find the current class type
	Type *t = e->findType( currentClass );
	if( !t ) ex( "Class '" + currentClass + "' not found" );
	StructType *st = t->structType();
	if( !st ) ex( "'" + currentClass + "' is not a class" );

	// Get the parent class
	StructType *parent = st->superType;
	if( !parent ) ex( "Class '" + currentClass + "' has no parent class" );

	// Build the function name: ParentClass_MethodName
	resolved_ident = parent->ident + "_" + method;

	// Find the function
	Type *ret_type = e->findType( tag );
	sem_decl = e->findFunc( resolved_ident );
	if( !sem_decl || !(sem_decl->kind & DECL_FUNC) ) {
		ex( "Method '" + method + "' not found in parent class '" + parent->ident + "'" );
	}

	// Find 'self' in the current environment
	self_decl = e->findDecl( "self" );
	if( !self_decl ) ex( "Super can only be used inside a method" );

	FuncType *f = sem_decl->type->funcType();
	if( ret_type && f->returnType != ret_type ) ex( "incorrect method return type" );

	// Semantic analysis on arguments
	exprs->semant( e );

	// The method's first parameter is 'self', we need to account for that
	DeclSeq *params_without_self = d_new DeclSeq();
	for( int i = 1; i < f->params->size(); ++i ) {
		params_without_self->decls.push_back( f->params->decls[i] );
	}
	exprs->castTo( params_without_self, e, f->cfunc );
	params_without_self->decls.clear();
	delete params_without_self;

	sem_type = f->returnType;
	return this;
}

TNode *SuperMethodCallNode::translate( Codegen *g ){
	FuncType *f = sem_decl->type->funcType();

	TNode *t;
	TNode *l = global( "_f" + resolved_ident );

	// Get 'self' variable as the object (using the stored self_decl)
	TNode *self_node = d_new TNode( IR_MEM, local( self_decl->offset ), 0, 0 );

	// Build args: first self, then the other args
	TNode *args_node = exprs->translate( g, f->cfunc );
	TNode *r = d_new TNode( IR_ARG, self_node, args_node, 0 );

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
llvm::Value *SuperMethodCallNode::translate2( Codegen_LLVM *g ){
	FuncType *f = sem_decl->type->funcType();
	auto func = f->llvmFunction( sem_decl->name, g );

	std::vector<llvm::Value*> args;

	// First argument is self (stored during semant)
	llvm::Value *self_val = g->builder->CreateLoad( self_decl->type->llvmType( g->context.get() ), self_decl->ptr );
	args.push_back( self_val );

	// Then the rest of the arguments
	for( int i = 0; i < exprs->size(); i++ ){
		args.push_back( exprs->exprs[i]->translate2( g ) );
	}

	return g->builder->CreateCall( func, args );
}
#endif

json SuperMethodCallNode::toJSON( Environ *e ){
	json tree;
	tree["@class"] = "SuperMethodCallNode";
	tree["sem_type"] = sem_type->toJSON();
	tree["currentClass"] = currentClass;
	tree["method"] = method;
	tree["resolved_ident"] = resolved_ident;
	tree["tag"] = tag;
	tree["sem_decl"] = sem_decl->toJSON();
	tree["exprs"] = exprs->toJSON( e );
	return tree;
}
