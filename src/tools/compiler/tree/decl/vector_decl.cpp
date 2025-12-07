#include "vector_decl.h"
#include "../expr/const.h"

////////////////////////
// Vector declaration //
////////////////////////
void VectorDeclNode::proto( DeclSeq *d,Environ *env ){

	Type *ty=tagType( tag,env );if( !ty ) ty=Type::int_type;

	std::vector<int> sizes;
	for( int k=0;k<exprs->size();++k ){
		ExprNode *e=exprs->exprs[k]=exprs->exprs[k]->semant( env );
		ConstNode *c=e->constNode();
		if( !c ) ex( "Blitz array sizes must be constant" );
		int n=c->intValue();
		if( n<0 ) ex( "Blitz array sizes must not be negative" );
		sizes.push_back( n+1 );
	}
	std::string label=genLabel();
	sem_type=d_new VectorType( label,ty,sizes );
	if( !d->insertDecl( ident,sem_type,kind ) ){
		delete sem_type;ex( "Duplicate identifier" );
	}
	env->types.push_back( sem_type );
}

void VectorDeclNode::translate( Codegen *g ){
	//type tag!
	g->align_data( 4 );
	VectorType *v=sem_type->vectorType();
	g->i_data( 6,v->label );
	int sz=1;
	for( int k=0;k<v->sizes.size();++k ) sz*=v->sizes[k];
	g->i_data( sz );
	std::string t;
	Type *type=v->elementType;
	if( type==Type::int_type ) t="__bbIntType";
	else if( type==Type::float_type ) t="__bbFltType";
	else if( type==Type::string_type ) t="__bbStrType";
	else if( StructType *s=type->structType() ) t="_t"+s->ident;
	else if( VectorType *v=type->vectorType() ) t=v->label;
	g->p_data( t );

	if( kind==DECL_GLOBAL ) g->i_data( 0,"_v"+ident );
}

#ifdef USE_LLVM
void VectorDeclNode::translate2( Codegen_LLVM *g ){
	// VectorType *v=sem_type->vectorType();

	// if( kind==DECL_GLOBAL ) g->i_data( 0,"_v"+ident );
}
#endif

json VectorDeclNode::toJSON( Environ *e ){
	json tree;tree["@class"]="VectorDeclNode";
	tree["pos"]=pos;
	tree["file"]=file;
	tree["ident"]=ident;
	tree["tag"]=tag;
	tree["exprs"]=exprs->toJSON( e );
	tree["kind"]=kind;
	tree["sem_type"]=sem_type->toJSON();
	return tree;
}

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

void VectorDeclNode::translate3( Codegen_C *g ){
	// Generate BBVecType structure for this vector type
	VectorType *v = sem_type->vectorType();
	std::string label = g->toCSafeName( v->label );

	// Calculate total size
	int sz = 1;
	for( int k = 0; k < (int)v->sizes.size(); ++k ) sz *= v->sizes[k];

	// Get element type reference
	std::string elementTypeRef;
	Type *type = v->elementType;
	if( type == Type::int_type ) elementTypeRef = "&_bbIntType";
	else if( type == Type::float_type ) elementTypeRef = "&_bbFltType";
	else if( type == Type::string_type ) elementTypeRef = "&_bbStrType";
	else if( StructType *s = type->structType() ) elementTypeRef = "(BBType*)&_t" + g->toCSafeName( s->ident );
	else if( VectorType *vt = type->vectorType() ) elementTypeRef = "(BBType*)&" + g->toCSafeName( vt->label );
	else elementTypeRef = "&_bbIntType";

	// Emit the BBVecType structure
	// BBVecType has: base.type, size, elementType
	g->emitGlobal( "static BBVecType " + label + " = { .base = { .type = 6 }, .size = " +
		std::to_string( sz ) + ", .elementType = " + elementTypeRef + " };" );

	// If this is a global vector, also declare the global variable to hold the pointer
	if( kind == DECL_GLOBAL ){
		g->emitGlobal( "static void *" + g->toCSafeName( "_v" + ident ) + " = 0;" );
	}
}
#endif
