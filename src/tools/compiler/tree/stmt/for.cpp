#include "for.h"
#include "../expr/const.h"
#include "../var/node.h"

///////////////////
// For/Next loop //
///////////////////
ForNode::ForNode( VarNode *var,ExprNode *from,ExprNode *to,ExprNode *step,StmtSeqNode *ss,int np )
:var(var),fromExpr(from),toExpr(to),stepExpr(step),stmts(ss),nextPos(np){
}

ForNode::~ForNode(){
	delete stmts;
	delete stepExpr;
	delete toExpr;
	delete fromExpr;
	delete var;
}

void ForNode::semant( Environ *e ){
	var->semant( e );
	Type *ty=var->sem_type;
	if( ty->constType() ) ex( "Index variable can not be constant" );
	if( ty!=Type::int_type && ty!=Type::float_type ){
		ex( "index variable must be integer or real" );
	}
	fromExpr=fromExpr->semant( e );
	fromExpr=fromExpr->castTo( ty,e );
	toExpr=toExpr->semant( e );
	toExpr=toExpr->castTo( ty,e );
	stepExpr=stepExpr->semant( e );
	stepExpr=stepExpr->castTo( ty,e );

	if( !stepExpr->constNode() ) ex( "Step value must be constant" );

	std::string brk=e->setBreak( sem_brk=genLabel() );
	stmts->semant( e );
	e->setBreak( brk );
}

void ForNode::translate( Codegen *g ){

	TNode *t;Type *ty=var->sem_type;

	//initial assignment
	g->code( var->store( g,fromExpr->translate( g ) ) );

	std::string cond=genLabel();
	std::string loop=genLabel();
	g->code( jump( cond ) );
	g->label( loop );
	stmts->translate( g );

	//execute the step part
	debug( nextPos,g );
	int op=ty==Type::int_type ? IR_ADD : IR_FADD;
	t=d_new TNode( op,var->load( g ),stepExpr->translate( g ) );
	g->code( var->store( g,t ) );

	//test for loop cond
	g->label( cond );
	op=stepExpr->constNode()->floatValue()>0 ? '>' : '<';
	t=compare( op,var->load( g ),toExpr->translate( g ),ty );
	g->code( jumpf( t,loop ) );

	g->label( sem_brk );
}

#ifdef USE_LLVM
void ForNode::translate2( Codegen_LLVM *g ){
	auto func=g->builder->GetInsertBlock()->getParent();

	llvm::Value *t;Type *ty=var->sem_type;

	auto cond=llvm::BasicBlock::Create( *g->context,"for_cond",func );
	auto loop=llvm::BasicBlock::Create( *g->context,"for_loop",func );
	auto cont=llvm::BasicBlock::Create( *g->context,"for_cont",func );

	//initial assignment
	var->store2( g,fromExpr->translate2( g ) );
	g->builder->CreateBr( cond );

	//the loop
	g->builder->SetInsertPoint( loop );

	auto oldBreakBlock=g->breakBlock;
	g->breakBlock=cont;
	stmts->translate2( g );
	g->breakBlock=oldBreakBlock;

	//execute the step part
	auto bop=ty==Type::int_type ? llvm::Instruction::BinaryOps::Add : llvm::Instruction::BinaryOps::FAdd;
	t=g->builder->CreateBinOp( bop,var->load2( g ),stepExpr->translate2( g ) );
	var->store2( g,t );
	g->builder->CreateBr( cond );

	//test for loop cond
	g->builder->SetInsertPoint( cond );
	char op=stepExpr->constNode()->floatValue()>0 ? '>' : '<';
	t=compare2( op,var->load2( g ),toExpr->translate2( g ),ty,g );
	g->builder->CreateCondBr( t,cont,loop );

	g->builder->SetInsertPoint( cont );
}
#endif

json ForNode::toJSON( Environ *e ){
	json tree;tree["@class"]="ForNode";
	tree["pos"]=pos;
	tree["nextPos"]=nextPos;
	tree["var"]=var->toJSON( e );
	tree["fromExpr"]=fromExpr->toJSON( e );
	tree["toExpr"]=toExpr->toJSON( e );
	tree["stepExpr"]=stepExpr->toJSON( e );
	tree["stmts"]=stmts->toJSON( e );
	tree["sem_brk"]=sem_brk;
	return tree;
}

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

void ForNode::translate3( Codegen_C *g ){
	Type *ty = var->sem_type;

	// Initial assignment
	std::string fromVal = fromExpr->translate3( g );
	var->store3( g, fromVal );

	// Generate labels
	std::string condLabel = g->getLabel( "for_cond" );
	std::string loopLabel = g->getLabel( "for_loop" );
	std::string brkLabel = g->getLabel( sem_brk );

	// Jump to condition check
	g->emitLine( "goto " + condLabel + ";" );

	// Push break label for Exit statements
	g->breakLabelStack.push_back( brkLabel );

	// Loop body
	g->emitLabel( loopLabel );
	stmts->translate3( g );

	// Pop break label
	g->breakLabelStack.pop_back();

	// Step increment
	std::string varLoad = var->load3( g );
	std::string stepVal = stepExpr->translate3( g );
	if( ty == Type::int_type ){
		var->store3( g, "(" + varLoad + " + " + stepVal + ")" );
	} else {
		var->store3( g, "(" + varLoad + " + " + stepVal + ")" );
	}

	// Condition check
	g->emitLabel( condLabel );
	std::string toVal = toExpr->translate3( g );
	varLoad = var->load3( g );

	char op = stepExpr->constNode()->floatValue() > 0 ? '>' : '<';
	std::string cmp;
	if( op == '>' ){
		cmp = varLoad + " <= " + toVal;
	} else {
		cmp = varLoad + " >= " + toVal;
	}
	g->emitLine( "if (" + cmp + ") goto " + loopLabel + ";" );

	// Break label
	g->emitLabel( brkLabel );
}
#endif
