#pragma once

#include "SREntity.h"
#include "StimTypes.h"

#include <wx/event.h>
#include "wxutil/TreeView.h"

class wxTextCtrl;
class wxButton;
class wxChoice;
class wxPanel;
class wxComboBox;
class wxBitmapComboBox;
class wxControl;
class wxCheckBox;
class wxSpinCtrl;
class wxSpinCtrlDouble;
class wxSpinDoubleEvent;
class wxBoxSizer;

// There's a bug in the MSW implementation of wxBitmapComboBox, don't use it in 3.0.0
#if wxCHECK_VERSION(3,0,1) && defined(__WXMSW__)
#define USE_BMP_COMBO_BOX
#endif

namespace ui
{

class ClassEditor :
	public wxEvtHandler
{
protected:
	typedef std::map<wxTextCtrl*, std::string> EntryMap;
	EntryMap _entryWidgets;

	typedef std::map<wxControl*, std::string> SpinCtrlMap;
	SpinCtrlMap _spinWidgets;

	wxutil::TreeView* _list;

	// The entity object we're editing
	SREntityPtr _entity;

	// Helper class (owned by StimResponseEditor)
	StimTypes& _stimTypes;

	// TRUE if the GTK callbacks should be disabled
	bool _updatesDisabled;

#ifdef USE_BMP_COMBO_BOX
	// The combo boxes to select the stim/response type
	wxBitmapComboBox* _type;
	wxBitmapComboBox* _addType;
#else
	wxComboBox* _type;
	wxComboBox* _addType;
#endif

	// The dialog hbox to pack the editing pane into
	wxBoxSizer* _overallHBox;

public:
	// Constructor is connecting the list pane
	ClassEditor(wxWindow* mainPanel, StimTypes& stimTypes);

	virtual ~ClassEditor() {}

	/** greebo: Sets the new entity (is called by the subclasses)
	 */
	virtual void setEntity(const SREntityPtr& entity);

	/** greebo: Sets the given <key> of the current entity to <value>
	 */
	virtual void setProperty(const std::string& key, const std::string& value);

	/** greebo: Updates the widgets (must be implemented by the child classes)
	 */
	virtual void update() = 0;

	void reloadStimTypes();

protected:
	// Constructs a new list view as child to the given parent
	void createListView(wxWindow* parent);

	/** 
	 * greebo: Returns the name of the selected stim in the given combo box.
	 * The client data behind that combo box has to be set by the 
	 * StimTypes helper class.
	 */
	virtual std::string getStimTypeIdFromSelector(wxComboBox* comboBox);

	/** greebo: Adds/removes a S/R from the main list
	 */
	virtual void addSR() = 0;
	virtual void removeSR();

	/** greebo: Duplicates the currently selected S/R object
	 */
	void duplicateStimResponse();

	/** greebo: Gets called when a check box is toggled, this should
	 * 			update the contents of possible associated entry fields.
	 */
	virtual void checkBoxToggled(wxCheckBox* toggleButton) = 0;

	/** greebo: Gets called when an entry box changes, this can be
	 * 			overriden by the subclasses, if this is needed
	 */
	virtual void entryChanged(wxTextCtrl* entry);

	/** greebo: Gets called when a spin button changes, this can be
	 * 			overriden by the subclasses, if this is needed
	 */
	virtual void spinButtonChanged(wxSpinCtrl* ctrl);
	virtual void spinButtonChanged(wxSpinCtrlDouble* ctrl);

	/** greebo: Returns the ID of the currently selected stim/response
	 *
	 * @returns: the index (number) of the selected stim or -1 on failure
	 */
	int getIndexFromSelection();

	/** greebo: Selects the given index in the S/R list
	 */
	void selectIndex(int index);

	/** greebo: Gets called when the list selection changes
	 */
	virtual void selectionChanged() = 0;

	/** 
	 * greebo: Opens the context menu. Needs to be implemented by subclasses.
	 */
	virtual void openSRListContextMenu() = 0;

	// Callback for Stim/Response selection changes
	void onSRSelectionChange(wxDataViewEvent& ev);

	// The keypress handler for catching the keys in the treeview
	void onTreeViewKeyPress(wxKeyEvent& ev);

	void onContextMenu(wxDataViewEvent& ev);

	// Gets called if any of the entry widget contents get changed
	void onEntryChanged(wxCommandEvent& ev);
	void onSpinCtrlChanged(wxSpinEvent& ev);
	void onSpinCtrlDoubleChanged(wxSpinDoubleEvent& ev);
	void onCheckboxToggle(wxCommandEvent& ev);

	// Utility function to connect a checkbutton's "toggled" signal to "onCheckBoxToggle"
	void connectCheckButton(wxCheckBox* checkButton);

	// Utility function to connect a spinbutton's "value-changed" event to "onSpinButtonChanged"
	// It also associates the given spin button in the SpinButton map
	void connectSpinButton(wxSpinCtrl* spinCtrl, const std::string& key);
	void connectSpinButton(wxSpinCtrlDouble* spinCtrl, const std::string& key);

	// Utility function to connect a entry's "changed" signal to "onEntryChanged"
	// It also associates the given entry in the EntryMap
	void connectEntry(wxTextCtrl* entry, const std::string& key);

	// Gets called on stim type selection change
	void onStimTypeSelect(wxCommandEvent& ev);
	void onAddTypeSelect(wxCommandEvent& ev);

	void onAddSR(wxCommandEvent& ev);
	void onRemoveSR(wxCommandEvent& ev);

	// Override/disable override menu items
	void onContextMenuEnable(wxCommandEvent& ev);
	void onContextMenuDisable(wxCommandEvent& ev);
	void onContextMenuDuplicate(wxCommandEvent& ev);
};

} // namespace ui
