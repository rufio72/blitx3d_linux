#include "string_const.h"

/////////////////////
// String constant //
/////////////////////
StringConstNode::StringConstNode( const std::string &s ):value(s){
	sem_type=Type::string_type;
}

TNode *StringConstNode::translate( Codegen *g ){
	std::string lab=genLabel();
	g->s_data( value,lab );
	return call( "__bbStrConst",global( lab ) );
}

#ifdef USE_LLVM
std::map<std::string,llvm::Value*> ptrs;

llvm::Value *StringConstNode::translate2( Codegen_LLVM *g ){
	auto t=g->constantString( value );
	return g->CallIntrinsic( "_bbStrConst",sem_type->llvmType( g->context.get() ),1,t );
}
#endif

#ifdef USE_GCC_BACKEND
#include "../../codegen_c/codegen_c.h"

std::string StringConstNode::translate3( Codegen_C *g ){
	std::string strName = g->constantString( value );
	return "_bbStrConst(" + strName + ")";
}
#endif

int StringConstNode::intValue(){
	return atoi( value );
}

float StringConstNode::floatValue(){
	return (float)atof( value );
}

std::string StringConstNode::stringValue(){
	return value;
}

json StringConstNode::toJSON( Environ *e ){
	json tree;tree["@class"]="StringConstNode";
	tree["sem_type"]=sem_type->toJSON();
	tree["value"]=value;
	return tree;
}
