#include "FaceItem.h"

#include "winding.h"

namespace selection {
	namespace textool {

FaceItem::FaceItem(Face& sourceFace) : 
	_sourceFace(sourceFace)
{}

AABB FaceItem::getExtents() {
	AABB returnValue;
	
	Winding& winding = _sourceFace.getWinding();
	
	for (Winding::iterator i = winding.begin(); i != winding.end(); i++) {
		returnValue.includePoint(Vector3(i->texcoord[0], i->texcoord[1], 0));
	}
	
	return returnValue;
}

void FaceItem::render() {
	glEnable(GL_BLEND);
	glBlendColor(0,0,0, 0.2f);
	glBlendFunc(GL_CONSTANT_ALPHA_EXT, GL_ONE_MINUS_CONSTANT_ALPHA_EXT);
	
	glColor3f(1, 1, 1);
	glBegin(GL_QUADS);
	
	Winding& winding = _sourceFace.getWinding();
	
	for (Winding::iterator i = winding.begin(); i != winding.end(); i++) {
		glVertex2f(i->texcoord[0], i->texcoord[1]);
	}
	
	glEnd();
	glDisable(GL_BLEND);
	
	glPointSize(5);
	glBegin(GL_POINTS);
	for (Winding::iterator i = winding.begin(); i != winding.end(); i++) {
		glVertex2f(i->texcoord[0], i->texcoord[1]);
	}
	glEnd();
}

void FaceItem::transform(const Matrix4& matrix) {
	// Cycle through all the children and ask them to render themselves
	for (unsigned int i = 0; i < _children.size(); i++) {
		_children[i]->transform(matrix);
	}
}

void FaceItem::transformSelected(const Matrix4& matrix) {
	// If this object is selected, transform the whole FaceItem and all children
	if (_selected) {
		transform(matrix);
	}
	else {
		// FaceItem is not selected, propagate the call
		for (unsigned int i = 0; i < _children.size(); i++) {
			_children[i]->transformSelected(matrix);
		}
	}
}

bool FaceItem::testSelect(const Rectangle& rectangle) {
	// Cycle through all the children and ask them to render themselves
	for (unsigned int i = 0; i < _children.size(); i++) {
		// Return true on the first selected child
		if (_children[i]->testSelect(rectangle)) {
			return true;
		}
	}
	
	// Nothing selectable, return false
	return false;
}

TexToolItemVec FaceItem::getSelectables(const Rectangle& rectangle) {
	TexToolItemVec returnVector;
	
	for (unsigned int i = 0; i < _children.size(); i++) {
		// Return true on the first selected child
		if (_children[i]->testSelect(rectangle)) {
			returnVector.push_back(_children[i]);
		}
	}
	
	return returnVector;
}

void FaceItem::beginTransformation() {
	_sourceFace.undoSave();
}

	} // namespace TexTool
} // namespace selection
