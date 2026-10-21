
#include "../std.h"
#include <cstdlib>
#include "parser.h"

#include "decl/data_decl.h"

#include "expr/int_const.h"
#include "expr/method_call.h"
#include "expr/new.h"
#include "expr/var_expr.h"

#include "stmt/ass.h"
#include "stmt/decl_stmt.h"
#include "stmt/delete.h"
#include "stmt/delete_each.h"
#include "stmt/expr_stmt.h"
#include "stmt/exit.h"
#include "stmt/for.h"
#include "stmt/for_each.h"
#include "stmt/goto.h"
#include "stmt/gosub.h"
#include "stmt/insert.h"
#include "stmt/include.h"
#include "stmt/label.h"
#include "stmt/read.h"
#include "stmt/repeat.h"
#include "stmt/restore.h"
#include "stmt/return.h"
#include "stmt/select.h"
#include "stmt/while.h"

#include "var/ident_var.h"


#ifdef DEMO
static const int TEXTLIMIT=16384;
#else
static const int TEXTLIMIT=1024*1024-1;
#endif

static bool isTerm( int c ){ return c==':' || c=='\n'; }

Parser::Parser( Toker &t ):toker(&t),main_toker(&t){
}

ProgNode *Parser::parse( const std::string &main ){

	incfile=main;

	consts=d_new DeclSeqNode();
	structs=d_new DeclSeqNode();
	funcs=d_new DeclSeqNode();
	datas=d_new DeclSeqNode();
	StmtSeqNode *stmts=0;

	try{
		stmts=parseStmtSeq( STMTS_PROG );
		if( toker->curr()!=EOF ) exp( "end-of-file" );
	}catch( Ex ){
		delete stmts;delete datas;delete funcs;delete structs;delete consts;
		throw;
	}

	return d_new ProgNode( consts,structs,funcs,datas,stmts );
}

void Parser::ex( const std::string &s ){
	throw Ex( s,toker->pos(),incfile );
}

void Parser::exp( const std::string &s ){
	switch( toker->curr() ){
	case NEXT:ex( "'Next' without 'For'" );
	case WEND:ex( "'Wend' without 'While'" );
	case ELSE:case ELSEIF:ex( "'Else' without 'If'" );
	case ENDIF:ex( "'Endif' without 'If'" );
	case ENDFUNCTION:ex( "'End Function' without 'Function'" );
	case ENDTYPE:ex( "'End Type' without 'Type'" );
	case ENDCLASS:ex( "'End Class' without 'Class'" );
	case ENDMETHOD:ex( "'End Method' without 'Method'" );
	case UNTIL:ex( "'Until' without 'Repeat'" );
	case FOREVER:ex( "'Forever' without 'Repeat'" );
	case CASE:ex( "'Case' without 'Select'" );
	case ENDSELECT:ex( "'End Select' without 'Select'" );
	}
	ex( "Expecting "+s );
}

#ifndef WIN32
#include <filesystem>
// Resolve a path case-insensitively against the filesystem. Windows projects
// often mismatch include-file casing, which only breaks on Linux/macOS.
static std::string resolvePathCI( const std::string &path ){
	namespace fs=std::filesystem;
	std::error_code ec;
	if( fs::exists( path,ec ) ) return path;
	fs::path p( path );
	fs::path dir=p.parent_path();
	if( dir.empty() ) dir=".";
	if( !fs::is_directory( dir,ec ) ) return path;
	std::string want=tolower( p.filename().string() );
	for( const auto &entry:fs::directory_iterator( dir,ec ) ){
		if( tolower( entry.path().filename().string() )==want ) return entry.path().string();
	}
	return path;
}
#endif

// The OOP keywords are contextual: Method/Static/Private/Public/Protected/
// Extends are keywords only inside a Class body, Self/This/Super only inside
// method bodies, Class only as a main-program declaration 'Class Name'.
// Everywhere else they are valid identifiers (classic Blitz3D code uses
// them as variable, function, field and label names).
bool Parser::identToke( int toke ){
	switch( toke ){
	case IDENT:case THIS:case SELF:case SUPER:case CLASS:case METHOD:
	case EXTENDS:case STATIC:case PRIVATE:case PUBLIC:case PROTECTED:
		return true;
	}
	return false;
}

std::string Parser::parseIdent(){
	if( !identToke( toker->curr() ) ) exp( "identifier" );
	std::string t=toker->text();
	toker->next();
	return t;
}

void Parser::parseChar( int c ){
	if( toker->curr()!=c ) exp( std::string( "'" )+char(c)+std::string( "'" ) );
	toker->next();
}

StmtSeqNode *Parser::parseStmtSeq( int scope ){
	a_ptr<StmtSeqNode> stmts( d_new StmtSeqNode( incfile ) );
	parseStmtSeq( stmts,scope );
	return stmts.release();
}

void Parser::parseStmtSeq( StmtSeqNode *stmts,int scope ){

	for(;;){
		while( toker->curr()==':' || (scope!=STMTS_LINE && toker->curr()=='\n') ) toker->next();
		StmtNode *result=0;

		int pos=toker->pos();

#ifdef DEMO
		if( Toker::chars_toked>TEXTLIMIT ){
			ex( "Demo version source limit exceeded" );
		}
#endif
		switch( toker->curr() ){
		case INCLUDE:
			{
				if( toker->next()!=STRINGCONST ) exp( "include filename" );
				std::string inc=toker->text();toker->next();
				inc=inc.substr( 1,inc.size()-2 );
#ifndef WIN32
				// Windows projects use '\\' in include paths
				for( size_t ci=0;ci<inc.size();++ci ) if( inc[ci]=='\\' ) inc[ci]='/';
#endif

				inc=fullfilename( inc );
#ifdef WIN32
				// TODO: find out why this was originally done. not good outside of
				// windows
				inc=tolower( inc );
#else
				// match the on-disk casing if the literal one doesn't exist
				inc=resolvePathCI( inc );
#endif

				if( included.find( inc )!=included.end() ) break;

				std::ifstream i_stream( inc.c_str() );
				if( !i_stream.good() ) ex( "Unable to open include file" );

				Toker i_toker( i_stream );

				std::string t_inc=incfile;incfile=inc;
				Toker *t_toker=toker;toker=&i_toker;

				included.insert( incfile );

				a_ptr<StmtSeqNode> ss( parseStmtSeq( scope ) );
				if( toker->curr()!=EOF ) exp( "end-of-file" );

				result=d_new IncludeNode( incfile,ss.release() );

				toker=t_toker;
				incfile=t_inc;
			}
			break;
		case BUNDLEIDENTIFIER:
			{
				if( toker->next()!=STRINGCONST ) exp( "App ID must be a string" );
				std::string name=toker->text();toker->next();
				name=name.substr( 1,name.size()-2 );

				bundle.enabled=true;
				bundle.identifier=name;
			}
			break;
		case BUNDLEAPPNAME:
			{
				if( toker->next()!=STRINGCONST ) exp( "App name must be a string" );
				std::string name=toker->text();toker->next();
				name=name.substr( 1,name.size()-2 );

				bundle.enabled=true;
				bundle.appName=name;
			}
			break;
		case BUNDLEFILE:
			{
				if( toker->next()!=STRINGCONST ) exp( "File must be a string" );
				std::string path=toker->text();toker->next();
				path=canonicalpath( path.substr( 1,path.size()-2 )) ;

				std::string abspath=fullfilename( path );
				std::ifstream i_stream( abspath.c_str() );
				if( !i_stream.good() ) ex( "Unable to open "+abspath );
				i_stream.close();

				bundle.enabled=true;
				bundle.files.push_back( BundleFile( path,abspath ) );
			}
			break;
		case THIS:
		case SELF:
			// aliases of the method receiver only inside methods; outside a
			// class they are plain identifiers (SCP-CB uses 'this', classic
			// Blitz code uses 'self' as variable/field names)
			if( currentClassName.empty() ) goto this_as_ident_stmt;
			// Self is treated as a variable named "self"; both Self\x and Self.x work
			{
				toker->next();
				a_ptr<ExprNode> expr_or_var( parseVarOrMethodCall( "self","",true ) );
				if( dynamic_cast<MethodCallNode*>( (ExprNode*)expr_or_var ) ){
					// Method call as statement: Self\Method() / Self.Method()
					result=d_new ExprStmtNode( expr_or_var.release() );
				}else{
					VarExprNode *var_expr=dynamic_cast<VarExprNode*>( (ExprNode*)expr_or_var );
					if( !var_expr ) ex( "Unexpected expression" );
					if( toker->curr()!='=' ) exp( "variable assignment" );
					toker->next();ExprNode *expr=parseExpr( false );
					VarNode *var=var_expr->var;
					var_expr->var=0; // prevent double delete
					delete expr_or_var.release();
					result=d_new AssNode( var,expr );
				}
			}
			break;
		case SUPER:
			// Super\Method() as a statement
			// (outside methods 'super' is a plain identifier)
			if( currentClassName.empty() ) goto this_as_ident_stmt;
			{
				toker->next();
				if( toker->curr()!='\\' && toker->curr()!='.' ){
					exp( "'\\'" );
				}
				toker->next();
				std::string method_ident=parseIdent();
				std::string method_tag=parseTypeTag();
				if( toker->curr()!='(' ) exp( "'('" );
				toker->next();
				a_ptr<ExprSeqNode> exprs( parseExprSeq() );
				if( toker->curr()!=')' ) exp( "')'" );
				toker->next();
				ExprNode *method_call=d_new SuperMethodCallNode( currentClassName, method_ident, method_tag, exprs.release() );
				result=d_new ExprStmtNode( method_call );
			}
			break;
		case METHOD:case EXTENDS:case STATIC:
		case PRIVATE:case PUBLIC:case PROTECTED:
			// class-member keywords are plain identifiers in statements
		case IDENT:
			this_as_ident_stmt:
			{
				std::string ident=toker->text();
				toker->next();std::string tag=parseTypeTag();
				if( arrayDecls.find(ident)==arrayDecls.end()
					&& toker->curr()!='=' && toker->curr()!='\\' && toker->curr()!='[' ){
					//must be a function
					a_ptr<ExprSeqNode> exprs;
					if( toker->curr()=='(' ){
						//ugly lookahead for optional '()' around statement params
						int nest=1,k;
						for( k=1;;++k ){
							int c=toker->lookAhead( k );
							if( isTerm( c ) ) ex( "Mismatched brackets" );
							else if( c=='(' ) ++nest;
							else if( c==')' && !--nest ) break;
						}
						if( isTerm( toker->lookAhead( ++k ) ) ){
							toker->next();
							exprs=parseExprSeq();
							if( toker->curr()!=')' ) exp( "')'" );
							toker->next();
						}else exprs=parseExprSeq();
					}else exprs=parseExprSeq();
					CallNode *call=d_new CallNode( ident,tag,exprs.release() );
					result=d_new ExprStmtNode( call );
				}else{
					//must be a var or method call
					a_ptr<ExprNode> expr_or_var( parseVarOrMethodCall( ident,tag ) );
					// Check if it's a method call (statement) or var (needs assignment)
					if( dynamic_cast<MethodCallNode*>( (ExprNode*)expr_or_var ) ){
						// Method call as statement
						result=d_new ExprStmtNode( expr_or_var.release() );
					}else{
						// Must be a var assignment
						VarExprNode *var_expr = dynamic_cast<VarExprNode*>( (ExprNode*)expr_or_var );
						if( !var_expr ) ex( "Unexpected expression" );
						if( toker->curr()!='=' ) exp( "variable assignment" );
						toker->next();ExprNode *rhs=parseExpr( false );

						// OOP Type Inference: if tag is empty and rhs is New ClassName(...),
						// infer the type from the class name
						std::string inferred_tag = tag;
						if( tag.empty() ){
							NewNode *newNode = dynamic_cast<NewNode*>(rhs);
							if( newNode && newNode->ctor_args ){
								// Infer type from New expression
								inferred_tag = newNode->ident;
							}
						}

						// Extract VarNode from VarExprNode for AssNode
						VarNode *var = var_expr->var;
						// Update the tag if we inferred it
						if( !inferred_tag.empty() && tag.empty() ){
							IdentVarNode *identVar = dynamic_cast<IdentVarNode*>(var);
							if( identVar ){
								identVar->tag = inferred_tag;
							}
						}
						var_expr->var = 0; // prevent double delete
						delete expr_or_var.release();
						result=d_new AssNode( var,rhs );
					}
				}
			}
			break;
		case IF:
			{
				toker->next();result=parseIf();
				if( toker->curr()==ENDIF ) toker->next();
			}
			break;
		case WHILE:
			{
				toker->next();
				a_ptr<ExprNode> expr( parseExpr( false ) );
				a_ptr<StmtSeqNode> stmts( parseStmtSeq( STMTS_BLOCK ) );
				int pos=toker->pos();
				if( toker->curr()!=WEND ) exp( "'Wend'" );
				toker->next();
				result=d_new WhileNode( expr.release(),stmts.release(),pos );
			}
			break;
		case REPEAT:
			{
				toker->next();ExprNode *expr=0;
				a_ptr<StmtSeqNode> stmts( parseStmtSeq( STMTS_BLOCK ) );
				int curr=toker->curr();
				int pos=toker->pos();
				if( curr!=UNTIL && curr!=FOREVER ) exp( "'Until' or 'Forever'" );
				toker->next();if( curr==UNTIL ) expr=parseExpr( false );
				result=d_new RepeatNode( stmts.release(),expr,pos );
			}
			break;
		case SELECT:
			{
				toker->next();ExprNode *expr=parseExpr( false );
				a_ptr<SelectNode> selNode( d_new SelectNode( expr ) );
				for(;;){
					while( isTerm( toker->curr() ) ) toker->next();
					if( toker->curr()==CASE ){
						toker->next();
						a_ptr<ExprSeqNode> exprs( parseExprSeq() );
						if( !exprs->size() ) exp( "expression sequence" );
						a_ptr<StmtSeqNode> stmts( parseStmtSeq( STMTS_BLOCK ) );
						selNode->push_back( d_new CaseNode( exprs.release(),stmts.release() ) );
						continue;
					}else if( toker->curr()==DEFAULT ){
						toker->next();
						a_ptr<StmtSeqNode> stmts( parseStmtSeq( STMTS_BLOCK ) );
						if( toker->curr()!=ENDSELECT ) exp( "'End Select'" );
						selNode->defStmts=stmts.release();
						break;
					}else if( toker->curr()==ENDSELECT ){
						break;
					}
					exp( "'Case', 'Default' or 'End Select'" );
				}
				toker->next();
				result=selNode.release();
			}
			break;
		case FOR:
			{
				a_ptr<VarNode> var;
				a_ptr<StmtSeqNode> stmts;
				toker->next();var=parseVar();
				if( toker->curr()!='=' ) exp( "variable assignment" );
				if( toker->next()==EACH ){
					toker->next();
					std::string ident=parseIdent();
					stmts=parseStmtSeq( STMTS_BLOCK );
					int pos=toker->pos();
					if( toker->curr()!=NEXT ) exp( "'Next'" );
					toker->next();
					result=d_new ForEachNode( var.release(),ident,stmts.release(),pos );
				}else{
					a_ptr<ExprNode> from,to,step;
					from=parseExpr( false );
					if( toker->curr()!=TO ) exp( "'TO'" );
					toker->next();to=parseExpr( false );
					//step...
					if( toker->curr()==STEP ){
						toker->next();step=parseExpr( false );
					}else step=d_new IntConstNode( 1 );
					stmts=parseStmtSeq( STMTS_BLOCK );
					int pos=toker->pos();
					if( toker->curr()!=NEXT ) exp( "'Next'" );
					toker->next();
					result=d_new ForNode( var.release(),from.release(),to.release(),step.release(),stmts.release(),pos );
				}
			}
			break;
		case EXIT:
			{
				toker->next();result=d_new ExitNode();
			}
			break;
		case GOTO:
			{
				toker->next();std::string t=parseIdent();result=d_new GotoNode( t );
			}
			break;
		case GOSUB:
			{
				toker->next();std::string t=parseIdent();result=d_new GosubNode( t );
			}
			break;
		case RETURN:
			{
				toker->next();result=d_new ReturnNode( parseExpr( true ) );
			}
			break;
		case BBDELETE:
			{
				if( toker->next()==EACH ){
					toker->next();std::string t=parseIdent();
					result=d_new DeleteEachNode( t );
				}else{
					ExprNode *expr=parseExpr( false );
					result=d_new DeleteNode( expr );
				}
			}
			break;
		case INSERT:
			{
				toker->next();
				a_ptr<ExprNode> expr1( parseExpr( false ) );
				if( toker->curr()!=BEFORE && toker->curr()!=AFTER ) exp( "'Before' or 'After'" );
				bool before=toker->curr()==BEFORE;toker->next();
				a_ptr<ExprNode> expr2( parseExpr( false ) );
				result=d_new InsertNode( expr1.release(),expr2.release(),before );
			}
			break;
		case READ:
			do{
				toker->next();VarNode *var=parseVar();
				StmtNode *stmt=d_new ReadNode( var );
				stmt->pos=pos;pos=toker->pos();
				stmts->push_back( stmt );
			}while( toker->curr()==',' );
			break;
		case RESTORE:
			if( identToke( toker->next() ) ){
				result=d_new RestoreNode( toker->text() );toker->next();
			}else result=d_new RestoreNode( "" );
			break;
		case DATA:
			if( scope!=STMTS_PROG ) ex( "'Data' can only appear in main program" );
			do{
				toker->next();
				ExprNode *expr=parseExpr( false );
				datas->push_back( d_new DataDeclNode( expr ) );
			}while( toker->curr()==',' );
			break;
		case TYPE:
			if( scope!=STMTS_PROG ) ex( "'Type' can only appear in main program" );
			toker->next();structs->push_back( parseStructDecl() );
			break;
		case CLASS:
			// 'Class Name' at main-program level is a class declaration;
			// any other use (class = 5, class(x), class 7) is an identifier
			if( scope!=STMTS_PROG || !identToke( toker->lookAhead( 1 ) ) ) goto this_as_ident_stmt;
			toker->next();structs->push_back( parseClassDecl() );
			break;
		case BBCONST:
			if( scope!=STMTS_PROG ) ex( "'Const' can only appear in main program" );
			do{
				toker->next();consts->push_back( parseVarDecl( DECL_GLOBAL,true ) );
			}while( toker->curr()==',' );
			break;
		case FUNCTION:
			if( scope!=STMTS_PROG ) ex( "'Function' can only appear in main program" );
			toker->next();funcs->push_back( parseFuncDecl() );
			break;
		case DIM:
			do{
				toker->next();
				StmtNode *stmt=parseArrayDecl();
				stmt->pos=pos;pos=toker->pos();
				stmts->push_back( stmt );
			}while( toker->curr()==',' );
			break;
		case LOCAL:
			do{
				toker->next();
				DeclNode *d=parseVarDecl( DECL_LOCAL,false );
				StmtNode *stmt=d_new DeclStmtNode( d );
				stmt->pos=pos;pos=toker->pos();
				stmts->push_back( stmt );
			}while( toker->curr()==',' );
			break;
		case GLOBAL:
			if( scope!=STMTS_PROG ) ex( "'Global' can only appear in main program" );
			do{
				toker->next();
				DeclNode *d=parseVarDecl( DECL_GLOBAL,false );
				StmtNode *stmt=d_new DeclStmtNode( d );
				stmt->pos=pos;pos=toker->pos();
				stmts->push_back( stmt );
			}while( toker->curr()==',' );
			break;
		case '.':
			{
				toker->next();std::string t=parseIdent();
				result=d_new LabelNode( t,datas->size() );
			}
			break;
		default:
			return;
		}

		if( result ){
			result->pos=pos;
			stmts->push_back( result );
		}
	}
}

std::string Parser::parseTypeTag(){
	switch( toker->curr() ){
	case '%':toker->next();return "%";
	case '#':toker->next();return "#";
	case '$':toker->next();return "$";
	case '.':toker->next();return parseIdent();
	}
	return "";
}

// Like parseTypeTag but without the '.ident' form: used in dot-style member
// access (Self.field), where a '.' is an access separator, not a type tag.
std::string Parser::parseSigilTag(){
	switch( toker->curr() ){
	case '%':toker->next();return "%";
	case '#':toker->next();return "#";
	case '$':toker->next();return "$";
	}
	return "";
}

IfNode *Parser::parseIf(){
	a_ptr<ExprNode> expr;
	a_ptr<StmtSeqNode> stmts,elseOpt;

	expr=parseExpr( false );
	if( toker->curr()==THEN ) toker->next();

	bool blkif=isTerm( toker->curr() );
	stmts=parseStmtSeq( blkif ? STMTS_BLOCK : STMTS_LINE );

	if( toker->curr()==ELSEIF ){
		int pos=toker->pos();
		toker->next();
		IfNode *ifnode=parseIf();
		ifnode->pos=pos;
		elseOpt=d_new StmtSeqNode( incfile );
		elseOpt->push_back( ifnode );
	}else if( toker->curr()==ELSE ){
		toker->next();
		elseOpt=parseStmtSeq( blkif ? STMTS_BLOCK : STMTS_LINE );
	}
	if( blkif ){
		if( toker->curr()!=ENDIF ) exp( "'EndIf'" );
	}else if( toker->curr()!='\n' ) exp( "end-of-line" );

	return d_new IfNode( expr.release(),stmts.release(),elseOpt.release() );
}
