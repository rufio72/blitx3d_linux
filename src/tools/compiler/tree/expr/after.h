#ifndef AFTER_NODE_H
#define AFTER_NODE_H

#include "node.h"

struct AfterNode : public ExprNode{
	ExprNode *expr;
	AfterNode( ExprNode *e ):expr(e){}
	~AfterNode(){ delete expr; }
	ExprNode *semant( Environ *e );
	TNode *translate( Codegen *g );
#ifdef USE_LLVM
	virtual llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
#endif

	json toJSON( Environ *e );
};

#endif
