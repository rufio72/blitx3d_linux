#ifndef BLITZCC_H
#define BLITZCC_H

#include "Preferences.h"

#include <wx/thread.h>
#include <wx/process.h>
#include <atomic>
#ifndef BB_MSVC
#include <sys/types.h>
#include <signal.h>
#endif

wxDECLARE_EVENT( BUILD_BEGIN,wxCommandEvent );
wxDECLARE_EVENT( BUILD_PROGRESS,wxCommandEvent );
wxDECLARE_EVENT( BUILD_GCC_PHASE,wxCommandEvent );  // Signal start of GCC compilation phase
wxDECLARE_EVENT( BUILD_ERROR,wxCommandEvent );  // Error with line number for highlighting
wxDECLARE_EVENT( BUILD_END,wxCommandEvent );
wxDECLARE_EVENT( BUILD_KILL,wxCommandEvent );

struct Target{
	wxString name,id;
	wxString platform;
	bool host,emulator,running;
};

class BlitzCC:public wxThread{
protected:
	virtual ExitCode Entry();

	wxEvtHandler *dest;
	wxString blitzpath,path;
	Target target;
	const Preferences *prefs;
	wxProcess *proc;
#ifndef BB_MSVC
	pid_t childPid;
	std::atomic<bool> shouldStop;
#endif

	int Monitor();
#ifndef BB_MSVC
	int RunCommand( const wxString &cmd, wxString &bundleId );
#endif
public:
	BlitzCC( wxEvtHandler *dest,const wxString &blitzpath );
	~BlitzCC();

	void Execute( const wxString &path,const Target &target,const Preferences *prefs );

	void Kill();
};

#endif
