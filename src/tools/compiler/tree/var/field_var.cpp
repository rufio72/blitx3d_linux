#include "field_var.h"

///////////////
// Field var //
///////////////
void FieldVarNode::semant( Environ *e ){
	expr=expr->semant( e );
	StructType *s=expr->sem_type->structType();
	if( !s ) ex( "Variable must be a Type" );
	sem_field=s->fields->findDecl( ident );
	if( !sem_field ) ex( "Type field not found" );

	// Check access control for private fields
	if( sem_field->kind & DECL_PRIVATE ){
		StructType *currentClass = e->getCurrentClass();
		if( !currentClass || currentClass != s ){
			ex( "Cannot access private field '" + ident + "'" );
		}
	}

	// Check access control for protected fields
	// Protected fields can be accessed from the same class or any subclass
	if( sem_field->kind & DECL_PROTECTED ){
		StructType *currentClass = e->getCurrentClass();
		if( !currentClass || !currentClass->isSubclassOf( s ) ){
			ex( "Cannot access protected field '" + ident + "'" );
		}
	}

	sem_type=sem_field->type;
}

TNode *FieldVarNode::translate( Codegen *g ){
	TNode *t=expr->translate( g );
	if( g->debug ) t=jumpf( t,"__bbNullObjEx" );
	t=mem( t );if( g->debug ) t=jumpf( t,"__bbNullObjEx" );
	return add( t,iconst( sem_field->offset ) );
}

#ifdef USE_LLVM
llvm::Value *FieldVarNode::translate2( Codegen_LLVM *g ){
	std::vector<llvm::Value*> indices;
	indices.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt( 32,0 ) ) );
	indices.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt( 32,sem_field->offset/4+1 ) ) );

	return g->builder->CreateGEP( expr->sem_type->structType()->structtype,expr->translate2( g ),indices );
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

std::string FieldVarNode::translate3( Codegen_C *g ){
	// Get the object pointer
	std::string objPtr = expr->translate3( g );
	// Access field through BBObj->fields pointer
	// sem_field->offset is field_index * 4 (from old 32-bit code)
	// Fields are stored in obj->fields which points to memory right after BBObj header
	int fieldIndex = sem_field->offset / 4; // Convert byte offset to field index
	std::string baseAccess = "((BBObj*)" + objPtr + ")->fields[" + std::to_string( fieldIndex ) + "]";

	// Return the appropriate union member based on type
	if( sem_type == Type::int_type ){
		return baseAccess + ".INT";
	}else if( sem_type == Type::float_type ){
		return baseAccess + ".FLT";
	}else if( sem_type == Type::string_type ){
		return baseAccess + ".STR";
	}else if( sem_type->structType() ){
		return baseAccess + ".OBJ";
	}else if( sem_type->vectorType() ){
		return baseAccess + ".VEC";
	}
	return baseAccess + ".INT";
}

std::string FieldVarNode::load3( Codegen_C *g ){
	std::string objPtr = expr->translate3( g );
	int fieldIndex = sem_field->offset / 4;
	std::string baseAccess = "((BBObj*)" + objPtr + ")->fields[" + std::to_string( fieldIndex ) + "]";

	if( sem_type == Type::int_type ){
		return baseAccess + ".INT";
	}else if( sem_type == Type::float_type ){
		return baseAccess + ".FLT";
	}else if( sem_type == Type::string_type ){
		return "_bbStrLoad((bb_string_t*)&" + baseAccess + ".STR)";
	}else if( sem_type->structType() ){
		return "(bb_obj_t)" + baseAccess + ".OBJ";
	}else if( sem_type->vectorType() ){
		return baseAccess + ".VEC";
	}
	return baseAccess + ".INT";
}

void FieldVarNode::store3( Codegen_C *g, const std::string &value ){
	std::string objPtr = expr->translate3( g );
	int fieldIndex = sem_field->offset / 4;
	std::string baseAccess = "((BBObj*)" + objPtr + ")->fields[" + std::to_string( fieldIndex ) + "]";

	if( sem_type == Type::int_type ){
		g->emitLine( baseAccess + ".INT = " + value + ";" );
	}else if( sem_type == Type::float_type ){
		g->emitLine( baseAccess + ".FLT = " + value + ";" );
	}else if( sem_type == Type::string_type ){
		g->emitLine( "_bbStrStore((bb_string_t*)&" + baseAccess + ".STR, " + value + ");" );
	}else if( sem_type->structType() ){
		g->emitLine( "_bbObjStore((void**)&" + baseAccess + ".OBJ, " + value + ");" );
	}else if( sem_type->vectorType() ){
		g->emitLine( baseAccess + ".VEC = " + value + ";" );
	}else{
		g->emitLine( baseAccess + ".INT = " + value + ";" );
	}
}
#endif

json FieldVarNode::toJSON( Environ *e ){
	json tree;tree["@class"]="FieldVarNode";
	tree["sem_type"]=sem_type->toJSON();
	tree["expr"]=expr->toJSON( e );
	tree["ident"]=ident;
	tree["tag"]=tag;
	tree["sem_field"]=sem_field->toJSON();
	return tree;
}
