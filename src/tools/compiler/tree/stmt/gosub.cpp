#include "gosub.h"

/////////////////////
// Gosub statement //
/////////////////////
void GosubNode::semant( Environ *e ){
	if( e->level>0 ) ex( "'Gosub' may not be used inside a function" );
	if( !e->findLabel( ident ) ) e->insertLabel( ident,-1,pos,-1 );
	ident=e->funcLabel+ident;
}

void GosubNode::translate( Codegen *g ){
	g->code( jsr( "_l"+ident ) );
}

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

void GosubNode::translate3( Codegen_C *g ){
	// Use GCC/Clang "labels as values" extension for computed goto
	std::string ret_label = "_gosub_ret_" + std::to_string((intptr_t)this);
	g->emitLine( "_bbPushGosub(&&" + ret_label + ");" );
	g->emitLine( "goto _l" + g->toCSafeName(ident) + ";" );
	g->emitLabel( ret_label );
}
#endif

#ifdef USE_LLVM
void GosubNode::translate2( Codegen_LLVM *g ){
	g->gosubUsed=true;

	auto func=g->builder->GetInsertBlock()->getParent();

	std::string label_cont=ident+"_"+std::string(itoa((bb_int_t)this))+"_cont";
	auto cont=g->getLabel( label_cont );
	func->insert( func->end(),cont );

	g->CallIntrinsic( "_bbPushGosub",g->voidTy,1,llvm::BlockAddress::get( cont ) );
	g->builder->CreateBr( g->getLabel( ident ) );

	g->builder->SetInsertPoint( cont );
}
#endif
