#include "call.h"
#include "const.h"
#include <fstream>
#include <iostream>
#include <cctype>

// Helper to convert string to lowercase for case-insensitive comparison
static std::string toLowerStr( const std::string &s ){
	std::string r = s;
	for( size_t i = 0; i < r.size(); i++ ){
		r[i] = tolower( (unsigned char)r[i] );
	}
	return r;
}

// Returns the argument position (0-based) that contains the filename for file-loading functions
// Returns -1 if not a file-loading function
static int getFileArgPosition( const std::string &funcName ){
	std::string name = toLowerStr( funcName );

	// Functions with filename as first argument (position 0)
	if( name == "loadtexture" || name == "loadanimtexture" ||
	    name == "loadbrush" || name == "loadmesh" || name == "loadanimmesh" ||
	    name == "loadsprite" || name == "loadmd2" || name == "loadbsp" ||
	    name == "loadterrain" || name == "loadimage" || name == "loadanimimage" ||
	    name == "loadsound" || name == "load3dsound" ||
	    name == "readfile" || name == "openfile" ){
		return 0;
	}

	// Functions with filename as second argument (position 1)
	if( name == "loadanimseq" || name == "loadbuffer" ){
		return 1;
	}

	return -1;
}

// Helper to convert Windows paths to Unix paths
static std::string toUnixPath( const std::string &path ){
	std::string result = path;
	for( size_t i = 0; i < result.size(); i++ ){
		if( result[i] == '\\' ) result[i] = '/';
	}
	return result;
}

// Check if file exists (tries case variations on Linux and handles Windows paths)
static bool fileExistsCI( const std::string &path ){
	// Convert Windows backslashes to Unix forward slashes
	std::string unixPath = toUnixPath( path );

	// Try the path (possibly converted)
	std::ifstream f( unixPath );
	if( f.good() ) return true;

	// Try different case for extension
	size_t dot = unixPath.rfind( '.' );
	if( dot != std::string::npos ){
		std::string base = unixPath.substr( 0, dot + 1 );
		std::string ext = unixPath.substr( dot + 1 );

		// Try uppercase extension
		std::string upper_ext;
		for( char c : ext ) upper_ext += toupper( c );
		std::ifstream f2( base + upper_ext );
		if( f2.good() ) return true;

		// Try lowercase extension
		std::string lower_ext;
		for( char c : ext ) lower_ext += tolower( c );
		std::ifstream f3( base + lower_ext );
		if( f3.good() ) return true;
	}

	return false;
}

///////////////////
// Function call //
///////////////////
ExprNode *CallNode::semant( Environ *e ){
	Type *t=e->findType( tag );
	sem_decl=e->findFunc( ident );
	if( !sem_decl || !(sem_decl->kind & DECL_FUNC) ) ex( "Function '"+ident+"' not found" );
	FuncType *f=sem_decl->type->funcType();
	if( t && f->returnType!=t ) ex( "incorrect function return type" );
	exprs->semant( e );
	exprs->castTo( f->params,e,f->cfunc );
	sem_type=f->returnType;

	// Check for file-loading functions with string constant arguments
	int fileArgPos = getFileArgPosition( ident );
	if( fileArgPos >= 0 && fileArgPos < exprs->size() ){
		ExprNode *arg = exprs->exprs[fileArgPos];
		ConstNode *constArg = arg->constNode();
		if( constArg && arg->sem_type == Type::string_type ){
			std::string filename = constArg->stringValue();
			if( !filename.empty() && !fileExistsCI( filename ) ){
				std::cerr << "Warning: File not found: \"" << filename << "\" in call to " << ident << "()" << std::endl;
			}
		}
	}

	return this;
}

TNode *CallNode::translate( Codegen *g ){

	FuncType *f=sem_decl->type->funcType();

	TNode *t;
	TNode *l=global( "_f"+ident );
	TNode *r=exprs->translate( g,f->cfunc );

	if( f->userlib ){
		l=d_new TNode( IR_MEM,l );
		usedfuncs.insert( ident );
	}

	if( sem_type==Type::float_type ){
		t=d_new TNode( IR_FCALL,l,r,exprs->size()*4 );
	}else{
		t=d_new TNode( IR_CALL,l,r,exprs->size()*4 );
	}

	if( f->returnType->stringType() ){
		if( f->cfunc ){
			t=call( "__bbCStrToStr",t );
		}
	}
	return t;
}

#ifdef USE_LLVM
#include <llvm/IR/Attributes.h>

llvm::Value *CallNode::translate2( Codegen_LLVM *g ){
	FuncType *f=sem_decl->type->funcType();
	auto func = f->llvmFunction( sem_decl->name,g );

	std::vector<llvm::Value*> args;
	for( int i=0;i<exprs->size();i++ ){
		args.push_back( exprs->exprs[i]->translate2( g ) );
	}

	return g->builder->CreateCall( func,args );
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

std::string CallNode::translate3( Codegen_C *g ){
	FuncType *f = sem_decl->type->funcType();

	// Get the C symbol name
	// Runtime functions have symbol set (e.g., "bbPrint")
	// User-defined functions use the safe name pattern
	std::string funcName = f->symbol;
	if( funcName.empty() ){
		// User-defined function - use same naming as FuncDeclNode
		funcName = g->toCSafeName( "f" + ident );
	}

	std::string result = funcName + "(";

	for( int i=0; i<exprs->size(); i++ ){
		if( i > 0 ) result += ", ";
		result += exprs->exprs[i]->translate3( g );
	}
	result += ")";

	return result;
}
#endif

json CallNode::toJSON( Environ *e ){
	json tree;tree["@class"]="CallNode";
	tree["sem_type"]=sem_type->toJSON();
	tree["ident"]=ident;
	tree["tag"]=tag;
	tree["sem_decl"]=sem_decl->toJSON();
	tree["exprs"]=exprs->toJSON( e );
	return tree;
}
