#ifndef VECTOR_VAR_NODE_H
#define VECTOR_VAR_NODE_H

#include "node.h"
#include "../expr/node.h"
#include "../expr/expr_seq.h"

struct VectorVarNode : public VarNode{
	ExprNode *expr;
	ExprSeqNode *exprs;
	VectorType *vec_type;
	VectorVarNode( ExprNode *e,ExprSeqNode *es ):expr(e),exprs(es){}
	~VectorVarNode(){ delete expr;delete exprs; }
	void semant( Environ *e );
	TNode *translate( Codegen *g );
	json toJSON( Environ *e );
#ifdef USE_LLVM
	llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
	std::string load3( Codegen_C *g );
	void store3( Codegen_C *g, const std::string &value );
#endif
};

#endif
