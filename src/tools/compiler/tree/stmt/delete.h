#ifndef DELETENODE_H
#define DELETENODE_H

#include "node.h"
#include "../expr/node.h"

struct DeleteNode : public StmtNode{
	ExprNode *expr;
	Decl *dtor_decl;  // Destructor declaration (may be null)
	DeleteNode( ExprNode *e ):expr(e),dtor_decl(0){}
	~DeleteNode(){ delete expr; }
	void semant( Environ *e );
	void translate( Codegen *g );
#ifdef USE_LLVM
	virtual void translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	void translate3( Codegen_C *g );
#endif

	json toJSON( Environ *e ){
		json tree;tree["@class"]="DeleteNode";
		tree["pos"]=pos;
		tree["expr"]=expr->toJSON( e );
		return tree;
	}
};

#endif
