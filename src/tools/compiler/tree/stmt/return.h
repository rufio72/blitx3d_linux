#ifndef RETURN_NODE_H
#define RETURN_NODE_H

#include "node.h"
#include "../expr/node.h"

struct ReturnNode : public StmtNode{
	ExprNode *expr;
	std::string returnLabel;
	ReturnNode( ExprNode *e ):expr( e ){}
	~ReturnNode(){ delete expr; }
	void semant( Environ *e );
	void translate( Codegen *g );
	json toJSON( Environ *e );
#ifdef USE_LLVM
	void translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	void translate3( Codegen_C *g );
#endif
};

#endif
