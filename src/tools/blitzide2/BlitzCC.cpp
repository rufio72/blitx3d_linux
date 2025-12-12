#include "BlitzCC.h"
#include <wx/sstream.h>
#include <wx/txtstrm.h>
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include <iostream>

#ifndef BB_MSVC
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

wxDEFINE_EVENT( BUILD_BEGIN,wxCommandEvent );
wxDEFINE_EVENT( BUILD_PROGRESS,wxCommandEvent );
wxDEFINE_EVENT( BUILD_GCC_PHASE,wxCommandEvent );
wxDEFINE_EVENT( BUILD_ERROR,wxCommandEvent );
wxDEFINE_EVENT( BUILD_END,wxCommandEvent );
wxDEFINE_EVENT( BUILD_KILL,wxCommandEvent );

BlitzCC::BlitzCC( wxEvtHandler *dest,const wxString &blitzpath ):wxThread(wxTHREAD_DETACHED),dest(dest),blitzpath(blitzpath),proc(0){
#ifndef BB_MSVC
	childPid = 0;
	shouldStop = false;
#endif
}

BlitzCC::~BlitzCC(){
}

void BlitzCC::Execute( const wxString &p,const Target &t,const Preferences *pf ){
	path=p;
	target=t;
	prefs=pf;

	// Debug: show what we're trying to compile
	std::cout << "BlitzCC::Execute called" << std::endl;
	std::cout << "  Path: " << path.ToStdString() << std::endl;
	std::cout << "  Target: " << target.platform.ToStdString() << std::endl;
	std::cout << "  Blitzpath: " << blitzpath.ToStdString() << std::endl;

	if( Create() == wxTHREAD_NO_ERROR ){
		std::cout << "  Thread created, starting..." << std::endl;
		Run();
	} else {
		std::cout << "  ERROR: Thread creation failed!" << std::endl;
	}
}

void BlitzCC::Kill(){
#ifndef BB_MSVC
	// Linux/macOS: just set stop flag - the worker thread will handle the kill
	// This avoids race conditions with childPid access
	shouldStop = true;
	std::cout << "Kill() called - stop flag set" << std::endl;
#else
	// Windows: use wxProcess
	if( proc==0 ) return;
	wxProcess::Kill( proc->GetPid() );
#endif
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
	std::cout << "BlitzCC::Entry() started" << std::endl;
	wxPostEvent( dest,wxCommandEvent( BUILD_BEGIN ) );

	wxFileName out( path );
	out.ClearExt();
	out.SetExt( "app" );
	out.Normalize( wxPATH_NORM_ABSOLUTE );
	std::cout << "  target.host = " << (target.host ? "true" : "false") << std::endl;

	wxString args = " -d";  // Always enable debug mode
	wxString exePath;  // Path to the executable we'll create

	if( !target.host ){
		args += " -c -sign "+prefs->signId;
		if( target.platform=="ios" ){ // only need entitlement when deploying to real device
			args +=" -team "+prefs->teamId;
		}
		args += " -target "+target.platform+" -o "+out.GetFullPath();
		exePath = out.GetFullPath();
	}
#ifndef BB_MSVC
	else {
		// Non-Windows: GCC backend - use two-phase compilation
		std::cout << "  Using GCC backend (host mode)" << std::endl;
		wxFileName tempExe( path );
		tempExe.ClearExt();
		tempExe.Normalize( wxPATH_NORM_ABSOLUTE );
		exePath = tempExe.GetFullPath();
		std::cout << "  Output exe: " << exePath.ToStdString() << std::endl;

		// Delete old executable before compiling to avoid running stale binary on error
		if( wxFileExists( exePath ) ){
			wxRemoveFile( exePath );
		}

		// Phase 1: Blitz3D compilation (generate C code)
		// Use -c flag to only generate C code and get the GCC command
		wxString blitzArgs = " -d -c -o " + exePath + " " + path;
		// Set blitzpath environment variable for the child process
		wxString blitzCmd = "blitzpath=\"" + blitzpath + "\" " + blitzpath + "/bin/blitzcc" + blitzArgs + " 2>&1";
		std::cout << "  Blitz cmd: " << blitzCmd.ToStdString() << std::endl;

		wxString gccCmd;
		wxString cFile;

		std::cout << "  Opening popen..." << std::endl;
		FILE *fp = popen( blitzCmd.ToUTF8().data(), "r" );
		if( !fp ){
			std::cout << "  ERROR: popen failed!" << std::endl;
			wxPostEvent( dest,wxCommandEvent( BUILD_END ) );
			return (wxThread::ExitCode)0;
		}
		std::cout << "  popen OK, reading output..." << std::endl;

		char buffer[4096];
		bool blitzError = false;
		int lineCount = 0;
		while( fgets( buffer, sizeof(buffer), fp ) != NULL ){
			wxString line = wxString::FromUTF8( buffer );
			line.Trim();
			lineCount++;
			std::cout << "  [" << lineCount << "] " << line.ToStdString() << std::endl;

			if( line.size() > 0 ){
				// Check for special output markers
				if( line.StartsWith("C_FILE:") ){
					cFile = line.Mid( 7 );  // Extract C file path
					std::cout << "  -> Found C_FILE: " << cFile.ToStdString() << std::endl;
				} else if( line.StartsWith("GCC_CMD:") ){
					gccCmd = line.Mid( 8 );  // Extract GCC command
					std::cout << "  -> Found GCC_CMD: " << gccCmd.ToStdString() << std::endl;
				} else {
					// Regular output - send to Blitz3D tab
					wxCommandEvent event( BUILD_PROGRESS );
					event.SetString( line );
					wxPostEvent( dest, event );

					// Check for errors and parse line number
					// Format: "filepath":line:col:line:col:message
					wxString lower = line.Lower();
					if( lower.Contains("error") || lower.Contains("not found") ){
						blitzError = true;

						// Try to parse error line number
						// Format: "filepath":line:col:line:col:message
						if( line.StartsWith("\"") ){
							// Find closing quote
							size_t pos = 1;
							while( pos < line.Length() && line[pos] != '"' ) pos++;
							if( pos < line.Length() ){
								pos++;  // Skip closing quote
								if( pos < line.Length() && line[pos] == ':' ){
									pos++;  // Skip colon
									// Now we should be at the line number
									wxString lineNumStr;
									while( pos < line.Length() && line[pos] >= '0' && line[pos] <= '9' ){
										lineNumStr += line[pos];
										pos++;
									}
									if( !lineNumStr.IsEmpty() ){
										long lineNum;
										if( lineNumStr.ToLong( &lineNum ) ){
											wxCommandEvent errorEvent( BUILD_ERROR );
											errorEvent.SetInt( (int)lineNum );
											errorEvent.SetString( line );
											wxPostEvent( dest, errorEvent );
										}
									}
								}
							}
						}
					}
				}
			}
		}
		std::cout << "  Read " << lineCount << " lines from blitzcc" << std::endl;

		int status = pclose( fp );
		int blitzResult = WEXITSTATUS( status );
		std::cout << "  blitzcc exit code: " << blitzResult << std::endl;
		std::cout << "  gccCmd empty: " << (gccCmd.IsEmpty() ? "yes" : "no") << std::endl;

		// If Blitz3D compilation failed, stop here
		if( blitzResult != 0 || blitzError || gccCmd.IsEmpty() ){
			std::cout << "  Blitz3D compilation failed or no GCC_CMD found" << std::endl;
			wxPostEvent( dest,wxCommandEvent( BUILD_END ) );
			return (wxThread::ExitCode)0;
		}

		// Phase 2: GCC compilation
		wxPostEvent( dest,wxCommandEvent( BUILD_GCC_PHASE ) );

		fp = popen( (gccCmd + " 2>&1").ToUTF8().data(), "r" );
		if( !fp ){
			wxPostEvent( dest,wxCommandEvent( BUILD_END ) );
			return (wxThread::ExitCode)0;
		}

		while( fgets( buffer, sizeof(buffer), fp ) != NULL ){
			wxString line = wxString::FromUTF8( buffer );
			line.Trim();

			if( line.size() > 0 ){
				wxCommandEvent event( BUILD_PROGRESS );
				event.SetString( line );
				wxPostEvent( dest, event );
			}
		}

		status = pclose( fp );
		int gccResult = WEXITSTATUS( status );

		// If GCC compilation failed, stop here
		if( gccResult != 0 ){
			wxPostEvent( dest,wxCommandEvent( BUILD_END ) );
			return (wxThread::ExitCode)0;
		}

		// Phase 3: Run the executable using fork() to track PID
		if( wxFileExists( exePath ) ){
			wxString sourceDir = wxFileName( path ).GetPath();

			std::cout << "  Running executable: " << exePath.ToStdString() << std::endl;

			// Reset stop flag
			shouldStop = false;

			// Fork to run the executable
			childPid = fork();
			if( childPid == 0 ){
				// Child process
				chdir( sourceDir.ToUTF8().data() );
				setenv( "blitzpath", blitzpath.ToUTF8().data(), 1 );
				execl( exePath.ToUTF8().data(), exePath.ToUTF8().data(), (char*)NULL );
				_exit(1); // If exec fails
			} else if( childPid > 0 ){
				// Parent process - wait for child or stop signal
				std::cout << "  Child process PID: " << childPid << std::endl;
				int status;
				while( !shouldStop ){
					pid_t result = waitpid( childPid, &status, WNOHANG );
					if( result == childPid ){
						// Child exited
						std::cout << "  Child process exited" << std::endl;
						break;
					} else if( result == -1 ){
						// Error
						break;
					}
					// Sleep a bit to avoid busy waiting
					usleep( 100000 ); // 100ms
				}
				if( shouldStop ){
					std::cout << "  Stop requested, killing child" << std::endl;
					kill( childPid, SIGTERM );
					waitpid( childPid, &status, 0 );
				}
				childPid = 0;
			}
		}

		wxPostEvent( dest,wxCommandEvent( BUILD_END ) );
		return (wxThread::ExitCode)0;
	}
#endif
	args += " "+path;

	wxString bundleId;
	int compileResult = 0;

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
	compileResult = RunCommand( cmd, bundleId );
#endif

	std::cout<<bundleId<<std::endl;

	// Only run executable if compilation succeeded
	if( compileResult != 0 ){
		wxPostEvent( dest,wxCommandEvent( BUILD_END ) );
		return (wxThread::ExitCode)0;
	}

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

	wxPostEvent( dest,wxCommandEvent( BUILD_END ) );

	return (wxThread::ExitCode)0;
}
