#include "PortableMapWriter.h"

#include "igame.h"
#include "ientity.h"

#include "string/string.h"

namespace map
{

namespace
{

	// Checks for NaN and infinity
	inline std::string getSafeDouble(double d)
	{
		if (isValid(d))
		{
			if (d == -0.0)
			{
				return "0"; // convert -0 to 0
			}
			else
			{
				return string::to_string(d);
			}
		}
		else
		{
			// Is infinity or NaN, write 0
			return "0";
		}
	}

}

PortableMapWriter::PortableMapWriter() :
	_entityCount(0),
	_primitiveCount(0),
	_document(xml::Document::create()),
	_map(_document.addTopLevelNode("map")),
	_curEntityPrimitives(nullptr)
{}

void PortableMapWriter::beginWriteMap(std::ostream& stream)
{}

void PortableMapWriter::endWriteMap(std::ostream& stream)
{
	stream << _document.saveToString();
}

void PortableMapWriter::beginWriteEntity(const IEntityNodePtr& entity, std::ostream& stream)
{
	auto node = _map.createChild("entity");
	node.setAttributeValue("number", string::to_string(_entityCount++));

	auto primitiveNode = node.createChild("primitives");
	_curEntityPrimitives = xml::Node(primitiveNode.getNodePtr());

	auto keyValues = node.createChild("keyValues");

	// Export the entity key values
	entity->getEntity().forEachKeyValue([&](const std::string& key, const std::string& value)
	{
		auto kv = keyValues.createChild("keyValue");
		kv.setAttributeValue("key", key);
		kv.setAttributeValue("value", value);
	});
}

void PortableMapWriter::endWriteEntity(const IEntityNodePtr& entity, std::ostream& stream)
{
	// Reset the primitive count again
	_primitiveCount = 0;

	_curEntityPrimitives = xml::Node(nullptr);
}

void PortableMapWriter::beginWriteBrush(const IBrushNodePtr& brushNode, std::ostream& stream)
{
	assert(_curEntityPrimitives.getNodePtr() != nullptr);

	auto brushTag = _curEntityPrimitives.createChild("brush");
	brushTag.setAttributeValue("number", string::to_string(_primitiveCount++));

	const auto& brush = brushNode->getIBrush();

	// Iterate over each brush face, exporting the tags for each
	for (std::size_t i = 0; i < brush.getNumFaces(); ++i)
	{
		const auto& face = brush.getFace(i);

		// greebo: Don't export faces with degenerate or empty windings (they are "non-contributing")
		if (face.getWinding().size() <= 2)
		{
			return;
		}

		auto faceTag = brushTag.createChild("face");

		// Write the plane equation
		const Plane3& plane = face.getPlane3();

		auto planeTag = faceTag.createChild("plane");
		planeTag.setAttributeValue("x", getSafeDouble(plane.normal().x()));
		planeTag.setAttributeValue("y", getSafeDouble(plane.normal().y()));
		planeTag.setAttributeValue("z", getSafeDouble(plane.normal().z()));
		planeTag.setAttributeValue("d", getSafeDouble(-plane.dist()));

		// Write TexDef
		Matrix4 texdef = face.getTexDefMatrix();

		auto texTag = faceTag.createChild("textureProjection");
		texTag.setAttributeValue("xx", getSafeDouble(texdef.xx()));
		texTag.setAttributeValue("yx", getSafeDouble(texdef.yx()));
		texTag.setAttributeValue("tx", getSafeDouble(texdef.tx()));
		texTag.setAttributeValue("xy", getSafeDouble(texdef.xy()));
		texTag.setAttributeValue("yy", getSafeDouble(texdef.yy()));
		texTag.setAttributeValue("ty", getSafeDouble(texdef.ty()));

		// Write Shader
		auto shaderTag = faceTag.createChild("material");
		shaderTag.setAttributeValue("name", face.getShader());
		
		// Export (dummy) contents/flags
		auto detailTag = faceTag.createChild("contentFlag");
		detailTag.setAttributeValue("value", string::to_string(brush.getDetailFlag()));
	}
}

void PortableMapWriter::endWriteBrush(const IBrushNodePtr& brush, std::ostream& stream)
{
	// nothing
}

void PortableMapWriter::beginWritePatch(const IPatchNodePtr& patch, std::ostream& stream)
{
	assert(_curEntityPrimitives.getNodePtr() != nullptr);

	auto node = _curEntityPrimitives.createChild("patch");
	node.setAttributeValue("number", string::to_string(_primitiveCount++));
}

void PortableMapWriter::endWritePatch(const IPatchNodePtr& patch, std::ostream& stream)
{
	// nothing
}

} // namespace
