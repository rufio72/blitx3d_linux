#ifndef METHOD_CALL_NODE_H
#define METHOD_CALL_NODE_H

#include "node.h"
#include "expr_seq.h"

// Method call node: obj\Method(args)
// During semantic analysis, this is resolved to ClassName_Method(obj, args)
struct MethodCallNode : public ExprNode{
	ExprNode *obj;        // The object (e.g., "p" in p\Init())
	std::string method;   // Method name (e.g., "Init")
	std::string tag;      // Return type tag
	ExprSeqNode *exprs;   // Arguments
	Decl *sem_decl;       // Resolved function declaration
	std::string resolved_ident; // Resolved function name (e.g., "Player_Init")

	MethodCallNode( ExprNode *o,const std::string &m,const std::string &t,ExprSeqNode *e )
		:obj(o),method(m),tag(t),exprs(e),sem_decl(0){}
	~MethodCallNode(){ delete obj; delete exprs; }
	ExprNode *semant( Environ *e );
	TNode *translate( Codegen *g );
	json toJSON( Environ *e );
#ifdef USE_LLVM
	llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
#endif
};

// Super method call node: Super\Method(args)
// Calls the parent class's method with self as the object
struct SuperMethodCallNode : public ExprNode{
	std::string currentClass; // Current class name (set by parser)
	std::string method;       // Method name
	std::string tag;          // Return type tag
	ExprSeqNode *exprs;       // Arguments
	Decl *sem_decl;           // Resolved function declaration
	Decl *self_decl;          // The 'self' parameter from current method
	std::string resolved_ident; // Resolved function name (ParentClass_Method)

	SuperMethodCallNode( const std::string &cls,const std::string &m,const std::string &t,ExprSeqNode *e )
		:currentClass(cls),method(m),tag(t),exprs(e),sem_decl(0),self_decl(0){}
	~SuperMethodCallNode(){ delete exprs; }
	ExprNode *semant( Environ *e );
	TNode *translate( Codegen *g );
	json toJSON( Environ *e );
#ifdef USE_LLVM
	llvm::Value *translate2( Codegen_LLVM *g );
#endif
#ifdef USE_GCC_BACKEND
	std::string translate3( Codegen_C *g );
#endif
};

#endif
