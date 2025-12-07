#include "vector_var.h"
#include "../expr/const.h"

////////////////
// Vector var //
////////////////
void VectorVarNode::semant( Environ *e ){
	expr=expr->semant( e );
	vec_type=expr->sem_type->vectorType();
	if( !vec_type ) ex( "Variable must be a Blitz array" );
	if( vec_type->sizes.size()!=exprs->size() ) ex( "Incorrect number of subscripts" );
	exprs->semant( e );
	exprs->castTo( Type::int_type,e );
	for( int k=0;k<exprs->size();++k ){
		if( ConstNode *t=exprs->exprs[k]->constNode() ){
			if( t->intValue()>=vec_type->sizes[k] ){
				ex( "Blitz array subscript out of range" );
			}
		}
	}
	sem_type=vec_type->elementType;
}

TNode *VectorVarNode::translate( Codegen *g ){
	int sz=4;
	TNode *t=0;
	for( int k=0;k<exprs->size();++k ){
		TNode *p;
		ExprNode *e=exprs->exprs[k];
		if( ConstNode *t=e->constNode() ){
			p=iconst( t->intValue() * sz );
		}else{
			p=e->translate( g );
			if( g->debug ){
				p=jumpge( p,iconst( vec_type->sizes[k] ),"__bbVecBoundsEx" );
			}
			p=mul( p,iconst( sz ) );
		}
		sz=sz*vec_type->sizes[k];
		t=t ? add( t,p ) : p;
	}
	return add( t,expr->translate( g ) );
}

#ifdef USE_LLVM
llvm::Value *VectorVarNode::translate2( Codegen_LLVM *g ){
	int sz=1;
	llvm::Value *t=0;
	for( int k=0;k<exprs->size();++k ){
		ExprNode *e=exprs->exprs[k];
		auto p=e->translate2( g );
		// if( g->debug ){
		// 	p=jumpge( p,iconst( vec_type->sizes[k] ),"__bbVecBoundsEx" );
		// }
		p=g->builder->CreateMul( p,llvm::ConstantInt::get( *g->context,llvm::APInt( 64,sz ) ) );

		sz=sz*vec_type->sizes[k]/4;
		t=t ? g->builder->CreateAdd( t,p ) : p;
	}


	std::vector<llvm::Value*> indices;
	indices.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt(32, 0) ) );
	indices.push_back( t );

	auto ty=vec_type->llvmType( g->context.get() );
	auto el=g->builder->CreateGEP( vec_type->ty,expr->translate2( g ),indices );
	return el;
}
#endif

json VectorVarNode::toJSON( Environ *e ){
	json tree;tree["@class"]="VectorVarNode";
	tree["sem_type"]=sem_type->toJSON();
	tree["expr"]=expr->toJSON( e );
	tree["exprs"]=exprs->toJSON( e );
	tree["vec_type"]=vec_type->toJSON();
	return tree;
}

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

std::string VectorVarNode::translate3( Codegen_C *g ){
	// Calculate the linear offset into the vector
	// For a multi-dimensional array arr[i][j][k] with sizes [S0][S1][S2]:
	// offset = i*S1*S2 + j*S2 + k
	// Each element is 4 bytes (like the original code)

	std::string offset = "";
	int stride = 1;
	for( int k = exprs->size() - 1; k >= 0; --k ){
		std::string idx = exprs->exprs[k]->translate3( g );
		if( stride == 1 ){
			offset = idx;
		} else {
			if( offset.empty() ){
				offset = "(" + idx + ") * " + std::to_string( stride );
			} else {
				offset = "(" + idx + ") * " + std::to_string( stride ) + " + " + offset;
			}
		}
		stride *= vec_type->sizes[k];
	}

	// The expr is the pointer to the vector data
	std::string basePtr = expr->translate3( g );

	// Access element at offset - vectors store elements directly
	// Vector is stored as: type metadata followed by actual data
	// The pointer we get is to the data array
	return "((" + g->toCType( sem_type ) + "*)(" + basePtr + "))[" + offset + "]";
}

std::string VectorVarNode::load3( Codegen_C *g ){
	std::string offset = "";
	int stride = 1;
	for( int k = exprs->size() - 1; k >= 0; --k ){
		std::string idx = exprs->exprs[k]->translate3( g );
		if( stride == 1 ){
			offset = idx;
		} else {
			if( offset.empty() ){
				offset = "(" + idx + ") * " + std::to_string( stride );
			} else {
				offset = "(" + idx + ") * " + std::to_string( stride ) + " + " + offset;
			}
		}
		stride *= vec_type->sizes[k];
	}

	std::string basePtr = expr->translate3( g );

	if( sem_type == Type::string_type ){
		return "_bbStrLoad((bb_string_t*)&((" + g->toCType( sem_type ) + "*)(" + basePtr + "))[" + offset + "])";
	}
	return "((" + g->toCType( sem_type ) + "*)(" + basePtr + "))[" + offset + "]";
}

void VectorVarNode::store3( Codegen_C *g, const std::string &value ){
	std::string offset = "";
	int stride = 1;
	for( int k = exprs->size() - 1; k >= 0; --k ){
		std::string idx = exprs->exprs[k]->translate3( g );
		if( stride == 1 ){
			offset = idx;
		} else {
			if( offset.empty() ){
				offset = "(" + idx + ") * " + std::to_string( stride );
			} else {
				offset = "(" + idx + ") * " + std::to_string( stride ) + " + " + offset;
			}
		}
		stride *= vec_type->sizes[k];
	}

	std::string basePtr = expr->translate3( g );

	if( sem_type == Type::string_type ){
		g->emitLine( "_bbStrStore((bb_string_t*)&((" + g->toCType( sem_type ) + "*)(" + basePtr + "))[" + offset + "], " + value + ");" );
	} else if( sem_type->structType() ){
		g->emitLine( "_bbObjStore((void**)&((" + g->toCType( sem_type ) + "*)(" + basePtr + "))[" + offset + "], " + value + ");" );
	} else {
		g->emitLine( "((" + g->toCType( sem_type ) + "*)(" + basePtr + "))[" + offset + "] = " + value + ";" );
	}
}
#endif
