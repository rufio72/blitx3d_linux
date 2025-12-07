#include "node.h"

#ifdef USE_LLVM
void StmtNode::translate2( Codegen_LLVM *g ){
	std::cerr<<"translate2 missing implementation for "<<typeid(*this).name()<<std::endl;
	abort();
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"
void StmtNode::translate3( Codegen_C *g ){
	std::cerr<<"translate3 missing implementation for "<<typeid(*this).name()<<std::endl;
	abort();
}
#endif
