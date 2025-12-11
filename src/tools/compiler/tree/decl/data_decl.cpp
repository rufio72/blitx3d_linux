#include "data_decl.h"
#include "../expr/const.h"

//////////////////////
// Data declaration //
//////////////////////
void DataDeclNode::proto( DeclSeq *d,Environ *e ){
	expr=expr->semant( e );
	ConstNode *c=expr->constNode();
	if( !c ) ex( "Data expression must be constant" );
	if( expr->sem_type==Type::string_type ) str_label=genLabel();
}

void DataDeclNode::semant( Environ *e ){
}

void DataDeclNode::translate( Codegen *g ){
	if( expr->sem_type!=Type::string_type ) return;
	ConstNode *c=expr->constNode();
	g->s_data( c->stringValue(),str_label );
}

void DataDeclNode::transdata( Codegen *g ){
	ConstNode *c=expr->constNode();
	if( expr->sem_type==Type::int_type ){
		g->i_data( 1 );g->i_data( c->intValue() );
	}else if( expr->sem_type==Type::float_type ){
		float n=c->floatValue();
		g->i_data( 2 );g->i_data( *(int*)&n );
	}else{
		g->i_data( 4 );g->p_data( str_label );
	}
}

#ifdef USE_LLVM
void DataDeclNode::translate2( Codegen_LLVM *g ){
	ConstNode *c=expr->constNode();
	int ty;
	llvm::Constant *v=0;
	if( expr->sem_type==Type::int_type ){
		ty=1;
		v=(llvm::Constant*)c->translate2( g );
	}else if( expr->sem_type==Type::float_type ){
		ty=2;
		v=llvm::ConstantExpr::getBitCast( (llvm::Constant*)c->translate2( g ),g->intTy );
	}else{
		ty=4;
		v=llvm::ConstantExpr::getPtrToInt( g->constantString( c->stringValue() ),g->intTy );
	}

	values_idx=g->data_values.size();
	g->data_values.push_back( g->constantInt( ty ) );
	g->data_values.push_back( v );
}

void DataDeclNode::transdata2( Codegen_LLVM *g ){
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"
#include <cstring>

void DataDeclNode::translate3( Codegen_C *g ){
	// Create string constants during translate phase
	if( expr->sem_type==Type::string_type ){
		ConstNode *c=expr->constNode();
		str_label = g->constantString( c->stringValue() );
	}
}

void DataDeclNode::transdata3( Codegen_C *g ){
	ConstNode *c=expr->constNode();
	if( expr->sem_type==Type::int_type ){
		// Type 1 = int
		g->dataValues.push_back( "1" );
		g->dataValues.push_back( std::to_string( c->intValue() ) );
	}else if( expr->sem_type==Type::float_type ){
		// Type 2 = float (stored as int bits)
		float n=c->floatValue();
		int bits;
		memcpy(&bits, &n, sizeof(bits));
		g->dataValues.push_back( "2" );
		g->dataValues.push_back( std::to_string( bits ) );
	}else{
		// Type 4 = string pointer
		g->dataValues.push_back( "4" );
		g->dataValues.push_back( "(bb_int_t)" + str_label );
	}
}
#endif

json DataDeclNode::toJSON( Environ *e ){
	json tree;tree["@class"]="DataDeclNode";
	tree["pos"]=pos;
	tree["file"]=file;
	tree["expr"]=expr->toJSON( e );
	tree["str_label"]=str_label;
	return tree;
}
