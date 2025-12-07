#include "func_decl.h"

//////////////////////////
// Function Declaration //
//////////////////////////
void FuncDeclNode::proto( DeclSeq *d,Environ *e ){
	Type *t=tagType( tag,e );if( !t ) t=Type::int_type;
	a_ptr<DeclSeq> decls( d_new DeclSeq() );
	params->proto( decls,e );
	sem_type=d_new FuncType( t,decls.release(),false,false );
	// Include access modifier in the declaration kind
	Decl *decl = d->insertDecl( ident,sem_type,DECL_FUNC | accessMod );
	if( !decl ){
		delete sem_type;ex( "duplicate identifier" );
	}
	// Set owner class if this is a method
	if( !ownerClassName.empty() ){
		Type *ownerType = e->findType( ownerClassName );
		if( ownerType && ownerType->structType() ){
			decl->ownerClass = ownerType->structType();
		}
	}
	e->types.push_back( sem_type );
}

void FuncDeclNode::semant( Environ *e ){

	sem_env=d_new Environ( genLabel(),sem_type->returnType,1,e );
	DeclSeq *decls=sem_env->decls;

	int k;
	for( k=0;k<sem_type->params->size();++k ){
		Decl *d=sem_type->params->decls[k];
		if( !decls->insertDecl( d->name,d->type,d->kind ) ) ex( "duplicate identifier" );
	}

	stmts->semant( sem_env );
}

void FuncDeclNode::translate( Codegen *g ){

	//var offsets
	int size=enumVars( sem_env );

	//enter function
	g->enter( "_f"+ident,size );

	//initialize locals
	TNode *t=createVars( sem_env );
	if( t ) g->code( t );
	if( g->debug ){
		std::string t=genLabel();
		g->s_data( ident,t );
		g->code( call( "__bbDebugEnter",local(0),iconst((bb_int_t)sem_env),global(t) ) );
	}

	//translate statements
	stmts->translate( g );

	for( int k=0;k<sem_env->labels.size();++k ){
		if( sem_env->labels[k]->def<0 )	ex( "Undefined label",sem_env->labels[k]->ref );
	}

	//leave the function
	g->label( sem_env->funcLabel+"_leave" );
	t=deleteVars( sem_env );
	if( g->debug ) t=d_new TNode( IR_SEQ,call( "__bbDebugLeave" ),t );
	g->leave( t,sem_type->params->size()*4 );
}

#ifdef USE_LLVM
#include <llvm/IR/Verifier.h>
#include <llvm/IR/ValueSymbolTable.h>

void FuncDeclNode::translate2( Codegen_LLVM *g ){
	auto func=sem_type->llvmFunction( ident,g );
	auto block = llvm::BasicBlock::Create( *g->context,"entry",func );
	g->builder->SetInsertPoint( block );

	for( int k=0;k<sem_type->params->size();k++ ){
		Decl *d=sem_env->decls->decls[k];
		d->ptr=g->builder->CreateAlloca( d->type->llvmType( g->context.get() ),nullptr,d->name );
		g->builder->CreateStore( g->builder->GetInsertBlock()->getParent()->getArg( k ),d->ptr );
	}

	createVars2( sem_env,g );

	stmts->translate2( g );

	auto final_block=g->builder->GetInsertBlock();
	if( final_block->size()==0 ){
		final_block->eraseFromParent();
	}
}
#endif

json FuncDeclNode::toJSON( Environ *e ){
	json tree;tree["@class"]="FuncDeclNode";
	tree["pos"]=pos;
	tree["file"]=file;
	tree["ident"]=ident;
	tree["tag"]=tag;
	tree["types"]=json::array();
	for( int k=0;k<sem_env->types.size();++k ){
		tree["types"].push_back( sem_env->types[k]->toFullJSON() );
	}
	tree["locals"]=json::array();
	for( int k=0;k<sem_env->decls->size();++k ){
		Decl *d=sem_env->decls->decls[k];
		if( d->kind!=DECL_LOCAL ) continue;
		tree["locals"].push_back( d->toJSON() );
	}
	tree["stmts"]=stmts->toJSON( sem_env );
	tree["sem_type"]=sem_type->toJSON( );
	return tree;
}

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

void FuncDeclNode::translate3( Codegen_C *g ){
	// Get return type
	std::string retType;
	if( sem_type->returnType == Type::int_type ) retType = "bb_int_t";
	else if( sem_type->returnType == Type::float_type ) retType = "bb_float_t";
	else if( sem_type->returnType == Type::string_type ) retType = "bb_string_t";
	else if( sem_type->returnType->structType() ) retType = "bb_obj_t";
	else retType = "bb_int_t";

	// Build parameter list
	std::vector<std::pair<std::string, std::string>> cParams;
	for( int k=0; k<sem_type->params->size(); ++k ){
		Decl *d = sem_type->params->decls[k];
		std::string ptype;
		if( d->type == Type::int_type ) ptype = "bb_int_t";
		else if( d->type == Type::float_type ) ptype = "bb_float_t";
		else if( d->type == Type::string_type ) ptype = "bb_string_t";
		else if( d->type->structType() ) ptype = "bb_obj_t";
		else ptype = "bb_int_t";
		std::string pname = g->toCSafeName( "_p" + d->name );
		cParams.push_back( std::make_pair( ptype, pname ) );
	}

	// Begin function
	g->beginFunction( "f" + ident, retType, cParams );

	// Create local variables
	createVars3( sem_env, g );

	// Translate statements
	stmts->translate3( g );

	g->endFunction();
}
#endif
