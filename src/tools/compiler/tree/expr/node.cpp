#include "cast.h"

//////////////////////////////////
// Cast an expression to a type //
//////////////////////////////////
ExprNode *ExprNode::castTo( Type *ty,Environ *e ){
	if( !sem_type->canCastTo( ty ) ){
		ex( "Illegal type conversion" );
	}

	ExprNode *cast=d_new CastNode( this,ty );
	return cast->semant( e );
}

#ifdef USE_LLVM
llvm::Value *ExprNode::translate2( Codegen_LLVM *g ){
	std::cerr<<"translate2 missing implementation for "<<typeid(*this).name()<<std::endl;
	abort();
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"
std::string ExprNode::translate3( Codegen_C *g ){
	std::cerr<<"translate3 missing implementation for "<<typeid(*this).name()<<std::endl;
	abort();
	return "";
}
#endif
