#include "decl_var.h"

//////////////////
// Declared var //
//////////////////

void DeclVarNode::semant( Environ *e ){
}

TNode *DeclVarNode::translate( Codegen *g ){
	if( sem_decl->kind==DECL_GLOBAL ) return global( "_v"+sem_decl->name );
	return local( sem_decl->offset );
}

#ifdef USE_LLVM
llvm::Value *DeclVarNode::load2( Codegen_LLVM *g ){
	llvm::Value *t=translate2( g );
	if( sem_type==Type::string_type ) return g->CallIntrinsic( "_bbStrLoad",sem_type->llvmType( g->context.get() ),1,t );
	return g->builder->CreateLoad( sem_decl->type->llvmType( g->context.get() ),sem_decl->ptr );
}

llvm::Value *DeclVarNode::translate2( Codegen_LLVM *g ){
	return sem_decl->ptr;
}
#endif

TNode *DeclVarNode::store( Codegen *g,TNode *n ){
	if( isObjParam() ){
		TNode *t=translate( g );
		return move( n,mem( t ) );
	}
	return VarNode::store( g,n );
}

bool DeclVarNode::isObjParam(){
	return sem_type->structType() && sem_decl->kind==DECL_PARAM;
}

json DeclVarNode::toJSON( Environ *e ){
	json tree;tree["@class"]="DeclVarNode";
	tree["sem_type"]=sem_type->toJSON();
	tree["sem_decl"]=sem_decl->toJSON();
	return tree;
}

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

std::string DeclVarNode::translate3( Codegen_C *g ){
	// Global variables use _v prefix, locals use _l prefix
	if( sem_decl->kind == DECL_GLOBAL ){
		return g->toCSafeName( "_v" + sem_decl->name );
	} else if( sem_decl->kind == DECL_LOCAL ){
		return g->toCSafeName( "_l" + sem_decl->name );
	} else if( sem_decl->kind == DECL_PARAM ){
		return g->toCSafeName( "_p" + sem_decl->name );
	}
	return g->toCSafeName( sem_decl->name );
}

std::string DeclVarNode::load3( Codegen_C *g ){
	std::string varName = translate3( g );

	// For string parameters, we need to make a copy because Blitz string functions
	// consume (delete) their arguments. Use _bbStrCopy for parameters.
	if( sem_decl->kind == DECL_PARAM ){
		if( sem_type == Type::string_type ){
			// Copy the parameter string to avoid double-free when used in expressions
			return "_bbStrCopy(" + varName + ")";
		}
		// Non-string parameters are passed by value, just return directly
		return varName;
	}

	// For local/global string variables, use _bbStrLoad for reference counting
	if( sem_type == Type::string_type ){
		return "_bbStrLoad(&" + varName + ")";
	}
	return varName;
}

void DeclVarNode::store3( Codegen_C *g, const std::string &value ){
	std::string varName = translate3( g );

	// For parameters, we should not modify them (copy-on-write semantics)
	// But if we do need to store, treat them like locals
	if( sem_type->structType() ){
		g->emitLine( "_bbObjStore(&" + varName + ", " + value + ");" );
	} else if( sem_type == Type::string_type ){
		g->emitLine( "_bbStrStore(&" + varName + ", " + value + ");" );
	} else {
		g->emitLine( varName + " = " + value + ";" );
	}
}
#endif
