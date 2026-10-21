#ifndef INT_CONST_NODE_H
#define INT_CONST_NODE_H

#include "const.h"

struct IntConstNode : public ConstNode{
	bb_int_t value;
	IntConstNode( bb_int_t n );
	TNode *translate( Codegen *g );
	bb_int_t intValue();
	float floatValue();
	std::string stringValue();
	json toJSON( Environ *e );
#ifdef USE_LLVM
	llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
#endif
};

#endif
