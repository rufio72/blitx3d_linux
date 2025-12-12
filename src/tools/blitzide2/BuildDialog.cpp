#include "BuildDialog.h"
#include "BlitzCC.h"

enum {
	ID_TERMINATE = 1001,
	ID_CLOSE = 1002
};

wxBEGIN_EVENT_TABLE(BuildDialog, wxDialog)
	EVT_BUTTON( ID_TERMINATE, BuildDialog::OnTerminate )
	EVT_BUTTON( ID_CLOSE, BuildDialog::OnClose )
wxEND_EVENT_TABLE()

BuildDialog::BuildDialog ( wxWindow * parent )
	: wxDialog( parent, wxID_ANY, "Building...", wxDefaultPosition, wxSize(700, 450), wxCAPTION|wxRESIZE_BORDER|wxSTAY_ON_TOP )
	, hasErrors(false)
	, buildFinished(false)
	, inGccPhase(false)
{
	wxSize pos = (parent->GetSize() - GetSize()) * 0.5;
	SetPosition( parent->GetPosition() + wxPoint( pos.x, pos.y ) );

	wxBoxSizer *mainSizer = new wxBoxSizer( wxVERTICAL );

	// Status label at top
	statusLabel = new wxStaticText( this, wxID_ANY, "Compiling Blitz3D..." );
	wxFont boldFont = statusLabel->GetFont();
	boldFont.SetWeight( wxFONTWEIGHT_BOLD );
	statusLabel->SetFont( boldFont );
	mainSizer->Add( statusLabel, 0, wxALL | wxEXPAND, 10 );

	// Notebook with two tabs
	notebook = new wxNotebook( this, wxID_ANY );

	// Blitz3D Output tab
	wxPanel *blitzPanel = new wxPanel( notebook );
	wxBoxSizer *blitzSizer = new wxBoxSizer( wxVERTICAL );
	blitzOutput = new wxTextCtrl( blitzPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxHSCROLL );
	wxFont monoFont( 10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
	blitzOutput->SetFont( monoFont );
	blitzSizer->Add( blitzOutput, 1, wxEXPAND );
	blitzPanel->SetSizer( blitzSizer );
	notebook->AddPage( blitzPanel, "Blitz3D Output" );

	// GCC Output tab
	wxPanel *gccPanel = new wxPanel( notebook );
	wxBoxSizer *gccSizer = new wxBoxSizer( wxVERTICAL );
	gccOutput = new wxTextCtrl( gccPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxHSCROLL );
	gccOutput->SetFont( monoFont );
	gccSizer->Add( gccOutput, 1, wxEXPAND );
	gccPanel->SetSizer( gccSizer );
	notebook->AddPage( gccPanel, "GCC Output" );

	mainSizer->Add( notebook, 1, wxALL | wxEXPAND, 10 );

	// Button sizer
	wxBoxSizer *buttonSizer = new wxBoxSizer( wxHORIZONTAL );
	buttonSizer->AddStretchSpacer();

	wxButton *termButton = new wxButton( this, ID_TERMINATE, "Terminate" );
	buttonSizer->Add( termButton, 0, wxRIGHT, 5 );

	closeButton = new wxButton( this, ID_CLOSE, "Close" );
	closeButton->Enable( false );  // Disabled until build finishes
	buttonSizer->Add( closeButton, 0 );

	mainSizer->Add( buttonSizer, 0, wxALL | wxEXPAND, 10 );

	SetSizer( mainSizer );
}

void BuildDialog::AddBlitzMessage( const wxString &m ){
	blitzOutput->AppendText( m + "\n" );

	// Check for error patterns in the message
	wxString lower = m.Lower();
	if( lower.Contains("error") || lower.Contains("not found") ||
		lower.Contains("failed") ){
		hasErrors = true;
		// Highlight error lines in red
		long start = blitzOutput->GetLastPosition() - m.Length() - 1;
		if( start < 0 ) start = 0;
		blitzOutput->SetStyle( start, blitzOutput->GetLastPosition(), wxTextAttr( *wxRED ) );
	}
}

void BuildDialog::AddGccMessage( const wxString &m ){
	gccOutput->AppendText( m + "\n" );

	// Check for error patterns in the message
	wxString lower = m.Lower();
	if( lower.Contains("error:") || lower.Contains("undefined reference") ||
		lower.Contains("cannot find") ){
		hasErrors = true;
		// Highlight error lines in red
		long start = gccOutput->GetLastPosition() - m.Length() - 1;
		if( start < 0 ) start = 0;
		gccOutput->SetStyle( start, gccOutput->GetLastPosition(), wxTextAttr( *wxRED ) );
		// Switch to GCC tab to show the error
		notebook->SetSelection( 1 );
	}
}

void BuildDialog::AddMessage( const wxString &m ){
	// Auto-route messages based on content or current phase
	if( inGccPhase ){
		AddGccMessage( m );
	} else {
		AddBlitzMessage( m );
	}
}

void BuildDialog::StartGccPhase(){
	inGccPhase = true;
	statusLabel->SetLabel( "Compiling with GCC..." );
	// Don't switch tab automatically - user can switch if interested
}

void BuildDialog::SetBuildFinished( bool success ){
	buildFinished = true;
	closeButton->Enable( true );

	if( success && !hasErrors ){
		statusLabel->SetLabel( "Build successful!" );
		statusLabel->SetForegroundColour( wxColour( 0, 128, 0 ) );  // Green
	} else {
		statusLabel->SetLabel( "Build failed - see errors in tabs" );
		statusLabel->SetForegroundColour( *wxRED );
		hasErrors = true;
	}
	statusLabel->Refresh();

	// If no errors, auto-close after a brief moment
	if( !hasErrors ){
		EndModal( 0 );
	}
}

void BuildDialog::OnTerminate( wxCommandEvent & event ){
	wxPostEvent( GetParent(), wxCommandEvent( BUILD_KILL ) );
}

void BuildDialog::OnClose( wxCommandEvent & event ){
	EndModal( hasErrors ? 1 : 0 );
}
