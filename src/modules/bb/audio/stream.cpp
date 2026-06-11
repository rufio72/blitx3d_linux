
#include "../stdutil/stdutil.h"
#include "stream.h"

#include <cstdlib>
#include <cstring>


AudioStream::Ref::Ref( AudioStream *s ):stream(s){
	pos=stream->start;
	refbuf=new unsigned char[stream->buf_size];
#ifndef BB_MINGW
	std::lock_guard<std::mutex> guard( stream->lock );
	++stream->refs;
#else
	WaitForSingleObject( stream->lock,INFINITE );
	++stream->refs;
	ReleaseMutex( stream->lock );
#endif
}

AudioStream::Ref::~Ref(){
	delete[] refbuf;
	bool del;
#ifndef BB_MINGW
	{
		std::lock_guard<std::mutex> guard( stream->lock );
		del=( --stream->refs==0 && stream->orphaned );
	}
#else
	WaitForSingleObject( stream->lock,INFINITE );
	del=( --stream->refs==0 && stream->orphaned );
	ReleaseMutex( stream->lock );
#endif
	if( del ) delete stream;
}

size_t AudioStream::Ref::decode( unsigned char **buf ){
#ifndef BB_MINGW
	std::lock_guard<std::mutex> guard( stream->lock );
#else
	WaitForSingleObject( stream->lock,INFINITE );
#endif
	stream->seek( pos );
	size_t n=stream->decode();
	pos=stream->pos();
	// copy out under the lock: another Ref may clobber stream->buf as
	// soon as we let go
	if( n>(size_t)stream->buf_size ) n=stream->buf_size;
	memcpy( refbuf,stream->buf,n );
	*buf=refbuf;

#ifdef BB_MINGW
	ReleaseMutex( stream->lock );
#endif
	return n;
}

void AudioStream::Ref::reset(){
	pos=stream->start;
}

unsigned int AudioStream::Ref::getChannels(){
	return stream->channels;
}

unsigned int AudioStream::Ref::getBits(){
	return stream->bits;
}

unsigned int AudioStream::Ref::getFrequency(){
	return stream->frequency;
}

unsigned int AudioStream::Ref::getSamples(){
	return stream->samples;
}

bool AudioStream::Ref::eof(){
	return pos==-1;
}

AudioStream::Ref *AudioStream::getRef(){
	return d_new Ref( this );
}

void AudioStream::release(){
	bool del;
#ifndef BB_MINGW
	{
		std::lock_guard<std::mutex> guard( lock );
		orphaned=true;
		del=( refs==0 );
	}
#else
	WaitForSingleObject( lock,INFINITE );
	orphaned=true;
	del=( refs==0 );
	ReleaseMutex( lock );
#endif
	if( del ) delete this;
}


AudioStream::AudioStream( int size ):buf_size(size),channels(0),bits(0),samples(0),frequency(0),refs(0),orphaned(false){
	buf=new unsigned char[buf_size];
#ifdef BB_MINGW
	lock=CreateMutex( NULL,FALSE,NULL );
#endif
}

AudioStream::~AudioStream(){
	in.close();
	delete[] buf;
#ifdef BB_MINGW
	CloseHandle( lock );
#endif
}

bool AudioStream::init( const char *url ){
	path=url;
	in.open( url,std::ios_base::in|std::ios::binary );
	if( !readHeader() ) return false;
	start=pos();
	return true;
}

size_t AudioStream::read( void *ptr, size_t size ){
	return in.read( (char*)ptr,size ).gcount();
}

bool AudioStream::eof(){
	return in.eof();
}

std::streampos AudioStream::getStart(){
	return start;
}
