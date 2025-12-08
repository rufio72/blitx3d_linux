#include "dim.h"

//////////////////////////////
// Dim AND declare an Array //
//////////////////////////////
void DimNode::semant( Environ *e ){
	Type *t=tagType( tag,e );
	if( Decl *d=e->findDecl( ident ) ){
		ArrayType *a=d->type->arrayType();
		if( !a || a->dims!=exprs->size() || (t && a->elementType!=t) ){
			ex( "Duplicate identifier" );
		}
		sem_type=a;sem_decl=0;
	}else{
		if( e->level>0 ) ex( "Array not found in main program" );
		if( !t ) t=Type::int_type;
		sem_type=d_new ArrayType( t,exprs->size() );
		sem_decl=e->decls->insertDecl( ident,sem_type,DECL_ARRAY );
		e->types.push_back( sem_type );
	}
	exprs->semant( e );
	exprs->castTo( Type::int_type,e );
}

void DimNode::translate( Codegen *g ){
	TNode *t;
	g->code( call( "__bbUndimArray",global( "_a"+ident ) ) );
	for( int k=0;k<exprs->size();++k ){
		t=add( global( "_a"+ident ),iconst( k*4+12 ) );
		t=move( exprs->exprs[k]->translate(g),mem( t ) );
		g->code( t );
	}
	g->code( call( "__bbDimArray",global( "_a"+ident ) ) );

	if( !sem_decl ) return;

	int et;
	Type *ty=sem_type->arrayType()->elementType;
	if( ty==Type::int_type ) et=1;
	else if( ty==Type::float_type ) et=2;
	else if( ty==Type::string_type ) et=3;
	else et=5;

	g->align_data( 4 );
	g->i_data( 0,"_a"+ident );
	g->i_data( et );
	g->i_data( exprs->size() );
	for( int k=0;k<exprs->size();++k ) g->i_data( 0 );
}

#ifdef USE_LLVM
void DimNode::translate2( Codegen_LLVM *g ){
	int et;
	Type *ty=sem_type->arrayType()->elementType;
	if( ty==Type::int_type ) et=1;
	else if( ty==Type::float_type ) et=2;
	else if( ty==Type::string_type ) et=3;
	else et=5;

	auto glob=g->getArray( ident,sem_type->arrayType()->dims );
	auto data_ty=g->arrayTypes[ident];

	std::vector<llvm::Constant*> fields;
	fields.push_back( llvm::ConstantPointerNull::get( g->voidPtr ) );
	fields.push_back( g->constantInt( et ) );
	fields.push_back( g->constantInt( exprs->size() ) );
	auto ary=llvm::ConstantStruct::get( g->bbArray,fields );

	std::vector<llvm::Constant*> afields;
	afields.push_back( ary );
	for( int k=0;k<exprs->size();++k ) {
		afields.push_back( g->constantInt( 0 ) );
	}
	auto defdata=llvm::ConstantStruct::get( data_ty,afields );

	glob->setInitializer( defdata );

	auto int_ty=Type::int_type->llvmType( g->context.get() );

	g->CallIntrinsic( "_bbUndimArray",g->voidTy,1,g->CastToArrayPtr( glob ) );
	for( int k=0;k<exprs->size();++k ){
		std::vector<llvm::Value*> idx;
		idx.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt(32, 0) ) );
		idx.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt(32, 1+k) ) );
		auto t=g->builder->CreateGEP( data_ty,glob,idx );
		g->builder->CreateStore( exprs->exprs[k]->translate2(g),t );
	}
	g->CallIntrinsic( "_bbDimArray",g->voidTy,1,g->CastToArrayPtr( glob ) );
}
#endif

json DimNode::toJSON( Environ *e ){
	json tree;tree["@class"]="DimNode";
	tree["pos"]=pos;
	tree["ident"]=ident;
	tree["tag"]=tag;
	tree["exprs"]=exprs->toJSON( e );
	tree["sem_type"]=sem_type->toJSON();
	if( sem_decl ) tree["sem_decl"]=sem_decl->toJSON();
	return tree;
}

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

void DimNode::translate3( Codegen_C *g ){
	std::string arrayName = g->toCSafeName( "_a" + ident );

	// Get element type
	int et;
	Type *ty = sem_type->arrayType()->elementType;
	if( ty == Type::int_type ) et = 1;
	else if( ty == Type::float_type ) et = 2;
	else if( ty == Type::string_type ) et = 3;
	else et = 5;

	// Declare array if this is the first Dim (sem_decl is set)
	if( sem_decl ){
		// Declare the array structure - must match runtime's BBArray: {data, elementType, dims, scales[]}
		g->emitGlobal( "static struct { void *data; bb_int_t elementType; bb_int_t dims; bb_int_t scales[" +
			std::to_string(exprs->size()) + "]; } " + arrayName + " = {0, " +
			std::to_string(et) + ", " + std::to_string(exprs->size()) + ", {0}};" );
	}

	// Undim the array
	g->emitLine( "_bbUndimArray((BBArray*)&" + arrayName + ");" );

	// Set dimension scales
	for( int k = 0; k < (int)exprs->size(); ++k ){
		std::string dimVal = exprs->exprs[k]->translate3( g );
		g->emitLine( arrayName + ".scales[" + std::to_string(k) + "] = " + dimVal + ";" );
	}

	// Dim the array
	g->emitLine( "_bbDimArray((BBArray*)&" + arrayName + ");" );
}
#endif
