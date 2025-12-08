#include "BlitzCC.h"
#include <wx/sstream.h>
#include <wx/txtstrm.h>
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include <iostream>

#ifndef BB_MSVC
#include <stdio.h>
#include <stdlib.h>
#endif

wxDEFINE_EVENT( BUILD_BEGIN,wxCommandEvent );
wxDEFINE_EVENT( BUILD_PROGRESS,wxCommandEvent );
wxDEFINE_EVENT( BUILD_END,wxCommandEvent );
wxDEFINE_EVENT( BUILD_KILL,wxCommandEvent );

BlitzCC::BlitzCC( wxEvtHandler *dest,const wxString &blitzpath ):dest(dest),blitzpath(blitzpath),proc(0){
}

BlitzCC::~BlitzCC(){
}

void BlitzCC::Execute( const wxString &p,const Target &t,const Preferences *pf ){
	path=p;
	target=t;
	prefs=pf;
	Run();
}

void BlitzCC::Kill(){
	if( proc==0 ) return;

	wxProcess::Kill( proc->GetPid() );
}

int BlitzCC::Monitor(){
	if( proc==0 ) return 0;

	wxTextInputStream outs( *proc->GetInputStream() );
	wxString lines;
	while( proc->IsInputOpened() ){
		wxString line=outs.ReadLine();

		if( line.size()>0 ){
			wxCommandEvent event( BUILD_PROGRESS );
			event.SetString( line );
			wxPostEvent( dest,event );
		}
	}

	return 0;
}

#ifndef BB_MSVC
// Linux/macOS: use popen() instead of wxExecute() which doesn't work from threads
int BlitzCC::RunCommand( const wxString &cmd, wxString &bundleId ){
	FILE *fp = popen( cmd.ToUTF8().data(), "r" );
	if( !fp ) return -1;

	char buffer[1024];
	while( fgets( buffer, sizeof(buffer), fp ) != NULL ){
		wxString line = wxString::FromUTF8( buffer );
		line.Trim();

		if( line.size() > 0 ){
			wxCommandEvent event( BUILD_PROGRESS );
			event.SetString( line );
			wxPostEvent( dest, event );

			// TODO: avoid this dirty hack...
			if( line.StartsWith("Signing ") && line.EndsWith("...") ){
				bundleId = line.Mid( wxString("Signing ").size() );
				bundleId = bundleId.Mid( 0, bundleId.size()-3 );
			}
		}
	}

	int status = pclose( fp );
	return WEXITSTATUS( status );
}
#endif

wxThread::ExitCode BlitzCC::Entry(){
	wxPostEvent( dest,wxCommandEvent( BUILD_BEGIN ) );

	wxFileName out( path );
	out.ClearExt();
	out.SetExt( "app" );
	out.Normalize( wxPATH_NORM_ABSOLUTE );

	wxString args;
	if( !target.host ){
		args += " -c -sign "+prefs->signId;
		if( target.platform=="ios" ){ // only need entitlement when deploying to real device
			args +=" -team "+prefs->teamId;
		}
		args += " -target "+target.platform+" -o "+out.GetFullPath();
	}
#ifndef BB_MSVC
	else {
		// Non-Windows: GCC backend doesn't support JIT, compile to temp exe and run it
		wxFileName tempExe( path );
		tempExe.ClearExt();
		tempExe.Normalize( wxPATH_NORM_ABSOLUTE );
		args += " -o " + tempExe.GetFullPath();
	}
#endif
	args += " "+path;

	wxString bundleId;

#ifdef BB_MSVC
	// Windows: use wxExecute
	proc=new wxProcess( 0 );
	proc->Redirect();
	wxExecute( blitzpath+"/bin/blitzcc"+args,wxEXEC_ASYNC|wxEXEC_HIDE_CONSOLE,proc );
	wxTextInputStream outs( *proc->GetInputStream() );
	wxString lines;
	while( proc->IsInputOpened() ){
		wxString line=outs.ReadLine();

		if( line.size()>0 ){
			wxCommandEvent event( BUILD_PROGRESS );
			event.SetString( line );
			wxPostEvent( dest,event );

			// TODO: avoid this dirty hack...
			if( line.StartsWith("Signing ") && line.EndsWith("...") ){
				bundleId=line.Mid( wxString("Signing ").size() );
				bundleId=bundleId.Mid( 0,bundleId.size()-3 );
			}
		}
	}
#else
	// Linux/macOS: use popen() to avoid wxExecute thread issues
	wxString cmd = blitzpath + "/bin/blitzcc" + args + " 2>&1";
	RunCommand( cmd, bundleId );
#endif

	std::cout<<bundleId<<std::endl;

	if( target.platform=="ios-sim" ){
#ifdef BB_MSVC
		proc=new wxProcess( 0 );
		proc->Redirect();
		wxExecute( "xcrun simctl install "+target.id+" "+out.GetFullPath(),wxEXEC_ASYNC|wxEXEC_HIDE_CONSOLE,proc );
		Monitor();

		proc=new wxProcess( 0 );
		proc->Redirect();
		wxExecute( "xcrun simctl launch --console --terminate-running-process "+target.id+" "+bundleId,wxEXEC_ASYNC|wxEXEC_HIDE_CONSOLE,proc );
		Monitor();
#else
		wxString dummy;
		RunCommand( "xcrun simctl install "+target.id+" "+out.GetFullPath()+" 2>&1", dummy );
		RunCommand( "xcrun simctl launch --console --terminate-running-process "+target.id+" "+bundleId+" 2>&1", dummy );
#endif
	}else if( target.platform=="ios" ){
#ifdef BB_MSVC
		proc=new wxProcess( 0 );
		proc->Redirect();
		wxExecute( "ios-deploy --bundle "+out.GetFullPath(),wxEXEC_ASYNC|wxEXEC_HIDE_CONSOLE,proc );
		Monitor();
#else
		wxString dummy;
		RunCommand( "ios-deploy --bundle "+out.GetFullPath()+" 2>&1", dummy );
#endif
	}
#ifndef BB_MSVC
	else if( target.host ){
		// Non-Windows: run the compiled executable
		wxFileName tempExe( path );
		tempExe.ClearExt();
		tempExe.Normalize( wxPATH_NORM_ABSOLUTE );

		// Change to the source file directory so relative paths work
		wxString sourceDir = wxFileName( path ).GetPath();
		wxString runCmd = "cd \"" + sourceDir + "\" && blitzpath=\"" + blitzpath + "\" \"" + tempExe.GetFullPath() + "\" 2>&1";

		wxString dummy;
		RunCommand( runCmd, dummy );
	}
#endif

	wxPostEvent( dest,wxCommandEvent( BUILD_END ) );

	return (wxThread::ExitCode)0;
}
