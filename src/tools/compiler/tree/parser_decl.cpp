
// Declaration parsing (Local/Global/Const, Dim, Function, Type, Class,
// Method): split out of parser.cpp to keep files small.

#include "../std.h"
#include "parser.h"

#include "decl/func_decl.h"
#include "decl/struct_decl.h"
#include "decl/class_decl.h"
#include "decl/var_decl.h"
#include "decl/vector_decl.h"

#include "stmt/return.h"

DeclNode *Parser::parseVarDecl( int kind,bool constant ){
	int pos=toker->pos();
	std::string ident=parseIdent();
	std::string tag=parseTypeTag();
	DeclNode *d;
	if( toker->curr()=='[' ){
		if( constant ) ex( "Blitz arrays may not be constant" );
		toker->next();
		a_ptr<ExprSeqNode> exprs( parseExprSeq() );
		if( exprs->size()!=1 || toker->curr()!=']' ) exp( "']'" );
		toker->next();
		d=d_new VectorDeclNode( ident,tag,exprs.release(),kind );
	}else{
		ExprNode *expr=0;
		if( toker->curr()=='=' ){
			toker->next();expr=parseExpr( false );
		}else if( constant ) ex( "Constants must be initialized" );
		d=d_new VarDeclNode( ident,tag,kind,constant,expr );
	}
	d->pos=pos;d->file=incfile;
	return d;
}

DimNode *Parser::parseArrayDecl(){
	int pos=toker->pos();
	std::string ident=parseIdent();
	std::string tag=parseTypeTag();
	if( toker->curr()!='(' ) exp( "'('" );
	toker->next();a_ptr<ExprSeqNode> exprs( parseExprSeq() );
	if( toker->curr()!=')' ) exp( "')'" );
	if( !exprs->size() ) ex( "can't have a 0 dimensional array" );
	toker->next();
	DimNode *d=d_new DimNode( ident,tag,exprs.release() );
	arrayDecls[ident]=d;
	d->pos=pos;
	return d;
}

DeclNode *Parser::parseFuncDecl(){
	int pos=toker->pos();
	std::string ident=parseIdent();
	std::string tag=parseTypeTag();
	if( toker->curr()!='(' ) exp( "'('" );
	a_ptr<DeclSeqNode> params( d_new DeclSeqNode() );
	if( toker->next()!=')' ){
		for(;;){
			params->push_back( parseVarDecl( DECL_PARAM,false ) );
			if( toker->curr()!=',' ) break;
			toker->next();
		}
		if( toker->curr()!=')' ) exp( "')'" );
	}
	toker->next();
	a_ptr<StmtSeqNode> stmts( parseStmtSeq( STMTS_BLOCK ) );
	if( toker->curr()!=ENDFUNCTION ) exp( "'End Function'" );
	StmtNode *ret=d_new ReturnNode(0);ret->pos=toker->pos();
	stmts->push_back( ret );toker->next();
	DeclNode *d=d_new FuncDeclNode( ident,tag,params.release(),stmts.release() );
	d->pos=pos;d->file=incfile;
	return d;
}

DeclNode *Parser::parseStructDecl(){
	int pos=toker->pos();
	std::string ident=parseIdent();
	while( toker->curr()=='\n' ) toker->next();
	a_ptr<DeclSeqNode> fields( d_new DeclSeqNode() );
	while( toker->curr()==FIELD ){
		do{
			toker->next();
			fields->push_back( parseVarDecl( DECL_FIELD,false ) );
		}while( toker->curr()==',' );
		while( toker->curr()=='\n' ) toker->next();
	}
	if( toker->curr()!=ENDTYPE ) exp( "'Field' or 'End Type'" );
	toker->next();
	DeclNode *d=d_new StructDeclNode( ident,fields.release() );
	d->pos=pos;d->file=incfile;
	return d;
}

DeclNode *Parser::parseClassDecl(){
	int pos=toker->pos();
	std::string ident=parseIdent();
	std::string superName;
	// Check for Extends keyword
	if( toker->curr()==EXTENDS ){
		toker->next();
		superName=parseIdent();
	}
	while( toker->curr()=='\n' ) toker->next();
	a_ptr<DeclSeqNode> fields( d_new DeclSeqNode() );
	a_ptr<DeclSeqNode> methods( d_new DeclSeqNode() );
	while( toker->curr()==FIELD || toker->curr()==METHOD || toker->curr()==STATIC ||
	       toker->curr()==PRIVATE || toker->curr()==PUBLIC || toker->curr()==PROTECTED ){
		// Check for access modifier
		int accessMod = 0;  // Default is public (no flag)
		if( toker->curr()==PRIVATE ){
			accessMod = DECL_PRIVATE;
			toker->next();
		}else if( toker->curr()==PUBLIC ){
			accessMod = DECL_PUBLIC;
			toker->next();
		}else if( toker->curr()==PROTECTED ){
			accessMod = DECL_PROTECTED;
			toker->next();
		}

		if( toker->curr()==FIELD ){
			do{
				toker->next();
				DeclNode *fieldDecl = parseVarDecl( DECL_FIELD | accessMod, false );
				fields->push_back( fieldDecl );
			}while( toker->curr()==',' );
		}else if( toker->curr()==STATIC ){
			toker->next();
			if( toker->curr()!=METHOD ) exp( "'Method'" );
			toker->next();
			DeclNode *methodDecl = parseMethodDecl( ident, true, accessMod );  // isStatic=true
			methods->push_back( methodDecl );
		}else if( toker->curr()==METHOD ){
			toker->next();
			DeclNode *methodDecl = parseMethodDecl( ident, false, accessMod );  // isStatic=false
			methods->push_back( methodDecl );
		}else if( accessMod ){
			// Access modifier without Field or Method
			exp( "'Field' or 'Method'" );
		}
		while( toker->curr()=='\n' ) toker->next();
	}
	if( toker->curr()!=ENDCLASS ) exp( "'Field', 'Method', 'Static', 'Private', 'Public', 'Protected' or 'End Class'" );
	toker->next();
	DeclNode *d=d_new ClassDeclNode( ident,superName,fields.release(),methods.release() );
	d->pos=pos;d->file=incfile;
	return d;
}

DeclNode *Parser::parseMethodDecl( const std::string &className, bool isStatic, int accessMod ){
	int pos=toker->pos();
	std::string methodName=parseIdent();
	std::string tag=parseTypeTag();

	// constructor e destructor ricevono sempre l'oggetto come primo argomento
	// (NewNode, DeleteNode): non possono essere Static
	if( isStatic && (methodName=="constructor" || methodName=="destructor") ){
		ex( methodName+" cannot be a Static Method" );
	}

	// Method becomes a global function named ClassName_MethodName
	std::string funcName=className+"_"+methodName;

	if( toker->curr()!='(' ) exp( "'('" );

	// Create params
	a_ptr<DeclSeqNode> params( d_new DeclSeqNode() );

	// Add implicit Self parameter only for non-static methods
	if( !isStatic ){
		DeclNode *selfParam=d_new VarDeclNode( "self",className,DECL_PARAM,false,0 );
		selfParam->pos=pos;selfParam->file=incfile;
		params->push_back( selfParam );
	}

	// Parse remaining parameters
	if( toker->next()!=')' ){
		for(;;){
			params->push_back( parseVarDecl( DECL_PARAM,false ) );
			if( toker->curr()!=',' ) break;
			toker->next();
		}
		if( toker->curr()!=')' ) exp( "')'" );
	}
	toker->next();

	// Parse method body - set current class for Super keyword
	std::string prevClass = currentClassName;
	currentClassName = className;
	a_ptr<StmtSeqNode> stmts( parseStmtSeq( STMTS_BLOCK ) );
	currentClassName = prevClass;  // Restore previous (empty if not nested)
	if( toker->curr()!=ENDMETHOD ) exp( "'End Method'" );
	StmtNode *ret=d_new ReturnNode(0);ret->pos=toker->pos();
	stmts->push_back( ret );toker->next();

	// Create as a regular function declaration with access modifier and owner class
	DeclNode *d=d_new FuncDeclNode( funcName,tag,params.release(),stmts.release(),accessMod,className );
	d->pos=pos;d->file=incfile;
	return d;
}
