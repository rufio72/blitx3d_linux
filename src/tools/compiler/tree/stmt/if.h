#ifndef IFNODE_H
#define IFNODE_H

#include "node.h"
#include "../expr/node.h"
#include "../stmt/stmt_seq.h"

struct IfNode : public StmtNode{
	ExprNode *expr;
	StmtSeqNode *stmts,*elseOpt;
	IfNode( ExprNode *e,StmtSeqNode *s,StmtSeqNode *o ):expr(e),stmts(s),elseOpt(o){}
	~IfNode(){ delete expr;delete stmts;delete elseOpt; }
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
