#ifndef FILEVIEW_H
#define FILEVIEW_H

#include "std.h"
#include <wx/stc/stc.h>
#include <wx/filename.h>
#include "BlitzCC.h"

class FileView : public wxPanel{
private:
  static void LoadKeywords();

  wxString path, source;
	wxStyledTextCtrl* text;
	bool dirty;
	BlitzCC *cc;
	wxDateTime storedModTime;  // File modification time when loaded
	bool checkingExternal;     // Prevent re-entry during reload prompt

	void Open( wxString &path );

	void OnTextEvent( wxStyledTextEvent& event );

	void EmitDirtyEvent();
public:
  FileView( wxString &path,wxWindow *parent,wxWindowID id );

	wxString GetTitle();
	wxString GetPath();
	wxString GetSource();
	bool IsDirty();

  bool Save();
  bool Save( wxString &newPath );

	void Execute( const Target &target,const Preferences *prefs );
	void Kill();
	void Build( wxString &out );

	// Error highlighting
	void HighlightErrorLine( int line );
	void ClearErrorHighlights();
	void GotoLine( int line );

	// External modification detection
	bool IsModifiedExternally();
	void Reload();
	void UpdateStoredModTime();
};

wxDECLARE_EVENT(FILE_VIEW_DIRTY_EVENT, wxCommandEvent);

#endif
