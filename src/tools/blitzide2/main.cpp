#include <wx/wxprec.h>
#include <wx/cmdline.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif
#include "MainFrame.h"
#include "dpi.h"

wxString blitzpath;

// Auto-detect blitzpath from executable location
// Executable is in _release/bin/ide2, so blitzpath is parent of bin/
static wxString detectBlitzPath() {
	wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
	exePath.MakeAbsolute();
	// Get directory containing executable (bin/)
	wxString binDir = exePath.GetPath();
	// Go up one level to get _release/ using simple string manipulation
	wxFileName parentDir = wxFileName::DirName(binDir);
	parentDir.RemoveLastDir();
	return parentDir.GetPath(wxPATH_GET_VOLUME);
}

class MyApp : public wxApp{
private:
	MainFrame *frame;

	wxArrayString filesToOpen;
public:
	virtual bool OnInit();

	virtual void OnInitCmdLine( wxCmdLineParser& parser );
	virtual bool OnCmdLineParsed( wxCmdLineParser& parser );
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit(){
	if( !wxApp::OnInit() ){
		return false;
	}

	// Try environment variable first, then auto-detect from executable location
	if( !wxGetEnv( "blitzpath",&blitzpath ) || blitzpath.length()==0 ){
		blitzpath = detectBlitzPath();
	}

	// Verify blitzpath is valid (check for bin/blitzcc)
	wxString blitzccPath = blitzpath + "/bin/blitzcc";
	if( !wxFileExists( blitzccPath ) ){
		wxMessageBox( "Could not find blitzcc compiler at:\n" + blitzccPath + "\n\nDetected blitzpath: " + blitzpath,
			"Blitz3D \"NG\"", wxOK|wxICON_ERROR );
		return false;
	}

	frame = new MainFrame( "Blitz3D" );
	frame->SetClientSize( frame->FromDIP( wxSize( 900,600 ) ) );
	frame->AddFiles( filesToOpen );
	frame->Show( true );
	return true;
}

void MyApp::OnInitCmdLine( wxCmdLineParser& parser ){
	parser.SetSwitchChars( wxT("-") );
	parser.AddParam( wxT("input files"),wxCMD_LINE_VAL_STRING,wxCMD_LINE_PARAM_MULTIPLE|wxCMD_LINE_PARAM_OPTIONAL );
}

bool MyApp::OnCmdLineParsed( wxCmdLineParser& parser ){
	for( int i=0;i<parser.GetParamCount();i++ ){
		wxString param=parser.GetParam(i);
		filesToOpen.Add( param );
	}
	return true;
}
