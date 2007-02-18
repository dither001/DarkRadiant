#ifndef SURFACEINSPECTOR_H_
#define SURFACEINSPECTOR_H_

#include <gtk/gtkwidget.h>
#include <gtk/gtktable.h>
#include <map>
#include "iselection.h"
#include "iregistry.h"
#include "gtkutil/RegistryConnector.h"

namespace ui {

class SurfaceInspector :
	public RegistryKeyObserver,
	public SelectionSystem::Observer
{
	// The actual dialog window
	GtkWidget* _dialog;

	struct ManipulatorRow {
		GtkWidget* hbox;
		GtkWidget* label;
		GtkWidget* value;
		GtkWidget* smaller; 
		GtkWidget* larger;
		GtkWidget* step;
		GtkWidget* steplabel;
	};

	// This are the named manipulator rows (shift, scale, rotation, etc) 
	typedef std::map<std::string, ManipulatorRow> ManipulatorMap;
	ManipulatorMap _manipulators;

	// The "shader" entry field
	GtkWidget* _shaderEntry;
	
	struct FitTextureWidgets {
		GtkObject* widthAdj;
		GtkObject* heightAdj;
		GtkWidget* width;
		GtkWidget* height;
		GtkWidget* button;
		GtkWidget* label;
	} _fitTexture;

	struct FlipTextureWidgets {
		GtkWidget* hbox;
		GtkWidget* flipX;
		GtkWidget* flipY;
		GtkWidget* label;
	} _flipTexture;
	
	struct ApplyTextureWidgets {
		GtkWidget* hbox;
		GtkWidget* label;
		GtkWidget* natural;
		GtkWidget* axial;
	} _applyTex;
	
	GtkWidget* _defaultTexScale;
	GtkWidget* _texLockButton;
	
	// To avoid key changed loopbacks when the registry is updated 
	bool _callbackActive;

	// The TexDef gets emitted if this is set to TRUE.
	// Set this to FALSE to update the widget values without triggering the update 
	bool _widgetsActive;

	// This member takes care of importing/exporting Registry
	// key values from and to widgets
	gtkutil::RegistryConnector _connector;

	// A reference to the SelectionInfo structure (with the counters)
	const SelectionInfo& _selectionInfo;

public:

	// Constructor & Destructor
	SurfaceInspector();
	~SurfaceInspector();

	/** greebo: Contains the static instance of this dialog.
	 * Constructs the instance and calls toggle() when invoked.
	 */
	static void toggleInspector();

	/** greebo: Gets called when the default texscale registry key changes
	 */
	void keyChanged();

	/** greebo: SelectionSystem::Observer implementation. Gets called by
	 * the SelectionSystem upon selection change to allow updating of the
	 * texture properties.
	 */
	void selectionChanged();
	
private:
	/** greebo: Creates a row consisting of label, value entry,
	 * two arrow buttons and a step entry field.
	 * 
	 * @table: the GtkTable the row should be packed into.
	 * @row: the target row number (first table row = 0).
	 * 
	 * @returns: the structure containing the widget pointers. 
	 */
	ManipulatorRow createManipulatorRow(const std::string& label, 
										GtkTable* table, 
										int row,
										bool vertical);

	// Adds all the widgets to the window
	void populateWindow();
	
	// Connect IEvents to the widgets 
	void connectEvents();

	// Shows/hides this dialog
	void toggle();
	
	// Updates the widgets
	void update();
	// Updates the texture shift/scale/rotation fields
	void updateTexDef();
	
	// Saves the connected widget content into the registry
	void saveToRegistry();
	
	// The callback for the delete event (toggles the visibility)
	static gboolean onDelete(GtkWidget* widget, GdkEvent* event, SurfaceInspector* self);
	
	// Gets called when the step entry fields get changed
	static gboolean onStepFocusOut(GtkWidget* widget, GdkEventFocus *event, SurfaceInspector* self);
	
}; // class SurfaceInspector

} // namespace ui

#endif /*SURFACEINSPECTOR_H_*/
