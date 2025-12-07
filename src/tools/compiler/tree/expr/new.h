#ifndef NEW_NODE_H
#define NEW_NODE_H

#include "node.h"
#include "expr_seq.h"

struct NewNode : public ExprNode{
	std::string ident;
	ExprSeqNode *ctor_args;  // Constructor arguments (may be null)
	Decl *ctor_decl;         // Constructor function declaration (may be null)

	NewNode( const std::string &i, ExprSeqNode *args=0 ):ident(i),ctor_args(args),ctor_decl(0){}
	~NewNode(){ delete ctor_args; }
	ExprNode *semant( Environ *e );
	TNode *translate( Codegen *g );
#ifdef USE_LLVM
	virtual llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
#endif

	json toJSON( Environ *e ){
		json tree;tree["@class"]="NewNode";
		tree["sem_type"]=sem_type->toJSON();
		tree["ident"]=ident;
		if( ctor_args ) tree["ctor_args"]=ctor_args->toJSON( e );
		return tree;
	}
};

#endif
