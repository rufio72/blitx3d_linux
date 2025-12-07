#include "new.h"
#include "../../ex.h"
#include "../type.h"

////////////////////
// New expression //
////////////////////
ExprNode *NewNode::semant( Environ *e ){
	sem_type=e->findType( ident );
	if( !sem_type ) ex( "custom type name not found" );
	if( sem_type->structType()==0 ) ex( "type is not a custom type" );

	// Check for constructor: classname_constructor (all lowercase)
	if( ctor_args ){
		StructType *st = sem_type->structType();
		std::string ctor_name = st->ident + "_constructor";
		ctor_decl = e->findFunc( ctor_name );

		// If arguments provided, constructor is required
		if( ctor_args->size() > 0 ){
			if( !ctor_decl || !(ctor_decl->kind & DECL_FUNC) ){
				ex( "Constructor not found in class '" + st->ident + "'" );
			}
		}

		// If constructor found, validate and prepare arguments
		if( ctor_decl && (ctor_decl->kind & DECL_FUNC) ){
			FuncType *f = ctor_decl->type->funcType();

			// Semantic analysis on arguments
			ctor_args->semant( e );

			// The constructor's first parameter is 'self', skip it for argument casting
			DeclSeq *params_without_self = d_new DeclSeq();
			for( int i = 1; i < f->params->size(); ++i ){
				params_without_self->decls.push_back( f->params->decls[i] );
			}
			ctor_args->castTo( params_without_self, e, f->cfunc );
			params_without_self->decls.clear();
			delete params_without_self;
		}else{
			// No constructor found, but empty args is OK - just create object
			ctor_decl = 0;
		}
	}

	return this;
}

TNode *NewNode::translate( Codegen *g ){
	TNode *obj = call( "__bbObjNew",global( "_t"+ident ) );

	// If we have constructor args, call the constructor
	if( ctor_args && ctor_decl ){
		FuncType *f = ctor_decl->type->funcType();

		// Build args: first the object (self), then the other args
		TNode *args_node = ctor_args->translate( g, f->cfunc );
		TNode *r = d_new TNode( IR_ARG, obj, args_node, 0 );

		TNode *l = global( "_f" + ident + "_Constructor" );
		TNode *ctor_call = d_new TNode( IR_CALL, l, r, (ctor_args->size() + 1) * 4 );

		// Sequence: create object, call constructor, return object
		// We need the object value after constructor, so use SEQ to execute both
		return d_new TNode( IR_SEQ, ctor_call, obj );
	}

	return obj;
}

#ifdef USE_LLVM
llvm::Value *NewNode::translate2( Codegen_LLVM *g ){
	auto objty=g->builder->CreateBitOrPointerCast( sem_type->structType()->objty,llvm::PointerType::get( g->bbObjType,0 ) );

	auto ty=sem_type->llvmType( g->context.get() );
	auto t=g->CallIntrinsic( "_bbObjNew",llvm::PointerType::get( g->bbObj,0 ),1,objty );
	llvm::Value *obj = g->builder->CreateBitOrPointerCast( t,ty );

	// If we have constructor args, call the constructor
	if( ctor_args && ctor_decl ){
		FuncType *f = ctor_decl->type->funcType();
		auto func = f->llvmFunction( ctor_decl->name, g );

		std::vector<llvm::Value*> args;

		// First argument is the object (self)
		args.push_back( obj );

		// Then the rest of the arguments
		for( int i = 0; i < ctor_args->size(); i++ ){
			args.push_back( ctor_args->exprs[i]->translate2( g ) );
		}

		g->builder->CreateCall( func, args );
	}

	return obj;
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

std::string NewNode::translate3( Codegen_C *g ){
	// Create a new object of this type
	std::string typePtr = "_t" + g->toCSafeName( ident );
	std::string obj = "_bbObjNew((BBObjType*)&" + typePtr + ")";

	// If we have constructor args, call the constructor
	if( ctor_args && ctor_decl ){
		// Need to store the object in a temp, call constructor, return temp
		std::string temp = g->newTemp();
		g->emitLine( "bb_obj_t " + temp + " = " + obj + ";" );

		// Build constructor call
		std::string ctorName = g->toCSafeName( "f" + ident + "_constructor" );
		std::string args = temp;
		for( int i = 0; i < ctor_args->size(); i++ ){
			args += ", " + ctor_args->exprs[i]->translate3( g );
		}
		g->emitLine( ctorName + "(" + args + ");" );

		return temp;
	}

	return obj;
}
#endif
