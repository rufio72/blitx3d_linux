#ifndef CLASSDECLNODE_H
#define CLASSDECLNODE_H

#include "node.h"
#include "decl_seq.h"
#include "func_decl.h"

struct ClassDeclNode : public DeclNode{
	std::string ident;
	std::string superName;  // Parent class name (empty if no inheritance)
	DeclSeqNode *fields;
	DeclSeqNode *methods;  // Method declarations
	StructType *sem_type;
	ClassDeclNode( const std::string &i,const std::string &s,DeclSeqNode *f,DeclSeqNode *m ):ident(i),superName(s),fields(f),methods(m){}
	~ClassDeclNode(){ delete fields; delete methods; }
	void proto( DeclSeq *d,Environ *e );
	void semant( Environ *e );
	void translate( Codegen *g );
#ifdef USE_LLVM
	void translate2( Codegen_LLVM *g );
#endif

	json toJSON( Environ *e );
};

#endif
