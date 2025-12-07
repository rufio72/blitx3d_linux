
#ifndef STMTNODE_H
#define STMTNODE_H

#include "../node.h"

#ifdef USE_GCC_BACKEND
class Codegen_C;
#endif

struct StmtNode : public Node{
	int pos;	//offset in source stream
	StmtNode():pos(-1){}
	void debug( int pos,Codegen *g );
#ifdef USE_LLVM
	void debug2( int pos,Codegen_LLVM *g );
#endif

	virtual void semant( Environ *e ){}
	virtual void translate( Codegen *g ){}
#ifdef USE_LLVM
	virtual void translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	virtual void translate3( Codegen_C *g );
#endif
};

#endif
