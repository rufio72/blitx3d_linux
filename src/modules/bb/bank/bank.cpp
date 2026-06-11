

#include <bb/blitz/blitz.h>
#include "../stdutil/stdutil.h"
#include "bank.h"

#include <cstring>
#include <set>

bbBank::bbBank( int sz ):size(sz){
	if( size<0 ) size=0;
	capacity=(size+15)&~15;
	data=d_new char[capacity];
	memset( data,0,size );
}

bbBank::~bbBank(){
	delete[] data;
}

void bbBank::resize( int n ){
	if( n<0 ) n=0;
	if( n>size ){
		if( n>capacity ){
			long long c=(long long)capacity*3/2;
			if( n>c ) c=n;
			c=(c+15)&~15;
			capacity=(int)c;
			char *p=d_new char[capacity];
			memcpy( p,data,size );
			memset( p+size,0,n-size );
			delete[] data;
			data=p;
		}else memset( data+size,0,n-size );
	}
	size=n;
}

static std::set<bbBank*> bank_set;

void debugBank( bbBank *b ){
	if( bb_env.debug ){
		if( !bank_set.count( b ) ) RTEX( "bbBank does not exist" );
	}
}

void debugBank( bbBank *b,int offset ){
	if( bb_env.debug ){
		debugBank( b );
		if( offset<0 || offset>=b->size ) RTEX( "Offset out of range" );
	}
}

static void debugBankRange( bbBank *b,bb_int_t offset,bb_int_t count ){
	if( bb_env.debug ){
		debugBank( b );
		if( offset<0 || count<0 || offset+count>b->size ) RTEX( "Offset out of range" );
	}
}

bbBank * BBCALL bbCreateBank( bb_int_t size ){
	if( size<0 ) RTEX( "Illegal bank size" );
	bbBank *b=d_new bbBank( size );
	bank_set.insert( b );
	return b;
}

void BBCALL bbFreeBank( bbBank *b ){
	if( bank_set.erase( b ) ) delete b;
}

bb_int_t BBCALL bbBankSize( bbBank *b ){
	debugBank( b );
	return b->size;
}

void BBCALL bbResizeBank( bbBank *b,bb_int_t size ){
	debugBank( b );
	if( size<0 ) RTEX( "Illegal bank size" );
	b->resize( size );
}

void BBCALL bbCopyBank( bbBank *src,bb_int_t src_p,bbBank *dest,bb_int_t dest_p,bb_int_t count ){
	if( count<=0 ) return;
	debugBankRange( src,src_p,count );debugBankRange( dest,dest_p,count );
	memmove( dest->data+dest_p,src->data+src_p,count );
}

bb_int_t BBCALL bbPeekByte( bbBank *b,bb_int_t offset ){
	debugBank( b,offset );
	return *(unsigned char*)(b->data+offset);
}

bb_int_t BBCALL bbPeekShort( bbBank *b,bb_int_t offset ){
	debugBankRange( b,offset,2 );
	return *(unsigned short*)(b->data+offset);
}

bb_int_t BBCALL bbPeekInt( bbBank *b,bb_int_t offset ){
	debugBankRange( b,offset,4 );
	return *(int*)(b->data+offset);
}

bb_float_t BBCALL bbPeekFloat( bbBank *b,bb_int_t offset ){
	debugBankRange( b,offset,4 );
	return *(float*)(b->data+offset);
}

bb_int_t BBCALL bbPeekHandle( bbBank *b,bb_int_t offset ){
	debugBankRange( b,offset,sizeof(bb_ptr_t) );
	return *(bb_ptr_t*)(b->data+offset);
}

void BBCALL bbPokeByte( bbBank *b,bb_int_t offset,bb_int_t value ){
	debugBank( b,offset );
	*(char*)(b->data+offset)=value;
}

void BBCALL bbPokeShort( bbBank *b,bb_int_t offset,bb_int_t value ){
	debugBankRange( b,offset,2 );
	*(unsigned short*)(b->data+offset)=value;
}

void BBCALL bbPokeInt( bbBank *b,bb_int_t offset,bb_int_t value ){
	debugBankRange( b,offset,4 );
	*(int*)(b->data+offset)=value;
}

void BBCALL bbPokeFloat( bbBank *b,bb_int_t offset,bb_float_t value ){
	debugBankRange( b,offset,4 );
	*(float*)(b->data+offset)=value;
}

void BBCALL bbPokeHandle( bbBank *b,bb_int_t offset,bb_int_t value ){
	debugBankRange( b,offset,sizeof(bb_ptr_t) );
	*(bb_ptr_t*)(b->data+offset)=value;
}

bb_int_t BBCALL  bbReadBytes( bbBank *b,BBStream *s,bb_int_t offset,bb_int_t count ){
	if( count<=0 ) return 0;
	debugBankRange( b,offset,count );
	if( bb_env.debug ) debugStream( s );
	return s->read( b->data+offset,count );
}

bb_int_t BBCALL  bbWriteBytes( bbBank *b,BBStream *s,bb_int_t offset,bb_int_t count ){
	if( count<=0 ) return 0;
	debugBankRange( b,offset,count );
	if( bb_env.debug ) debugStream( s );
	return s->write( b->data+offset,count );
}

BBMODULE_CREATE( bank ){
	return true;
}

BBMODULE_DESTROY( bank ){
	while( bank_set.size() ) bbFreeBank( *bank_set.begin() );
	return true;
}
