#include "while.h"
#include "../expr/const.h"

/////////////////////
// While statement //
/////////////////////
void WhileNode::semant( Environ *e ){
	expr=expr->semant( e );
	expr=expr->castTo( Type::int_type,e );
	std::string brk=e->setBreak( sem_brk=genLabel() );
	stmts->semant( e );
	e->setBreak( brk );
}

void WhileNode::translate( Codegen *g ){
	std::string loop=genLabel();
	if( ConstNode *c=expr->constNode() ){
		if( !c->intValue() ) return;
		g->label( loop );
		stmts->translate( g );
		g->code( jump( loop ) );
	}else{
		std::string cond=genLabel();
		g->code( jump( cond ) );
		g->label( loop );
		stmts->translate( g );
		debug( wendPos,g );
		g->label( cond );
		g->code( jumpt( expr->translate( g ),loop ) );
	}
	g->label( sem_brk );
}

#ifdef USE_LLVM
void WhileNode::translate2( Codegen_LLVM *g ){
	ConstNode *c=expr->constNode();
	if( c && !c->intValue() ) return;

	auto current_block = g->builder->GetInsertBlock();
	auto func=g->builder->GetInsertBlock()->getParent();

	auto loop=llvm::BasicBlock::Create( *g->context,"while",func );
	auto next=llvm::BasicBlock::Create( *g->context,"wend" );

	g->builder->CreateBr( loop );
	g->builder->SetInsertPoint( loop );

	if( !c ) {
		auto body=llvm::BasicBlock::Create( *g->context,"body" );

		auto v=expr->translate2( g );
		v=g->builder->CreateIntCast( v,llvm::Type::getInt1Ty( *g->context ),true ); // TODO: this is a hack and hopefully optimized out...

		g->builder->CreateCondBr( v,body,next );

		func->insert( func->end(),body );
		g->builder->SetInsertPoint( body );
	}

	auto oldBreakBlock=g->breakBlock;
	g->breakBlock=next;
	stmts->translate2( g );
	g->breakBlock=oldBreakBlock;

	g->builder->CreateBr( loop );

	func->insert( func->end(),next );
	g->builder->SetInsertPoint( next );
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"
#include "../expr/const.h"

void WhileNode::translate3( Codegen_C *g ){
	ConstNode *c = expr->constNode();
	if( c && !c->intValue() ) return;

	std::string loopLabel = g->getLabel( "while" );
	std::string condLabel = g->getLabel( "while_cond" );
	std::string brkLabel = g->getLabel( sem_brk );

	// Push break label for Exit statements
	g->breakLabelStack.push_back( brkLabel );

	if( c ){
		// Infinite loop (constant true condition)
		g->emitLabel( loopLabel );
		stmts->translate3( g );
		g->emitLine( "goto " + loopLabel + ";" );
	}else{
		// Normal while loop
		g->emitLine( "goto " + condLabel + ";" );
		g->emitLabel( loopLabel );
		stmts->translate3( g );
		g->emitLabel( condLabel );
		g->emitLine( "if (" + expr->translate3( g ) + ") goto " + loopLabel + ";" );
	}

	// Pop break label
	g->breakLabelStack.pop_back();
	g->emitLabel( brkLabel );
}
#endif

json WhileNode::toJSON( Environ *e ){
	json tree;tree["@class"]="WhileNode";
	tree["pos"]=pos;
	tree["wendPos"]=wendPos;
	tree["expr"]=expr->toJSON( e );
	tree["stmts"]=stmts->toJSON( e );
	tree["sem_brk"]=sem_brk;
	return tree;
}
