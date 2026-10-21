#include "package.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <system_error>
#include <cstdlib>
#include <cctype>

#ifdef BB_WINDOWS
#include <io.h>
#include <direct.h>
#include <stdlib.h>
#define PATH_MAX _MAX_PATH
#define getwd( buff ) _getcwd( buff,_MAX_PATH )
#define access( a,b ) (_access( a,b )==0)
#define chdir _chdir
#define F_OK 00
#define FILE_EXISTS 0
#else
#include <unistd.h>
#define FILE_EXISTS
#endif
#include <limits.h>

#define RUN( args ) if( system( std::string(args).c_str() )!=0 ) { std::cerr<<"error on "<<__FILE__<<":"<<__LINE__<<std::endl;abort(); }

// Android packaging versions: keep in sync with docs (CLAUDE.md, Android section)
static const char *BUILD_TOOLS_VERSION="34.0.0";
static const char *TARGET_SDK_VERSION="34";

// Quote a path argument for a system() command line. Local copy of the
// shellQuote in ../std.cpp: std.h needs the CMake include paths, and this file
// must also build standalone.
static std::string shellQuote( const std::string &s ){
#ifdef _WIN32
	// cmd.exe has no POSIX quoting; keep the current double-quote behaviour.
	return "\""+s+"\"";
#else
	// POSIX sh: single quotes, with embedded ' turned into '\''
	std::string out="'";
	for( size_t i=0;i<s.size();++i ){
		if( s[i]=='\'' ) out+="'\\''";
		else out+=s[i];
	}
	return out+"'";
#endif
}

namespace fs=std::filesystem;

// Data/media extensions auto-bundled when the source dir has no assets/ folder.
static bool isBundledExtension( const fs::path &file ){
	static const char *exts[]={
		"png","jpg","jpeg","bmp","tga","gif","wav","ogg","mp3","mid","ttf","fnt",
		"b3d","3ds","x","md2","obj","mtl","glb","gltf","txt","json","xml","csv",
		"ini","cfg","dat"
	};
	std::string ext=file.extension().string();
	if( ext.size()<2 ) return false;
	ext=ext.substr( 1 );
	std::transform( ext.begin(),ext.end(),ext.begin(),[]( unsigned char c ){ return (char)std::tolower( c ); } );
	for( const char *e:exts ){
		if( ext==e ) return true;
	}
	return false;
}

static void copyAsset( const fs::path &src,const fs::path &dst ){
	std::error_code ec;
	fs::create_directories( dst.parent_path(),ec );
	if( !ec ) fs::copy_file( src,dst,fs::copy_options::overwrite_existing,ec );
	if( ec ){
		std::cerr<<"Unable to bundle asset "<<src.string()<<": "<<ec.message()<<std::endl;
		exit( 1 );
	}
}

// Auto-bundle assets next to the source file:
// - if <sourceDir>/assets exists, copy that folder only (no extension filter);
// - otherwise copy files with a known media/data extension, skipping hidden
//   entries and tmp/ folders, up to 10 levels deep.
static void autoBundleAssets( const std::string &sourceDir,const std::string &assetsDir ){
	std::error_code ec;
	fs::path root=sourceDir;
	fs::path assetsSrc=root/"assets";
	bool onlyAssets=fs::is_directory( assetsSrc,ec );
	if( onlyAssets ) root=assetsSrc;

	fs::recursive_directory_iterator it( root,ec ),end;
	if( ec ){
		std::cerr<<"Unable to scan "<<root.string()<<": "<<ec.message()<<std::endl;
		exit( 1 );
	}
	for( ;it!=end;it.increment( ec ) ){
		if( ec ){
			std::cerr<<"Unable to scan "<<root.string()<<": "<<ec.message()<<std::endl;
			exit( 1 );
		}
		const fs::path &p=it->path();
		std::string name=p.filename().string();
		std::error_code sec;
		if( it->is_directory( sec ) ){
			if( !onlyAssets && ( name.empty() || name[0]=='.' || name=="tmp" ) ){
				it.disable_recursion_pending();
			}else if( it.depth()>=9 ){
				it.disable_recursion_pending();
			}
			continue;
		}
		if( !it->is_regular_file( sec ) ) continue;
		if( !onlyAssets && ( name[0]=='.' || !isBundledExtension( p ) ) ) continue;
		copyAsset( p,fs::path( assetsDir )/p.lexically_relative( root ) );
	}
}

void createApk( const std::string &out,const std::string &tmpdir,const std::string &home,const std::string &toolchain,const BundleInfo &bundle,const Target &target,const std::string &rt,const std::string &androidsdk ){
	std::string libdir=toolchain+"/lib";

	std::string btversion=BUILD_TOOLS_VERSION;
	std::string buildtools=androidsdk+"/build-tools/"+btversion;
	std::string aapt=buildtools+"/aapt";
	std::string aapt2=buildtools+"/aapt2";
	std::string d8=buildtools+"/d8";
	std::string apksigner=buildtools+"/apksigner";
	std::string zipalign=buildtools+"/zipalign";
	std::string androidjar=androidsdk+"/platforms/android-"+TARGET_SDK_VERSION+"/android.jar";

	// TODO: support release keys...
	std::string keystore=home+"/cfg/debug.keystore";
	std::string keystorepass="pass:android";

	std::string manifest=tmpdir+"/AndroidManifest.xml";

	std::string resdir=tmpdir+"/res";

	RUN( "mkdir -p "+shellQuote( resdir ) );

	// icons...
	RUN( "mkdir -p "+shellQuote( resdir+"/mipmap-hdpi" ) );
	RUN( "mkdir -p "+shellQuote( resdir+"/mipmap-mdpi" ) );
	RUN( "mkdir -p "+shellQuote( resdir+"/mipmap-xhdpi" ) );
	RUN( "mkdir -p "+shellQuote( resdir+"/mipmap-xxhdpi" ) );
	RUN( "mkdir -p "+shellQuote( resdir+"/mipmap-xxxhdpi" ) );

	RUN( "cp "+shellQuote( home+"/cfg/bbexe.png" )+" "+shellQuote( resdir+"/mipmap-hdpi/ic_launcher.png" ) );
	RUN( "cp "+shellQuote( home+"/cfg/bbexe.png" )+" "+shellQuote( resdir+"/mipmap-mdpi/ic_launcher.png" ) );
	RUN( "cp "+shellQuote( home+"/cfg/bbexe.png" )+" "+shellQuote( resdir+"/mipmap-xhdpi/ic_launcher.png" ) );
	RUN( "cp "+shellQuote( home+"/cfg/bbexe.png" )+" "+shellQuote( resdir+"/mipmap-xxhdpi/ic_launcher.png" ) );
	RUN( "cp "+shellQuote( home+"/cfg/bbexe.png" )+" "+shellQuote( resdir+"/mipmap-xxxhdpi/ic_launcher.png" ) );

	// values...
	RUN( "mkdir -p "+shellQuote( resdir+"/values" ) );

	std::ofstream colors;
	colors.open( resdir+"/values/colors.xml" );
	colors<<"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	colors<<"<resources>\n";
	colors<<"  <color name=\"colorPrimary\">#3F51B5</color>\n";
	colors<<"  <color name=\"colorPrimaryDark\">#303F9F</color>\n";
	colors<<"  <color name=\"colorAccent\">#FF4081</color>\n";
	colors<<"</resources>\n";
	colors.close();

	std::ofstream strings;
	strings.open( resdir+"/values/strings.xml" );
	strings<<"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	strings<<"<resources>\n";
	strings<<"    <string name=\"app_name\">"+bundle.appName+"</string>\n";
	strings<<"</resources>\n";
	strings.close();

	std::ofstream styles;
	styles.open( resdir+"/values/styles.xml" );
	styles<<"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	styles<<"<resources>\n";
	styles<<"  <style name=\"AppTheme\" parent=\"android:Theme.Holo.Light.DarkActionBar\">\n";
	styles<<"  </style>\n";
	styles<<"</resources>\n";
	styles.close();

	// manifest...
	std::string packageId=bundle.identifier;
	if( packageId.empty() ) packageId="com.blitz3dng.game";

	// Extract API level from version string (e.g., "android-24" -> "24")
	std::string apiLevel=target.version;
	size_t dashpos=apiLevel.rfind('-');
	if( dashpos!=std::string::npos ){
		apiLevel=apiLevel.substr(dashpos+1);
	}

	std::ofstream m;
	m.open( manifest );
	m<<"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	m<<"<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n";
	m<<"  package=\""+packageId+"\"\n";
	m<<"  android:versionCode=\"1\"\n";
	m<<"  android:versionName=\"1.0\"\n";
	m<<"  android:installLocation=\"auto\">\n";
	m<<"\n";
	m<<"  <uses-sdk android:minSdkVersion=\""+apiLevel+"\"\n";
	m<<"            android:targetSdkVersion=\""+std::string( TARGET_SDK_VERSION )+"\" />\n";
	m<<"\n";
	std::ifstream content( toolchain+"/manifest.template.xml" );
	if( content.is_open() ){
		std::string line;
		while( getline( content,line ) ){
			m<<"  "<<line<<'\n';
		}
		content.close();
	}
	m<<"</manifest>\n";
	m.close();

	// assets...
	RUN( "mkdir -p "+shellQuote( tmpdir+"/assets" ) );
	bundleFiles( bundle,tmpdir+"/assets" );

	// Auto-bundle assets from the source directory (see autoBundleAssets)
	if( !bundle.sourceDir.empty() ){
		std::cout<<"Bundling assets from "<<bundle.sourceDir<<"..."<<std::endl;
		autoBundleAssets( bundle.sourceDir,tmpdir+"/assets" );
	}

	// build the apk...
	std::string jars="";

	const Target::Runtime &rti=target.runtimes.at( rt );
	for( std::string mod:rti.modules ){
		const Target::Module &m=target.modules.at( mod );
		for( std::string lib:m.extra_files ){
			if( lib.substr( std::max( 4,(int)lib.size() )-4 )==".jar" ){
				jars=jars+" "+shellQuote( libdir+"/"+lib );
			}
		}
	}

	if( jars!="" ){
		RUN( shellQuote( d8 )+" --output "+shellQuote( tmpdir )+jars );
	}

	for( std::string mod:rti.modules ){
		const Target::Module &m=target.modules.at( mod );
		for( std::string lib:m.extra_files ){
			if( lib.substr( std::max( 3,(int)lib.size() )-3 )==".so" ){
				RUN( "cp "+shellQuote( libdir+"/"+lib )+" "+shellQuote( tmpdir+"/lib/"+target.arch+"/" ) );
			}
		}
	}

	RUN( shellQuote( aapt2 )+" compile -v --dir "+shellQuote( resdir )+" -o "+shellQuote( tmpdir+"/resources.zip" ) );
	RUN( shellQuote( aapt2 )+" link -v -o "+shellQuote( tmpdir+"/unaligned.apk" )+" -I "+shellQuote( androidjar )+" --manifest "+shellQuote( manifest )+" -A "+shellQuote( tmpdir+"/assets" )+" "+shellQuote( tmpdir+"/resources.zip" ) );

	// since relative paths create issues...
	char dir[PATH_MAX];
	getcwd( dir,PATH_MAX );
	std::string currdir=dir;
	chdir( tmpdir.c_str() );
	RUN( "zip -u unaligned.apk *.dex lib/**/*.so" );
	chdir( currdir.c_str() );

	RUN( shellQuote( zipalign )+" -f 4 "+shellQuote( tmpdir+"/unaligned.apk" )+" "+shellQuote( tmpdir+"/signed.apk" ) );

	if( access( keystore.c_str(),F_OK ) ) {
		RUN( ("keytool -genkey -v -keystore "+shellQuote( keystore )+" -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname \"C=US, O=Android, CN=Android Debug\"" ).c_str() );
	}
	RUN( shellQuote( apksigner )+" sign --ks "+shellQuote( keystore )+" --ks-pass "+keystorepass+" "+shellQuote( tmpdir+"/signed.apk" ) );

	RUN( "cp "+shellQuote( tmpdir+"/signed.apk" )+" "+shellQuote( out ) );
}
