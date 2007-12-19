#ifndef MAPRESOURCE_H_
#define MAPRESOURCE_H_

#include "ireference.h"
#include "imodel.h"
#include <set>
#include <boost/utility.hpp>

namespace map {

class MapResource : 
	public Resource,
	public boost::noncopyable
{
	scene::INodePtr m_model;
  
	// Name given during construction
	const std::string m_originalName;
	
	std::string m_path;
	std::string m_name;
  
	// Type of resource (map, lwo etc)
	std::string _type;
	
	typedef std::set<Resource::Observer*> ResourceObserverList;
	ResourceObserverList _observers;
	
	std::time_t m_modified;
	bool _realised;

public:
	// Constructor
	MapResource(const std::string& name);
	
	virtual ~MapResource();

	void setModel(scene::INodePtr model);
	void clearModel();

	void loadCached();

	void loadModel();

	bool load();
  
	/**
	 * Save this resource (only for map resources).
	 * 
	 * @returns
	 * true if the resource was saved, false otherwise.
	 */
	bool save();
	
	void flush();
	
	scene::INodePtr getNode();
	void setNode(scene::INodePtr node);
	
	virtual void addObserver(Observer& observer);
	virtual void removeObserver(Observer& observer);
		
	bool realised();
  
	// Realise this MapResource
	void realise();
	void unrealise();
	
  bool isMap() const;
  void connectMap();
  std::time_t modified() const;
  void mapSave();

  bool isModified() const;
  void refresh();
  
	/// \brief Returns the model loader for the model \p type or 0 if the model \p type has no loader module
	static ModelLoader* getModelLoaderForType(const std::string& type);
	
private:
	scene::INodePtr loadModelNode();
	
	scene::INodePtr loadModelResource();
};
// Resource pointer types
typedef boost::shared_ptr<MapResource> MapResourcePtr;
typedef boost::weak_ptr<MapResource> MapResourceWeakPtr;

} // namespace map

#endif /*MAPRESOURCE_H_*/
