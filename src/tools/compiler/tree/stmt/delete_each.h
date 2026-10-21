#ifndef DELETEEACHNODE_H
#define DELETEEACHNODE_H

#include "node.h"

struct DeleteEachNode : public StmtNode{
	std::string typeIdent;
	Type *type;
	DeleteEachNode( const std::string &t ):typeIdent(t){}
	void semant( Environ *e );
	void translate( Codegen *g );
#ifdef USE_LLVM
	virtual void translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	virtual void translate3( Codegen_C *g );
#endif

	json toJSON( Environ *e ){
		json tree;tree["@class"]="DeleteEachNode";
		tree["pos"]=pos;
		tree["typeIdent"]=typeIdent;
		return tree;
	}
};

#endif
