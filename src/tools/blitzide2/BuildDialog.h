#ifndef BUILD_DIALOG
#define BUILD_DIALOG

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/notebook.h>

class BuildDialog:public wxDialog
{
public:
	BuildDialog( wxWindow * parent );

	void AddBlitzMessage( const wxString &m );
	void AddGccMessage( const wxString &m );
	void AddMessage( const wxString &m );  // Auto-routes to appropriate tab
	void SetBuildFinished( bool success );
	void StartGccPhase();  // Switch to GCC tab when GCC phase begins
	bool HasErrors() const { return hasErrors; }
private:
	wxNotebook *notebook;
	wxTextCtrl *blitzOutput;
	wxTextCtrl *gccOutput;
	wxButton *closeButton;
	wxStaticText *statusLabel;
	bool hasErrors;
	bool buildFinished;
	bool inGccPhase;

	void OnTerminate( wxCommandEvent & event );
	void OnClose( wxCommandEvent & event );

	DECLARE_EVENT_TABLE()
};


#endif
