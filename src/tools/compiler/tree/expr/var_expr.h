#ifndef VAR_EXPR_NODE_H
#define VAR_EXPR_NODE_H

#include "node.h"
#include "../var/node.h"

struct VarExprNode : public ExprNode{
	VarNode *var;
	VarExprNode( VarNode *v ):var(v){}
	~VarExprNode(){ delete var; }
	ExprNode *semant( Environ *e );
	TNode *translate( Codegen *g );
	json toJSON( Environ *e );
#ifdef USE_LLVM
	llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
#endif
};

#endif
