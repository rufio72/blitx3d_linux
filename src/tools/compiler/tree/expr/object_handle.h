#ifndef OBJECT_HANDLE_NODE_H
#define OBJECT_HANDLE_NODE_H

#include "node.h"

struct ObjectHandleNode : public ExprNode{
	ExprNode *expr;
	ObjectHandleNode( ExprNode *e ):expr(e){}
	~ObjectHandleNode(){ delete expr; }
	ExprNode *semant( Environ *e );
	TNode *translate( Codegen *g );
#ifdef USE_LLVM
	virtual llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
#endif

	json toJSON( Environ *e ){
		json tree;tree["@class"]="ObjectHandleNode";
		tree["sem_type"]=sem_type->toJSON();
		tree["expr"]=expr->toJSON( e );
		return tree;
	}
};

#endif
