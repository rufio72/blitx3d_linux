
// Expression and variable parsing: split out of parser.cpp to keep files small.

#include "../std.h"
#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include "parser.h"

#include "expr/after.h"
#include "expr/arith_expr.h"
#include "expr/before.h"
#include "expr/bin_expr.h"
#include "expr/call.h"
#include "expr/method_call.h"
#include "expr/cast.h"
#include "expr/first.h"
#include "expr/float_const.h"
#include "expr/int_const.h"
#include "expr/last.h"
#include "expr/new.h"
#include "expr/null.h"
#include "expr/object_cast.h"
#include "expr/object_handle.h"
#include "expr/rel_expr.h"
#include "expr/string_const.h"
#include "expr/uni_expr.h"
#include "expr/var_expr.h"

#include "var/array_var.h"
#include "var/ident_var.h"
#include "var/field_var.h"
#include "var/vector_var.h"

VarNode *Parser::parseVar(){
	std::string ident=parseIdent();
	std::string tag=parseTypeTag();
	return parseVar( ident,tag );
}

VarNode *Parser::parseVar( const std::string &ident,const std::string &tag,bool allowDot ){
	a_ptr<VarNode> var;
	if( toker->curr()=='(' ){
		toker->next();
		a_ptr<ExprSeqNode> exprs( parseExprSeq() );
		if( toker->curr()!=')' ) exp( "')'" );
		toker->next();
		var=d_new ArrayVarNode( ident,tag,exprs.release() );
	}else var=d_new IdentVarNode( ident,tag );

	for(;;){
		if( toker->curr()=='\\' || (allowDot && toker->curr()=='.') ){
			bool dot=( toker->curr()=='.' );
			toker->next();
			std::string ident=parseIdent();
			// in dot-mode a '.' is a member access, never a type tag
			std::string tag=dot ? parseSigilTag() : parseTypeTag();
			ExprNode *expr=d_new VarExprNode( var.release() );
			var=d_new FieldVarNode( expr,ident,tag );
		}else if( toker->curr()=='[' ){
			toker->next();
			a_ptr<ExprSeqNode> exprs( parseExprSeq() );
			if( exprs->exprs.size()!=1 || toker->curr()!=']' ) exp( "']'" );
			toker->next();
			ExprNode *expr=d_new VarExprNode( var.release() );
			var=d_new VectorVarNode( expr,exprs.release() );
		}else{
			break;
		}
	}
	return var.release();
}

// Parse a variable access or method call (obj\Method())
// Returns either VarExprNode or MethodCallNode
ExprNode *Parser::parseVarOrMethodCall( const std::string &ident,const std::string &tag,bool allowDot ){
	a_ptr<VarNode> var;
	if( toker->curr()=='(' ){
		toker->next();
		a_ptr<ExprSeqNode> exprs( parseExprSeq() );
		if( toker->curr()!=')' ) exp( "')'" );
		toker->next();
		var=d_new ArrayVarNode( ident,tag,exprs.release() );
	}else var=d_new IdentVarNode( ident,tag );

	for(;;){
		if( toker->curr()=='\\' || (allowDot && toker->curr()=='.') ){
			bool dot=( toker->curr()=='.' );
			toker->next();
			std::string field_ident=parseIdent();
			// in dot-mode a '.' is a member access, never a type tag
			std::string field_tag=dot ? parseSigilTag() : parseTypeTag();

			// Check if this is a method call: obj\Method(...)
			if( toker->curr()=='(' ){
				// This is a method call!
				toker->next();
				a_ptr<ExprSeqNode> exprs( parseExprSeq() );
				if( toker->curr()!=')' ) exp( "')'" );
				toker->next();

				// Create VarExprNode from current var
				ExprNode *obj_expr=d_new VarExprNode( var.release() );

				// Create method call node
				a_ptr<ExprNode> method_call( d_new MethodCallNode( obj_expr,field_ident,field_tag,exprs.release() ) );

				// Check for chained method calls or field access
				while( toker->curr()=='\\' || (allowDot && toker->curr()=='.') ){
					bool next_dot=( toker->curr()=='.' );
					toker->next();
					std::string next_ident=parseIdent();
					std::string next_tag=next_dot ? parseSigilTag() : parseTypeTag();
					if( toker->curr()=='(' ){
						// Chained method call
						toker->next();
						a_ptr<ExprSeqNode> next_exprs( parseExprSeq() );
						if( toker->curr()!=')' ) exp( "')'" );
						toker->next();
						ExprNode *prev=method_call.release();
						method_call=d_new MethodCallNode( prev,next_ident,next_tag,next_exprs.release() );
					}else{
						// Field access after method call - not supported yet
						ex( "Field access after method call not yet supported" );
					}
				}
				return method_call.release();
			}

			// Regular field access
			ExprNode *expr=d_new VarExprNode( var.release() );
			var=d_new FieldVarNode( expr,field_ident,field_tag );
		}else if( toker->curr()=='[' ){
			toker->next();
			a_ptr<ExprSeqNode> exprs( parseExprSeq() );
			if( exprs->exprs.size()!=1 || toker->curr()!=']' ) exp( "']'" );
			toker->next();
			ExprNode *expr=d_new VarExprNode( var.release() );
			var=d_new VectorVarNode( expr,exprs.release() );
		}else{
			break;
		}
	}
	return d_new VarExprNode( var.release() );
}

ExprSeqNode *Parser::parseExprSeq(){
	a_ptr<ExprSeqNode> exprs( d_new ExprSeqNode() );
	bool opt=true;
	while( ExprNode *e=parseExpr( opt ) ){
		exprs->push_back( e );
		if( toker->curr()!=',' ) break;
		toker->next();opt=false;
	}
	return exprs.release();
}

ExprNode *Parser::parseExpr( bool opt ){
	if( toker->curr()==NOT ){
		toker->next();
		ExprNode *expr=parseExpr1( false );
		return d_new RelExprNode( '=',expr,d_new IntConstNode( 0 ) );
	}
	return parseExpr1( opt );
}

ExprNode *Parser::parseExpr1( bool opt ){

	a_ptr<ExprNode> lhs( parseExpr2( opt ) );
	if( !lhs ) return 0;
	for(;;){
		int c=toker->curr();
		if( c!=AND && c!=OR && c!=XOR ) return lhs.release();
		toker->next();
		// After AND/OR/XOR, allow Not operator by calling parseUnaryNot
		ExprNode *rhs;
		if( toker->curr()==NOT ){
			toker->next();
			rhs=parseExpr2( false );
			rhs=d_new RelExprNode( '=',rhs,d_new IntConstNode( 0 ) );
		}else{
			rhs=parseExpr2( false );
		}
		lhs=d_new BinExprNode( c,lhs.release(),rhs );
	}
}

ExprNode *Parser::parseExpr2( bool opt ){

	a_ptr<ExprNode> lhs( parseExpr3( opt ) );
	if( !lhs ) return 0;
	for(;;){
		int c=toker->curr();
		if( c!='<' && c!='>' && c!='=' && c!=LE && c!=GE && c!=NE ) return lhs.release();
		toker->next();ExprNode *rhs=parseExpr3( false );
		lhs=d_new RelExprNode( c,lhs.release(),rhs );
	}
}

ExprNode *Parser::parseExpr3( bool opt ){

	a_ptr<ExprNode> lhs( parseExpr4( opt ) );
	if( !lhs ) return 0;
	for(;;){
		int c=toker->curr();
		if( c!='+' && c!='-' ) return lhs.release();
		toker->next();ExprNode *rhs=parseExpr4( false );
		lhs=d_new ArithExprNode( c,lhs.release(),rhs );
	}
}

ExprNode *Parser::parseExpr4( bool opt ){
	a_ptr<ExprNode> lhs( parseExpr5( opt ) );
	if( !lhs ) return 0;
	for(;;){
		int c=toker->curr();
		if( c!=SHL && c!=SHR && c!=SAR ) return lhs.release();
		toker->next();ExprNode *rhs=parseExpr5( false );
		lhs=d_new BinExprNode( c,lhs.release(),rhs );
	}
}

ExprNode *Parser::parseExpr5( bool opt ){

	a_ptr<ExprNode> lhs( parseExpr6( opt ) );
	if( !lhs ) return 0;
	for(;;){
		int c=toker->curr();
		if( c!='*' && c!='/' && c!=MOD ) return lhs.release();
		toker->next();ExprNode *rhs=parseExpr6( false );
		lhs=d_new ArithExprNode( c,lhs.release(),rhs );
	}
}

ExprNode *Parser::parseExpr6( bool opt ){

	a_ptr<ExprNode> lhs( parseUniExpr( opt ) );
	if( !lhs ) return 0;
	for(;;){
		int c=toker->curr();
		if( c!='^' ) return lhs.release();
		toker->next();ExprNode *rhs=parseUniExpr( false );
		lhs=d_new ArithExprNode( c,lhs.release(),rhs );
	}
}

ExprNode *Parser::parseUniExpr( bool opt ){

	ExprNode *result=0;
	std::string t;

	int c=toker->curr();
	switch( c ){
	case BBINT:
		if( toker->next()=='%' ) toker->next();
		result=parseUniExpr( false );
		result=d_new CastNode( result,Type::int_type );
		break;
	case BBFLOAT:
		if( toker->next()=='#' ) toker->next();
		result=parseUniExpr( false );
		result=d_new CastNode( result,Type::float_type );
		break;
	case BBSTR:
		if( toker->next()=='$' ) toker->next();
		result=parseUniExpr( false );
		result=d_new CastNode( result,Type::string_type );
		break;
	case OBJECT:
		if( toker->next()=='.' ) toker->next();
		t=parseIdent();
		result=parseUniExpr( false );
		result=d_new ObjectCastNode( result,t );
		break;
	case BBHANDLE:
		toker->next();
		result=parseUniExpr( false );
		result=d_new ObjectHandleNode( result );
		break;
	case BEFORE:
		toker->next();
		result=parseUniExpr( false );
		result=d_new BeforeNode( result );
		break;
	case AFTER:
		toker->next();
		result=parseUniExpr( false );
		result=d_new AfterNode( result );
		break;
	case '+':case '-':case '~':case ABS:case SGN:
		toker->next();
		result=parseUniExpr( false );
		if( c=='~' ){
			result=d_new BinExprNode( XOR,result,d_new IntConstNode( -1 ) );
		}else{
			result=d_new UniExprNode( c,result );
		}
		break;
	default:
		result=parsePrimary( opt );
	}
	return result;
}

#ifdef BB64
// Parses an integer literal token (decimal, %binary or $hex) into a 64-bit value.
// Binary/hex literals are the plain 64-bit bit pattern (up to 64/16 digits);
// decimal literals must not exceed INT64_MAX. Returns false on overflow.
static bool parseIntLiteral( const std::string &t,bb_int_t &out ){
	uint64_t n=0;
	if( t[0]=='%' || t[0]=='$' ){
		int bits=t[0]=='%' ? 1 : 4;
		for( size_t k=1;k<t.size();++k ){
			if( n>>(64-bits) ) return false;
			int d=bits==1 ? (t[k]=='1') : ( isdigit(t[k]) ? t[k]&0xf : (t[k]&7)+9 );
			n=(n<<bits)|(uint64_t)d;
		}
	}else{
		errno=0;
		unsigned long long v=strtoull( t.c_str(),0,10 );
		if( errno==ERANGE || v>(unsigned long long)INT64_MAX ) return false;
		n=v;
	}
	out=(bb_int_t)n;
	return true;
}
#endif

ExprNode *Parser::parsePrimary( bool opt ){

	a_ptr<ExprNode> expr;
	std::string t,ident,tag;
	ExprNode *result=0;
	size_t n,k;

	switch( toker->curr() ){
	case '(':
		toker->next();
		expr=parseExpr( false );
		if( toker->curr()!=')' ) exp( "')'" );
		toker->next();
		result=expr.release();
		break;
	case BBNEW:
		toker->next();t=parseIdent();
		// Check for constructor arguments: New ClassName(args)
		if( toker->curr()=='(' ){
			toker->next();
			a_ptr<ExprSeqNode> args( parseExprSeq() );
			if( toker->curr()!=')' ) exp( "')'" );
			toker->next();
			result=d_new NewNode( t, args.release() );
		}else{
			result=d_new NewNode( t );
		}
		break;
	case FIRST:
		toker->next();t=parseIdent();
		result=d_new FirstNode( t );
		break;
	case LAST:
		toker->next();t=parseIdent();
		result=d_new LastNode( t );
		break;
	case BBNULL:
		result=d_new NullNode();
		toker->next();
		break;
#ifdef BB64
	case INTCONST:case BINCONST:case HEXCONST:{
		bb_int_t v;
		if( !parseIntLiteral( toker->text(),v ) ) ex( "Integer constant too large" );
		result=d_new IntConstNode( v );
		toker->next();
		break;
	}
#else
	case INTCONST:
		result=d_new IntConstNode( atoi( toker->text() ) );
		toker->next();
		break;
	case BINCONST:
		n=0;t=toker->text();
		for( k=1;k<t.size();++k ) n=(n<<1)|(t[k]=='1');
		result=d_new IntConstNode( n );
		toker->next();
		break;
	case HEXCONST:
		n=0;t=toker->text();
		for( k=1;k<t.size();++k ) n=(n<<4)|( isdigit(t[k]) ? t[k]&0xf : (t[k]&7)+9 );
		result=d_new IntConstNode( n );
		toker->next();
		break;
#endif
	case FLOATCONST:
		result=d_new FloatConstNode( atof( toker->text() ) );
		toker->next();
		break;
	case STRINGCONST:
		t=toker->text();
		result=d_new StringConstNode( t.substr( 1,t.size()-2 ) );
		toker->next();
		break;
	case PI:
		result=d_new FloatConstNode( 3.1415926535897932384626433832795f );
		toker->next();break;
	case BBTRUE:
		result=d_new IntConstNode( 1 );
		toker->next();break;
	case BBFALSE:
		result=d_new IntConstNode( 0 );
		toker->next();break;
	case THIS:
	case SELF:
		// aliases of the method receiver only inside methods; plain
		// identifiers elsewhere (classic code uses 'self'/'this' as names)
		if( currentClassName.empty() ) goto this_as_ident_expr;
		// Self is treated as a variable named "self"
		toker->next();
		// Use parseVarOrMethodCall to support self\Method() / self.Method() syntax
		result=parseVarOrMethodCall( "self","",true );
		break;
	case SUPER:
		// Super\Method() calls the parent class's method
		// (outside methods 'super' is a plain identifier)
		if( currentClassName.empty() ) goto this_as_ident_expr;
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
			result=d_new SuperMethodCallNode( currentClassName, method_ident, method_tag, exprs.release() );
		}
		break;
	case CLASS:case METHOD:case EXTENDS:case STATIC:
	case PRIVATE:case PUBLIC:case PROTECTED:
		// declarations are statements only: in expressions these are names
	case IDENT:
		this_as_ident_expr:
		ident=toker->text();
		toker->next();tag=parseTypeTag();
		if( toker->curr()=='(' && arrayDecls.find(ident)==arrayDecls.end() ){
			//must be a func
			toker->next();
			a_ptr<ExprSeqNode> exprs( parseExprSeq() );
			if( toker->curr()!=')' ) exp( "')'" );
			toker->next();
			result=d_new CallNode( ident,tag,exprs.release() );
		}else{
			//must be a var - or method call if obj\Method()
			result=parseVarOrMethodCall( ident,tag );
		}
		break;
	default:
		if( !opt ) exp( "expression" );
	}
	return result;
}
