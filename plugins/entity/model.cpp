/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "model.h"

#include <boost/algorithm/string/replace.hpp>

SingletonModel::SingletonModel() : 
	_resource("")
{
	// Attach this class as ModuleObserver to the Resource
	_resource.attach(*this);
}

SingletonModel::~SingletonModel() {
	_resource.detach(*this);
}

scene::INodePtr SingletonModel::getNode() const {
	// Returns the reference to the "master" model node
	return _node;
}

void SingletonModel::realise() {
	_resource.get()->load();
	_node = _resource.get()->getNode();
	
	if (_node != NULL) {
		// Add the master model node to this Traversable
		TraversableNode::insert(_node);
	}
}

void SingletonModel::unrealise() {
	if (_node != NULL) {
		// Remove the master model node from this Traversable
		TraversableNode::erase(_node);
	}
}

// Update the contained model from the provided keyvalues

void SingletonModel::modelChanged(const std::string& value) {
	// Sanitise the keyvalue - must use forward slashes
	std::string lowercaseValue = boost::algorithm::replace_all_copy(value, "\\", "/");
	
    _resource.detach(*this);
    _resource.setName(lowercaseValue.c_str());
    _resource.attach(*this);
}
