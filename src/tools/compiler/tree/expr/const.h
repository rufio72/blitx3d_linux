#ifndef CONST_NODE_H
#define CONST_NODE_H

#include "node.h"
#include <type_traits>

// Unsigned counterpart of bb_int_t, used for wrap-around constant folding.
typedef std::make_unsigned<bb_int_t>::type bb_uint_t;

struct ConstNode : public ExprNode{
	ExprNode *semant( Environ *e ){ return this; }
	ConstNode *constNode(){ return this; }
	virtual bb_int_t intValue()=0;
	virtual float floatValue()=0;
	virtual std::string stringValue()=0;

	DEFAULT_NODE_JSON( ConstNode );
};

#endif
