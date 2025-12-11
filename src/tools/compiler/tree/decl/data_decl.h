#ifndef DATADECLNODE_H
#define DATADECLNODE_H

#include "node.h"
#include "../expr/node.h"

struct DataDeclNode : public DeclNode{
	ExprNode *expr;
	std::string str_label;
	int str_index;  // Index in string constants for GCC backend
	DataDeclNode( ExprNode *e ):expr(e),str_index(-1){}
	~DataDeclNode(){ delete expr; }
	void proto( DeclSeq *d,Environ *e );
	void semant( Environ *e );
	void translate( Codegen *g );
	void transdata( Codegen *g );
#ifdef USE_LLVM
	int values_idx;
	virtual void translate2( Codegen_LLVM *g );
	virtual void transdata2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	virtual void translate3( Codegen_C *g );
	virtual void transdata3( Codegen_C *g );
#endif

	json toJSON( Environ *e );
};

#endif
