#ifndef GOSUBNODE_H
#define GOSUBNODE_H

#include "node.h"

struct GosubNode : public StmtNode{
	std::string ident;
	GosubNode( const std::string &s ):ident(s){}
	void semant( Environ *e );
	void translate( Codegen *g );
#ifdef USE_LLVM
	virtual void translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	void translate3( Codegen_C *g );
#endif

	json toJSON( Environ *e ){
		json tree;tree["@class"]="GosubNode";
		tree["pos"]=pos;
		tree["ident"]=ident;
		return tree;
	}
};

#endif
