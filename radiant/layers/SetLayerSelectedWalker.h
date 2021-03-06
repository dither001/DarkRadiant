#pragma once

#include "ientity.h"
#include "ilayer.h"
#include "entitylib.h"

namespace scene
{

class SetLayerSelectedWalker :
	public NodeVisitor
{
	int _layer;
	bool _selected;

public:
	SetLayerSelectedWalker(int layer, bool selected) :
		_layer(layer),
		_selected(selected)
	{}

	// scene::NodeVisitor
	bool pre(const scene::INodePtr& node)
	{
		if (!node->visible())
		{
			return false; // skip hidden nodes
		}

		if (Node_isWorldspawn(node))
		{
			// Skip the worldspawn
			return true;
		}

		const auto& layers = node->getLayers();

		if (layers.find(_layer) != layers.end())
		{
			Node_setSelected(node, _selected);
		}

		return true;
	}
};

} // namespace scene
