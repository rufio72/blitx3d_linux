#ifndef VAR_NODE_H
#define VAR_NODE_H

#include "../node.h"

#ifdef USE_GCC_BACKEND
class Codegen_C;
#endif

struct VarNode : public Node{
	Type *sem_type;

	//get set var
	TNode *load( Codegen *g );
	virtual TNode *store( Codegen *g,TNode *n );
	virtual bool isObjParam();

	//addr of var
	virtual void semant( Environ *e )=0;
	virtual TNode *translate( Codegen *g )=0;

#ifdef USE_LLVM
	virtual llvm::Value *load2( Codegen_LLVM *g );
	virtual void store2( Codegen_LLVM *g,llvm::Value *v );
	virtual llvm::Value *translate2( Codegen_LLVM *g ); // TODO: make abstract once everything is implemented
#endif
#ifdef USE_GCC_BACKEND
	virtual std::string load3( Codegen_C *g );
	virtual void store3( Codegen_C *g, const std::string &value );
	virtual std::string translate3( Codegen_C *g ); // returns the variable name/address
#endif
};

#endif
