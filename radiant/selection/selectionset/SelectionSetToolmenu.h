#pragma once

#include "iselectionset.h"
#include "imap.h"
#include <memory>
#include <sigc++/connection.h>

class wxToolBar;
class wxComboBox;
class wxCommandEvent;
class wxToolBarToolBase;

namespace selection
{

class SelectionSetToolmenu
{
private:
	wxComboBox* _dropdown;
	sigc::connection _setsChangedSignal;
	sigc::connection _mapEventHandler;
	int _dropdownToolId;

	wxToolBarToolBase* _clearAllButton;

public:
	SelectionSetToolmenu();

private:
	// Updates the available list items and widget sensitivity
	void update();

	void onSelectionChanged(wxCommandEvent& ev);
	void onEntryActivated(wxCommandEvent& ev);
	void onDeleteAllSetsClicked(wxCommandEvent& ev);

	void onMapEvent(IMap::MapEvent ev);
	void onRadiantShutdown();

	void connectToMapRoot();
	void disconnectFromMapRoot();
};

} // namespace selection
