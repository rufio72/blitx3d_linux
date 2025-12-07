#include "repeat.h"
#include "../expr/const.h"

////////////////////////////
// Repeat...Until/Forever //
////////////////////////////
void RepeatNode::semant( Environ *e ){
	sem_brk=genLabel();
	std::string brk=e->setBreak( sem_brk );
	stmts->semant( e );
	e->setBreak( brk );
	if( expr ){
		expr=expr->semant( e );
		expr=expr->castTo( Type::int_type,e );
	}
}

void RepeatNode::translate( Codegen *g ){

	std::string loop=genLabel();
	g->label( loop );
	stmts->translate( g );
	debug( untilPos,g );

	if( ConstNode *c=expr ? expr->constNode() : 0 ){
		if( !c->intValue() ) g->code( jump( loop ) );
	}else{
		if( expr ) g->code( jumpf( expr->translate( g ),loop ) );
		else g->code( jump( loop ) );
	}
	g->label( sem_brk );
}

#ifdef USE_LLVM
void RepeatNode::translate2( Codegen_LLVM *g ){
	auto current_block = g->builder->GetInsertBlock();
	auto func=g->builder->GetInsertBlock()->getParent();

	auto loop=llvm::BasicBlock::Create( *g->context,"repeat",func );
	auto iter=llvm::BasicBlock::Create( *g->context,"repeat_iter" );
	auto cont=llvm::BasicBlock::Create( *g->context,"repeat_cont" );

	g->builder->CreateBr( loop );
	g->builder->SetInsertPoint( loop );

	auto oldBreakBlock=g->breakBlock;
	g->breakBlock=cont;
	stmts->translate2( g );
	g->builder->CreateBr( iter );
	g->breakBlock=oldBreakBlock;

	func->insert( func->end(),iter );
	g->builder->SetInsertPoint( iter );
	if( expr ) {
		auto cond=expr->translate2( g );
		auto cmp=cond;

		if( !llvm::dyn_cast<llvm::CmpInst>(cmp) ) {
			cmp=compare2( '=',cond,0,expr->sem_type,g );
		}

		g->builder->CreateCondBr( cmp,loop,cont );
	}else{
		g->builder->CreateBr( loop );
	}

	func->insert( func->end(),cont );
	g->builder->SetInsertPoint( cont );
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"
#include "../expr/const.h"

void RepeatNode::translate3( Codegen_C *g ){
	std::string loopLabel = g->getLabel( "repeat" );
	std::string brkLabel = g->getLabel( sem_brk );

	// Push break label for Exit statements
	g->breakLabelStack.push_back( brkLabel );

	g->emitLabel( loopLabel );
	stmts->translate3( g );

	// Pop break label
	g->breakLabelStack.pop_back();

	if( ConstNode *c = expr ? expr->constNode() : 0 ){
		// Constant condition
		if( !c->intValue() ){
			// Constant false = keep looping
			g->emitLine( "goto " + loopLabel + ";" );
		}
		// Constant true = exit loop (fall through to break)
	}else{
		if( expr ){
			// Until condition: exit loop if true
			g->emitLine( "if (!(" + expr->translate3( g ) + ")) goto " + loopLabel + ";" );
		}else{
			// Forever loop (no condition)
			g->emitLine( "goto " + loopLabel + ";" );
		}
	}
	g->emitLabel( brkLabel );
}
#endif

json RepeatNode::toJSON( Environ *e ){
	json tree;tree["@class"]="RepeatNode";
	tree["pos"]=pos;
	tree["untilPos"]=untilPos;
	tree["stmts"]=stmts->toJSON( e );
	if( expr ) tree["expr"]=expr->toJSON( e );
	tree["sem_brk"]=sem_brk;
	return tree;
}
