#ifndef FIELD_VAR_NODE_H
#define FIELD_VAR_NODE_H

#include "node.h"
#include "../expr/node.h"

struct FieldVarNode : public VarNode{
	ExprNode *expr;
	std::string ident,tag;
	Decl *sem_field;
	FieldVarNode( ExprNode *e,const std::string &i,const std::string &t ):expr(e),ident(i),tag(t){}
	~FieldVarNode(){ delete expr; }
	void semant( Environ *e );
	TNode *translate( Codegen *g );
#ifdef USE_LLVM
	virtual llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
	std::string load3( Codegen_C *g );
	void store3( Codegen_C *g, const std::string &value );
#endif

	json toJSON( Environ *e );
};

#endif
