
#include "../stdutil/stdutil.h"
#include "audio.openal.h"
#include <bb/audio/ogg_stream.h>
#include <bb/audio/wav_stream.h>
#include <bb/audio/mp3_stream.h>

#ifndef __APPLE__
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#endif

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <string.h>
#include <math.h>

#define NUM_BUFFERS 6
#define BUFFER_SIZE 4096

static std::set<BBChannel*> channel_set;

class OpenALChannel : public BBChannel{
public:
	AudioStream::Ref *stream;

	ALuint source,buffers[NUM_BUFFERS];
	ALuint frequency;
	ALenum format;

	std::thread playbackThread;
	std::atomic<bool> playbackRunning;
	std::atomic<bool> paused;
	std::atomic<bool> looping;

	OpenALChannel():stream(0),source(0),frequency(0),playbackRunning(false),paused(false),looping(false){}

	~OpenALChannel(){
		playbackRunning=false;
		if( playbackThread.joinable() ) playbackThread.join();
		delete stream;
	}

	bool setStream( AudioStream *s ){
		stream=s->getRef();

		unsigned int bits=stream->getBits(),channels=stream->getChannels();
		frequency=stream->getFrequency();

		format=0;
		if( bits==8 ){
			if( channels==1 )
				format=AL_FORMAT_MONO8;
			else if( channels==2 )
				format=AL_FORMAT_STEREO8;
		}else if( bits==16 ){
			if( channels==1 )
				format=AL_FORMAT_MONO16;
			else if( channels==2 )
				format=AL_FORMAT_STEREO16;
		}
		if( format==0 ){
			LOGD( "unsupported format: %u bits, %u channels",bits,channels );
			return false;
		}

		alGenBuffers( NUM_BUFFERS,buffers );
		alGenSources( 1,&source );

		return alGetError()==AL_NO_ERROR;
	}

	bool queue( ALuint buffer ){
		unsigned char *buf;
		size_t size=stream->decode( &buf );
		if( size==0 && looping ){
			stream->reset();
			size=stream->decode( &buf );
		}
		if( size==0 ) return false;

		alBufferData( buffer,format,buf,size,stream->getFrequency() );
		alSourceQueueBuffers( source,1,&buffer );
		return true;
	}

	bool streaming(){
		return playbackRunning && ( looping || !stream->eof() );
	}

	void play(){
		if( !playbackRunning ){
			playbackRunning=true;
			playbackThread=std::thread( &playback,this );
		}
	}

	static void playback( OpenALChannel *channel ){
		{
			ALint val;

			for( int i=0;i<NUM_BUFFERS;i++ ){
				if( !channel->queue( channel->buffers[i] ) ) break;
			}

			if( alGetError()!=AL_NO_ERROR ){
				goto end;
			}

			alSourcePlay( channel->source );
			if( alGetError()!=AL_NO_ERROR ){
				goto end;
			}

			while( channel->streaming() ){
				if( channel->paused ){
					std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
					continue;
				}

				ALuint buffer;
				alGetSourcei( channel->source,AL_BUFFERS_PROCESSED,&val );
				if( val<=0 ){
					std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
					continue;
				}

				while( val-- ){
					alSourceUnqueueBuffers(channel->source, 1, &buffer);
					if( channel->queue( buffer )==false ){
						goto drain;
					}else if( alGetError()!=AL_NO_ERROR ){
						LOGD( "%s","error buffering..." );
						goto end;
					}
				}

				alGetSourcei( channel->source,AL_SOURCE_STATE,&val );
				if( val!=AL_PLAYING && !channel->paused ){
					alSourcePlay(channel->source);
				}
			}

drain:
			while( channel->playbackRunning ){
				alGetSourcei( channel->source,AL_SOURCE_STATE,&val );
				if( val!=AL_PLAYING && val!=AL_PAUSED ) break;
				std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
			}
		}

end:
		channel->playbackRunning=false;

		alDeleteSources( 1,&channel->source );
		alDeleteBuffers( NUM_BUFFERS,channel->buffers );
	}

	void stop(){
		if( !playbackRunning ) return;
		playbackRunning=false;
		alSourceStop( source );
	}
	void setPaused( bool p ){
		if( !playbackRunning ) return;
		paused=p;
		if( p ) alSourcePause( source );
		else alSourcePlay( source );
	}
	void setPitch( int pitch ){
		if( !playbackRunning || !frequency ) return;
		alSourcef( source,AL_PITCH,pitch/(float)frequency );
	}
	void setVolume( float volume ){
		if( !playbackRunning ) return;
		alSourcef( source,AL_GAIN,volume );
	}
	void setPan( float pan ){
		if( !playbackRunning ) return;
		if( pan<-1 ) pan=-1;
		if( pan>1 ) pan=1;
		alSourcei( source,AL_SOURCE_RELATIVE,AL_TRUE );
		alSource3f( source,AL_POSITION,pan,0,-sqrtf( 1-pan*pan ) );
	}
	void set3d( const float pos[3],const float vel[3] ){
		float p[3]={ pos[0],pos[1],-pos[2] };
		float v[3]={ vel[0],vel[1],-vel[2] };

		// get these from set3dOptions...
		alSourcef( source,AL_ROLLOFF_FACTOR,0.1 );
		alSourcef( source,AL_REFERENCE_DISTANCE,0.2 );

		alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );
		alSourcefv( source,AL_POSITION,p );
		alSourcefv( source,AL_VELOCITY,v );
		alSourcei( source,AL_SOURCE_RELATIVE,false );
		// alSourcef( source,AL_MIN_GAIN,0.0 );
		// alSourcef( source,AL_MAX_GAIN,100.0f );
	}
	bool isPlaying(){
		return playbackRunning && !paused;
	}
	float getDuration(){
		return (stream->getSamples() / (float)stream->getChannels()) / (float)stream->getFrequency();
	}
	float getPosition(){
		return (stream->pos / (float)stream->getChannels()) / (float)stream->getFrequency();
	}
};

class OpenALSound : public BBSound{
public:
	AudioStream *stream;
	ALenum format;

	bool loop;
	int pitch;
	float volume,pan;

	OpenALSound():stream(0),format(0),loop(false),pitch(0),volume(1),pan(0){
	}

	~OpenALSound(){
		// channels still playing hold Refs: the stream deletes itself
		// when the last one goes away
		if( stream ) stream->release();
	}

	bool setStream( AudioStream *s ){
		stream=s;
		return true;
	}

	void applyDefaults( OpenALChannel *channel ){
		channel->looping=loop;
		if( volume!=1 ) channel->setVolume( volume );
		if( pan ) channel->setPan( pan );
		if( pitch ) channel->setPitch( pitch );
	}

	BBChannel *play(){
		OpenALChannel *channel=new OpenALChannel();
		if( !channel->setStream( stream ) ){
			delete channel;
			return 0;
		}
		alDistanceModel( AL_NONE );
		channel->looping=loop;
		channel->play();
		applyDefaults( channel );
		channel_set.insert( channel );
		return channel;
	}

	BBChannel *play3d( const float pos[3],const float vel[3] ){
		OpenALChannel *channel=new OpenALChannel();
		if( !channel->setStream( stream ) ){
			delete channel;
			return 0;
		}
		channel->set3d( pos,vel );
		channel->looping=loop;
		channel->play();
		applyDefaults( channel );
		channel_set.insert( channel );
		return channel;
	}

	void setLoop( bool t ){
		loop=t;
	}

	void setPitch( int hertz ){
		pitch=hertz;
	}

	void setVolume( float t ){
		volume=t;
	}

	void setPan( float t ){
		pan=t;
	}
};

class OpenALAudioDriver : public BBAudioDriver{
protected:
	ALCdevice *dev;
	ALCcontext *ctx;

	AudioStream *loadStream( const std::string &filename,bool preload ){
		AudioStream *stream=0;

		// TODO: come up with something a little more clever
		const char *ext = strrchr( filename.c_str(),'.' );
		if( !ext ) ext=".wav";
		const char *exts[]={ ext,strcasecmp( ext + 1,"wav" )==0?".ogg":".wav",0 };
		int tries=0;
		while( exts[tries] ){
			if( strcasecmp( exts[tries] + 1,"wav" ) == 0 ){
				stream=new WAVAudioStream( BUFFER_SIZE );
			}else if( strcasecmp( exts[tries] + 1,"ogg" ) == 0 ){
				stream=new OGGAudioStream( BUFFER_SIZE );
			}else if( strcasecmp( exts[tries] + 1,"mp3" ) == 0 ){
				stream=new MP3AudioStream( BUFFER_SIZE );
			}

			if( !stream ) return 0;

			if( stream->init( filename.c_str() ) ){
				break;
			}else{
				delete stream;
				stream=0;
			}

			tries++;
		}

		return stream;
	}

public:
	OpenALAudioDriver():dev(0),ctx(0){
	}

	~OpenALAudioDriver(){
		while( channel_set.size() ) {
			BBChannel *c=*channel_set.begin();
			if( channel_set.erase( c ) ) delete c;
		}
		while( sound_set.size() ) freeSound( *sound_set.begin() );
		alcMakeContextCurrent( NULL );
		if( ctx ){ alcDestroyContext( ctx );ctx=0; }
		if( dev ){ alcCloseDevice( dev );dev=0; }
	}

	bool init(){
		if( !(dev=alcOpenDevice(NULL)) ){
			fprintf(stderr, "Oops\n");
			return false;
		}
		if( !(ctx=alcCreateContext(dev,NULL)) ){
			fprintf(stderr, "Oops2\n");
			return false;
		}
		alcMakeContextCurrent( ctx );

		return true;
	}

	BBSound *loadSound( const std::string &filename,bool use_3d ){
		AudioStream *stream=loadStream( filename,true );
		if( !stream ){
			return 0;
		}

		OpenALSound *sound=new OpenALSound();
		if( !sound->setStream( stream ) ){
			delete sound; // releases the stream if it was stored
			return 0;
		}

		sound_set.insert( sound );
		return sound;
	}

	void setPaused( bool paused ){
	}
	//master pause
	void setVolume( float volume ){
		alListenerf( AL_GAIN,volume );
	}
	//master volume

	void set3dOptions( float roll,float dopp,float dist ){
		alDopplerFactor( dopp );
	}

	void set3dListener( const float pos[3],const float vel[3],const float forward[3],const float up[3] ){
		float p[3]={ pos[0],pos[1],-pos[2] };
		float v[3]={ vel[0],vel[1],-vel[2] };

		// AL_ORIENTATION wants 6 floats: "at" followed by "up"
		float orient[6]={ forward[0],forward[1],-forward[2],up[0],up[1],-up[2] };

		alListenerfv( AL_POSITION,p );
		alListenerfv( AL_VELOCITY,v );
		alListenerfv( AL_ORIENTATION,orient );
	}

	BBChannel *playCDTrack( int track,int mode ){
		RTEX( "PlayCDTrack not implemented" );
	}

	BBChannel *playFile( const std::string &filename,bool use_3d ){
		AudioStream *stream=loadStream( filename,false );
		if( !stream ){
			return 0;
		}

		OpenALChannel *channel=new OpenALChannel();
		if( !channel->setStream( stream ) ){
			delete channel; // drops its Ref, if any
			stream->release();
			return 0;
		}
		// channel's Ref keeps the stream alive; deleted with the channel
		stream->release();

		channel->play();
		channel_set.insert( channel );
		return channel;
	}
};

static OpenALAudioDriver *driver;

BBMODULE_CREATE( audio_openal ){
	driver=d_new OpenALAudioDriver();
	if( !driver->init() ){
		delete driver;
		driver=0;
		return true;
	}
	gx_audio=driver;
	return true;
}

BBMODULE_DESTROY( audio_openal ){
	return true;
}
