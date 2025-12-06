#include "class_decl.h"

/////////////////////
// Class Declaration //
/////////////////////
void ClassDeclNode::proto( DeclSeq *d,Environ *e ){
	sem_type=d_new StructType( ident,d_new DeclSeq() );
	if( !d->insertDecl( ident,sem_type,DECL_STRUCT ) ){
		delete sem_type;ex( "Duplicate identifier" );
	}
	e->types.push_back( sem_type );

	// Handle inheritance - store parent type reference
	if( !superName.empty() ){
		Type *t=e->findType( superName );
		if( !t ) ex( "Type '"+superName+"' not found" );
		StructType *st=t->structType();
		if( !st ) ex( "'"+superName+"' is not a class type" );
		sem_type->superType=st;
	}

	// Register methods as global functions
	methods->proto( e->funcDecls,e );
}

void ClassDeclNode::semant( Environ *e ){
	// First, copy inherited fields from parent class
	int inherited_offset=0;
	if( sem_type->superType ){
		DeclSeq *parentFields=sem_type->superType->fields;
		for( int k=0;k<parentFields->size();++k ){
			Decl *parentDecl=parentFields->decls[k];
			// Insert inherited field (with same type and kind)
			Decl *d=sem_type->fields->insertDecl( parentDecl->name,parentDecl->type,parentDecl->kind,parentDecl->defType );
			if( d ) d->offset=k*4;
			inherited_offset=(k+1)*4;
		}
	}

	// Then add our own fields
	fields->proto( sem_type->fields,e );
	// Set offsets for own fields (after inherited fields)
	int own_start=inherited_offset/4;
	for( int k=own_start;k<sem_type->fields->size();++k ){
		sem_type->fields->decls[k]->offset=k*4;
	}

	// Semantic analysis for methods
	methods->semant( e );
}

void ClassDeclNode::translate( Codegen *g ){

	//translate fields
	fields->translate( g );

	//translate methods
	methods->translate( g );

	//type ID
	g->align_data( 4 );
	g->i_data( 5,"_t"+ident );

	//used and free lists for type
	int k;
	for( k=0;k<2;++k ){
		std::string lab=genLabel();
		g->i_data( 0,lab );	//fields
		g->p_data( lab );	//next
		g->p_data( lab );	//prev
		g->i_data( 0 );		//type
		g->i_data( -1 );	//ref_cnt
	}

	//number of fields
	g->i_data( sem_type->fields->size() );

	//type of each field
	for( k=0;k<sem_type->fields->size();++k ){
		Decl *field=sem_type->fields->decls[k];
		Type *type=field->type;
		std::string t;
		if( type==Type::int_type ) t="__bbIntType";
		else if( type==Type::float_type ) t="__bbFltType";
		else if( type==Type::string_type ) t="__bbStrType";
		else if( StructType *s=type->structType() ) t="_t"+s->ident;
		else if( VectorType *v=type->vectorType() ) t=v->label;
		g->p_data( t );
	}

}

#ifdef USE_LLVM
void ClassDeclNode::translate2( Codegen_LLVM *g ){
	auto template_typesary=llvm::ArrayType::get( llvm::PointerType::get( g->bbType,0 ),sem_type->fields->size() );
	std::vector<llvm::Constant*> template_types;
	std::vector<llvm::Constant*> template_defs;
	std::vector<llvm::Type*> fields_vec;
	fields_vec.push_back( g->bbObj ); // base
	for( int k=0;k<sem_type->fields->size();++k ){
		Decl *field=sem_type->fields->decls[k];
		Type *type=field->type;
		fields_vec.push_back( type->llvmType( g->context.get() ) );

		llvm::GlobalVariable* gt=0;
		std::string t;
		if( type==Type::int_type ) t="_bbIntType";
		else if( type==Type::float_type ) t="_bbFltType";
		else if( type==Type::string_type ) t="_bbStrType";
		else if( StructType *s=type->structType() ) gt=(llvm::GlobalVariable*)s->llvmTypeDef( g );
		else if( VectorType *v=type->vectorType() ) gt=v->llvmDef( g );

		if( !gt ) {
			gt=(llvm::GlobalVariable*)g->module->getOrInsertGlobal( t,g->bbType );
		}

		template_types.push_back( gt );
	}

	sem_type->llvmTypeDef( g );
	sem_type->llvmType( g->context.get() );

	auto ty2=sem_type->deftype;

	llvm::StructType* ty=(llvm::StructType*)sem_type->structtype;
	ty->setBody( fields_vec );

	std::vector<llvm::Type*> templatefields;
	templatefields.push_back( g->bbObjType ); // base
	templatefields.push_back( template_typesary );
	ty2->setBody( templatefields );

	auto temp=(llvm::GlobalVariable*)sem_type->objty;

	std::vector<llvm::Constant*> objdata;
	objdata.push_back( llvm::ConstantPointerNull::get( llvm::PointerType::get( g->bbField,0 ) ) );   // fields
	objdata.push_back( llvm::ConstantPointerNull::get( llvm::PointerType::get( g->bbObj,0 ) ) );     // next
	objdata.push_back( llvm::ConstantPointerNull::get( llvm::PointerType::get( g->bbObj,0 ) ) );     // prev
	objdata.push_back( llvm::ConstantPointerNull::get( llvm::PointerType::get( g->bbObjType,0 ) ) ); // type
	objdata.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt(64, -1) ) );                  // ref_cnt
	auto emptyobj=llvm::ConstantStruct::get( g->bbObj,objdata );

	std::vector<llvm::Constant*> objtypedata;
	objtypedata.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt( 64,5 ) ) ); // type

	for( int k=0;k<2;++k ){ // used/free
		std::vector<llvm::Value*> indices;
		indices.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt(32, 0) ) );
		indices.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt(32, 0) ) );
		indices.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt(32, k+1) ) );
		auto objptr=llvm::ConstantExpr::getGetElementPtr( ty2,temp,indices );

		std::vector<llvm::Constant*> objdata;
		objdata.push_back( llvm::ConstantPointerNull::get( llvm::PointerType::get( g->bbField,0 ) ) );   // fields
		objdata.push_back( objptr );     // next
		objdata.push_back( objptr );     // prev
		objdata.push_back( llvm::ConstantPointerNull::get( llvm::PointerType::get( g->bbObjType,0 ) ) ); // type
		objdata.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt(64, -1) ) );                  // ref_cnt
		auto defaultobj=llvm::ConstantStruct::get( g->bbObj,objdata );

		objtypedata.push_back( defaultobj );
	}

	objtypedata.push_back( llvm::ConstantInt::get( *g->context,llvm::APInt( 64,sem_type->fields->size() ) ) ); // fieldCnt
	// fieldTypes does not need to be accounted for.

	std::vector<llvm::Constant*> structfields;
	structfields.push_back( llvm::ConstantStruct::get( g->bbObjType,objtypedata ) );
	structfields.push_back( llvm::ConstantArray::get( template_typesary,template_types ) );
	auto init=llvm::ConstantStruct::get( ty2,structfields );
	temp->setInitializer( init );

	// Translate methods
	methods->translate2( g );
}
#endif

json ClassDeclNode::toJSON( Environ *e ){
	json tree;tree["@class"]="ClassDeclNode";
	tree["pos"]=pos;
	tree["file"]=file;
	tree["ident"]=ident;
	tree["fields"]=fields->toJSON( e );
	tree["sem_type"]=sem_type->toJSON();
	return tree;
}
