#ifndef BB_AUDIO_STREAM_H
#define BB_AUDIO_STREAM_H

#include <cstdio>
#include <fstream>
#include <mutex>

class AudioStream{
protected:
	std::ifstream in;
	std::streampos start;
	unsigned char *buf;
	int buf_size;
	unsigned int channels,bits,samples,frequency;
#ifndef BB_MINGW
	std::mutex lock;
#else
	HANDLE lock;
#endif
	std::string path;

	// Refs outstanding; once release() marks the stream orphaned the
	// last Ref to go away deletes it
	int refs;
	bool orphaned;

public:
	struct Ref{
		AudioStream *stream;
		long pos;
		// private decode buffer: stream->buf is shared between all Refs
		unsigned char *refbuf;

		Ref( AudioStream *s );
		~Ref();

		size_t decode( unsigned char **buf );
		void reset();

		unsigned int getChannels();
		unsigned int getBits();
		unsigned int getFrequency();
		unsigned int getSamples();

		bool eof();
	};

	Ref *getRef();

	// give up ownership: stream deletes itself when the last Ref dies
	void release();

public:

	AudioStream( int size );
	virtual ~AudioStream();

	virtual bool readHeader()=0;

	virtual void seek( long pos )=0;
	virtual long pos()=0;
	virtual size_t decode()=0;

	bool init( const char *url );
	virtual size_t read( void *ptr, size_t size );
	bool eof();

	std::streampos getStart();
};


#endif
