/*
Copyright (c) 2001, Loki software, inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list 
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

Neither the name of Loki software nor the names of its contributors may be used 
to endorse or promote products derived from this software without specific prior 
written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/

//
// Shaders Manager Plugin
//
// Leonardo Zide (leo@lokigames.com)
//

#include "shaders.h"
#include "MissingXMLNodeException.h"
#include "ShaderTemplate.h"
#include "ShaderFileLoader.h"

#include <map>

#include "ifilesystem.h"
#include "ishaders.h"
#include "itextures.h"
#include "qerplugin.h"
#include "irender.h"
#include "iregistry.h"

#include "parser/ParseException.h"
#include "parser/DefTokeniser.h"
#include "debugging/debugging.h"
#include "generic/callback.h"
#include "generic/referencecounted.h"
#include "shaderlib.h"
#include "texturelib.h"
#include "moduleobservers.h"
#include "archivelib.h"
#include "imagelib.h"

#include "xmlutil/Node.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include "constructors/DefaultConstructor.h"
#include "constructors/FileLoader.h"

/* GLOBALS */

namespace {

	const char* MISSING_BASEPATH_NODE =
	"Failed to find \"/game/filesystem/shaders/basepath\" node \
in game descriptor";
	 
	const char* MISSING_EXTENSION_NODE =
	"Failed to find \"/game/filesystem/shaders/extension\" node \
in game descriptor";
	
}

_QERPlugImageTable* g_bitmapModule = 0;
const char* g_texturePrefix = "textures/";

void ActiveShaders_IteratorBegin();
bool ActiveShaders_IteratorAtEnd();
IShader *ActiveShaders_IteratorCurrent();
void ActiveShaders_IteratorIncrement();
Callback g_ActiveShadersChangedNotify;

void FreeShaders();
qtexture_t *Texture_ForName (const char *filename);


/*!
NOTE TTimo: there is an important distinction between SHADER_NOT_FOUND and SHADER_NOTEX:
SHADER_NOT_FOUND means we didn't find the raw texture or the shader for this
SHADER_NOTEX means we recognize this as a shader script, but we are missing the texture to represent it
this was in the initial design of the shader code since early GtkRadiant alpha, and got sort of foxed in 1.2 and put back in
*/

Image* loadBitmap(void* environment, const char* name)
{
  DirectoryArchiveFile file(name, name);
  if(!file.failed())
  {
    return g_bitmapModule->loadImage(file);
  }
  return 0;
}

inline byte* getPixel(byte* pixels, int width, int height, int x, int y)
{
  return pixels + (((((y + height) % height) * width) + ((x + width) % width)) * 4);
}

class KernelElement
{
public:
  int x, y;
  float w;
};

Image& convertHeightmapToNormalmap(Image& heightmap, float scale)
{
  int w = heightmap.getWidth();
  int h = heightmap.getHeight();
  
  Image& normalmap = *(new RGBAImage(heightmap.getWidth(), heightmap.getHeight()));
  
  byte* in = heightmap.getRGBAPixels();
  byte* out = normalmap.getRGBAPixels();

#if 1
  // no filtering
  const int kernelSize = 2;
  KernelElement kernel_du[kernelSize] = {
    {-1, 0,-0.5f },
    { 1, 0, 0.5f }
  };
  KernelElement kernel_dv[kernelSize] = {
    { 0, 1, 0.5f },
    { 0,-1,-0.5f }
  };
#else
  // 3x3 Prewitt
  const int kernelSize = 6;
  KernelElement kernel_du[kernelSize] = {
    {-1, 1,-1.0f },
    {-1, 0,-1.0f },
    {-1,-1,-1.0f },
    { 1, 1, 1.0f },
    { 1, 0, 1.0f },
    { 1,-1, 1.0f }
  };
  KernelElement kernel_dv[kernelSize] = {
    {-1, 1, 1.0f },
    { 0, 1, 1.0f },
    { 1, 1, 1.0f },
    {-1,-1,-1.0f },
    { 0,-1,-1.0f },
    { 1,-1,-1.0f }
  };
#endif

  int x, y = 0;
  while( y < h )
  {
    x = 0;
    while( x < w )
    {
      float du = 0;
      for(KernelElement* i = kernel_du; i != kernel_du + kernelSize; ++i)
      {
        du += (getPixel(in, w, h, x + (*i).x, y + (*i).y)[0] / 255.0) * (*i).w;
      }
      float dv = 0;
      for(KernelElement* i = kernel_dv; i != kernel_dv + kernelSize; ++i)
      {
        dv += (getPixel(in, w, h, x + (*i).x, y + (*i).y)[0] / 255.0) * (*i).w;
      }

      float nx = -du * scale;
      float ny = -dv * scale;
      float nz = 1.0;

      // Normalize      
      float norm = 1.0/sqrt(nx*nx + ny*ny + nz*nz);
      out[0] = float_to_integer(((nx * norm) + 1) * 127.5);
      out[1] = float_to_integer(((ny * norm) + 1) * 127.5);
      out[2] = float_to_integer(((nz * norm) + 1) * 127.5);
      out[3] = 255;
     
      x++;
      out += 4;
    }
    
    y++;
  }
  
  return normalmap;
}

Image* loadHeightmap(void* environment, const char* name)
{
  Image* heightmap = GlobalTexturesCache().loadImage(name);
  if(heightmap != 0)
  {
    Image& normalmap = convertHeightmapToNormalmap(*heightmap, *reinterpret_cast<float*>(environment));
    heightmap->release();
    return &normalmap;
  }
  return 0;
}

/**
 * Wrapper class that associates a ShaderTemplate with its filename.
 */
struct ShaderDefinition
{
	// The shader template
	ShaderTemplatePtr shaderTemplate;
	
	// Filename from which the shader was parsed
	std::string filename;

	/* Constructor
	 */
	ShaderDefinition(ShaderTemplatePtr templ, const std::string& fname)
    : shaderTemplate(templ),
      filename(fname)
	{ }
	
};

typedef std::map<std::string, ShaderDefinition> ShaderDefinitionMap;

ShaderDefinitionMap g_shaderDefinitions;

///\todo BlendFunc parsing
BlendFunc evaluateBlendFunc(const BlendFuncExpression& blendFunc)
{
  return BlendFunc(BLEND_ONE, BLEND_ZERO);
}

// Map string blend functions to their enum equivalents
BlendFactor evaluateBlendFactor(const std::string& value)
{
  if(value == "gl_zero")
  {
    return BLEND_ZERO;
  }
  if(value == "gl_one")
  {
    return BLEND_ONE;
  }
  if(value == "gl_src_color")
  {
    return BLEND_SRC_COLOUR;
  }
  if(value == "gl_one_minus_src_color")
  {
    return BLEND_ONE_MINUS_SRC_COLOUR;
  }
  if(value == "gl_src_alpha")
  {
    return BLEND_SRC_ALPHA;
  }
  if(value == "gl_one_minus_src_alpha")
  {
    return BLEND_ONE_MINUS_SRC_ALPHA;
  }
  if(value == "gl_dst_color")
  {
    return BLEND_DST_COLOUR;
  }
  if(value == "gl_one_minus_dst_color")
  {
    return BLEND_ONE_MINUS_DST_COLOUR;
  }
  if(value == "gl_dst_alpha")
  {
    return BLEND_DST_ALPHA;
  }
  if(value == "gl_one_minus_dst_alpha")
  {
    return BLEND_ONE_MINUS_DST_ALPHA;
  }
  if(value == "gl_src_alpha_saturate")
  {
    return BLEND_SRC_ALPHA_SATURATE;
  }

  return BLEND_ZERO;
}

class CShader : public IShader
{
	// Internal reference count
	int _nRef;

  const ShaderTemplate& m_template;
  std::string m_filename;
	
	// Name of shader
	std::string _name;

	// Textures for this shader
	qtexture_t* m_pTexture;
	qtexture_t* m_notfound;
	qtexture_t* m_pDiffuse;
	float m_heightmapScale;
	qtexture_t* m_pBump;
	qtexture_t* m_pSpecular;
	qtexture_t* _texLightFalloff;
	BlendFunc m_blendFunc;

  bool m_bInUse;


public:
  static bool m_lightingEnabled;

	/*
	 * Constructor. Sets the name and the ShaderDefinition to use.
	 */
	CShader(const std::string& name, const ShaderDefinition& definition)
	: _nRef(0),
	  m_template(*definition.shaderTemplate),
      m_filename(definition.filename),
	  _name(name),
      m_blendFunc(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA),
      m_bInUse(false) 
	{
		assert(definition.shaderTemplate != NULL); // otherwise we have NULL ref
		
		// Initialise texture pointers
	    m_pTexture = 0;
	    m_pDiffuse = 0;
	    m_pBump = 0;
	    m_pSpecular = 0;
	
	    m_notfound = 0;

		// Realise the shader
	    realise();
	}
	
	virtual ~CShader() {
		unrealise();
	}

	// Increase reference count
	void IncRef() {
		++_nRef;
	}
	
	// Decrease reference count
	void DecRef()  {
		if(--_nRef == 0) {
			delete this;
		}
	}

  std::size_t refcount()
  {
    return _nRef;
  }

  // get/set the qtexture_t* Radiant uses to represent this shader object
  qtexture_t* getTexture() const
  {
    return m_pTexture;
  }
  qtexture_t* getDiffuse() const
  {
    return m_pDiffuse;
  }

	// Return bumpmap if it exists, otherwise _flat
  qtexture_t* getBump() const
  {
    	return m_pBump;
  }
  qtexture_t* getSpecular() const
  {
    return m_pSpecular;
  }
  
	/*
	 * Return name of shader.
	 */
	const char* getName() const {
		return _name.c_str();
	}
	
  bool IsInUse() const
  {
    return m_bInUse;
  }
  void SetInUse(bool bInUse)
  {
    m_bInUse = bInUse;
    g_ActiveShadersChangedNotify();
  }
  // get the shader flags
  int getFlags() const
  {
    return m_template.m_nFlags;
  }
  // get the transparency value
  float getTrans() const
  {
    return m_template.m_fTrans;
  }
  // test if it's a true shader, or a default shader created to wrap around a texture
  bool IsDefault() const 
  {
    return m_filename.empty();
  }
  // get the alphaFunc
  void getAlphaFunc(EAlphaFunc *func, float *ref) { *func = m_template.m_AlphaFunc; *ref = m_template.m_AlphaRef; };
  BlendFunc getBlendFunc() const
  {
    return m_blendFunc;
  }
  // get the cull type
  ECull getCull()
  {
    return m_template.m_Cull;
  };
  // get shader file name (ie the file where this one is defined)
  const char* getShaderFileName() const
  {
    return m_filename.c_str();
  }
  // -----------------------------------------

	void realise() {
		
		std::cout << "CShader::realise called\n";
		
		// Grab the display texture (may be null)
		std::string displayTex = m_template._texture->getTextureName();
		
		// Allocate a default ImageConstructor with this name
		ImageConstructorPtr imageConstructor(new DefaultConstructor(displayTex));
		
		m_pTexture = GlobalTexturesCache().capture(imageConstructor, displayTex);

		// Has the texture been successfully realised? 
    	if (m_pTexture->texture_number == 0) {
    		std::cout << "CShader::realise failed, falling back to shadernotex\n";
    		
    		// No, it has not
			m_notfound = m_pTexture;
			
			std::string name = std::string(GlobalRadiant().getAppPath())
			                   + "bitmaps/"
			                   + (IsDefault() ? "notex.bmp" : "shadernotex.bmp");
			
			// Construct a new BMP loader
			ImageConstructorPtr bmpConstructor(new FileLoader(name, "bmp"));
			m_pTexture = GlobalTexturesCache().capture(bmpConstructor, name);
		}

		realiseLighting();
	}

  void unrealise()
  {
    GlobalTexturesCache().release(m_pTexture);

    if(m_notfound != 0)
    {
      GlobalTexturesCache().release(m_notfound);
    }

    unrealiseLighting();
  }

	// Parse and load image maps for this shader
	void realiseLighting() {
		
		// Create a shortcut reference
		TexturesCache& cache = GlobalTexturesCache();
		
		// Set up the diffuse, bump and specular stages. Bump and specular will 
		// be set to defaults _flat and _black respectively, if an image map is 
		// not specified in the material.
		
		// Load the diffuse map
		ImageConstructorPtr diffuseConstructor(new DefaultConstructor(m_template._diffuse->getTextureName()));
		m_pDiffuse = cache.capture(diffuseConstructor, m_template._diffuse->getTextureName());

		// Load the bump map
    	ImageConstructorPtr bumpConstructor(new DefaultConstructor(m_template._bump->getTextureName()));
		m_pBump = cache.capture(bumpConstructor, m_template._bump->getTextureName());
		
		if (m_pBump == 0 || m_pBump->texture_number == 0) {
			// Bump Map load failed
			cache.release(m_pBump); // release old object first
			
			// Flat image name
			std::string flatName = std::string(GlobalRadiant().getAppPath()) 
    									   + "bitmaps/_flat.bmp";
			
			// Construct a new BMP loader
			ImageConstructorPtr bmpConstructor(new FileLoader(flatName, "bmp"));
			m_pBump = cache.capture(bmpConstructor, flatName);
		}
		
		// Load the specular map
    	ImageConstructorPtr specConstructor(new DefaultConstructor(m_template._specular->getTextureName()));
		m_pSpecular = cache.capture(specConstructor, m_template._specular->getTextureName());
		
		if (m_pSpecular == 0 || m_pSpecular->texture_number == 0) {
			cache.release(m_pSpecular);
			
			// Default specular (black image)
			std::string blackName = std::string(GlobalRadiant().getAppPath()) + "bitmaps/_black.bmp";
			
			// Construct a new BMP loader
			ImageConstructorPtr bmpConstructor(new FileLoader(blackName, "bmp"));
			m_pSpecular = cache.capture(bmpConstructor, blackName);
		}
		
		// Get light falloff image. If a falloff image is defined but invalid,
		// emit a warning since this will result in a black light
		std::string foTexName = m_template._lightFallOff->getTextureName();
		
		// Allocate a default ImageConstructor with this name
		ImageConstructorPtr imageConstructor(new DefaultConstructor(foTexName));
		
		_texLightFalloff = cache.capture(imageConstructor, foTexName);
		if (!foTexName.empty() && _texLightFalloff->texture_number == 0) {
			std::cerr << "[shaders] " << _name 
					  << " : defines invalid lightfalloff \"" << foTexName
					  << "\"" << std::endl;
		}

		for(ShaderTemplate::Layers::const_iterator i = m_template.m_layers.begin(); 
			i != m_template.m_layers.end(); 
			++i)
		{
        	m_layers.push_back(evaluateLayer(*i));
		}

      if(m_layers.size() == 1)
      {
        const BlendFuncExpression& blendFunc = 
        	m_template.m_layers.front().m_blendFunc;
        
		// If explicit blend function (2-components), evaluate it, otherwise
		// use a standard one
        if(!blendFunc.second.empty()) {
			m_blendFunc = BlendFunc(evaluateBlendFactor(blendFunc.first),
									evaluateBlendFactor(blendFunc.second));
        }
        else {
			if(blendFunc.first == "add") {
				m_blendFunc = BlendFunc(BLEND_ONE, BLEND_ONE);
			}
			else if(blendFunc.first == "filter") {
				m_blendFunc = BlendFunc(BLEND_DST_COLOUR, BLEND_ZERO);
			}
			else if(blendFunc.first == "blend") {
				m_blendFunc = BlendFunc(BLEND_SRC_ALPHA, 
										BLEND_ONE_MINUS_SRC_ALPHA);
			}
        }
      }
  }

  void unrealiseLighting()
  {
      GlobalTexturesCache().release(m_pDiffuse);
      GlobalTexturesCache().release(m_pBump);
      GlobalTexturesCache().release(m_pSpecular);

      GlobalTexturesCache().release(_texLightFalloff);

      for(MapLayers::iterator i = m_layers.begin(); i != m_layers.end(); ++i)
      {
        GlobalTexturesCache().release((*i).texture());
      }
      m_layers.clear();

      m_blendFunc = BlendFunc(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA);
  }

	/*
	 * Set name of shader.
	 */
	void setName(const std::string& name) {
		_name = name;
	}

  class MapLayer : public ShaderLayer
  {
    qtexture_t* m_texture;
    BlendFunc m_blendFunc;
    bool m_clampToBorder;
    float m_alphaTest;
  public:
    MapLayer(qtexture_t* texture, BlendFunc blendFunc, bool clampToBorder, float alphaTest) :
      m_texture(texture),
      m_blendFunc(blendFunc),
      m_clampToBorder(false),
      m_alphaTest(alphaTest)
    {
    }
    qtexture_t* texture() const
    {
      return m_texture;
    }
    BlendFunc blendFunc() const
    {
      return m_blendFunc;
    }
    bool clampToBorder() const
    {
      return m_clampToBorder;
    }
    float alphaTest() const
    {
      return m_alphaTest;
    }
  };

	static MapLayer evaluateLayer(const LayerTemplate& layerTemplate) 
	{
		// Allocate a default ImageConstructor with this name
		ImageConstructorPtr imageConstructor(
			new DefaultConstructor(layerTemplate.mapExpr->getTextureName())
		);
		
    	return MapLayer(
      		GlobalTexturesCache().
      					capture(imageConstructor, layerTemplate.mapExpr->getTextureName()),
      		evaluateBlendFunc(layerTemplate.m_blendFunc),
      		layerTemplate.m_clampToBorder,
      		boost::lexical_cast<float>(layerTemplate.m_alphaTest)
    	);
  	}

  typedef std::vector<MapLayer> MapLayers;
  MapLayers m_layers;

  const ShaderLayer* firstLayer() const
  {
    if(m_layers.empty())
    {
      return 0;
    }
    return &m_layers.front();
  }
  void forEachLayer(const ShaderLayerCallback& callback) const
  {
    for(MapLayers::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i)
    {
      callback(*i);
    }
  }
  
	/* Required IShader light type predicates */
	
	bool isAmbientLight() const {
		return m_template.ambientLight;
	}

	bool isBlendLight() const {
		return m_template.blendLight;
	}

	bool isFogLight() const {
		return m_template.fogLight;
	}

	/*
	 * Return the light falloff texture (Z dimension).
	 */
	qtexture_t* lightFalloffImage() const {
		if (m_template._lightFallOff)
			return _texLightFalloff;
		else 
			return 0;
	}
};

#ifdef _DEBUG

#endif

bool CShader::m_lightingEnabled = false;

typedef SmartPointer<CShader> ShaderPointer;
typedef std::map<std::string, ShaderPointer> shaders_t;

shaders_t g_ActiveShaders;

static shaders_t::iterator g_ActiveShadersIterator;

void ActiveShaders_IteratorBegin()
{
  g_ActiveShadersIterator = g_ActiveShaders.begin();
}

bool ActiveShaders_IteratorAtEnd()
{
  return g_ActiveShadersIterator == g_ActiveShaders.end();
}

IShader *ActiveShaders_IteratorCurrent()
{
  return static_cast<CShader*>(g_ActiveShadersIterator->second);
}

void ActiveShaders_IteratorIncrement()
{
  ++g_ActiveShadersIterator;
}

// will free all GL binded qtextures and shaders
// NOTE: doesn't make much sense out of Radiant exit or called during a reload
void FreeShaders()
{
	// Warn if any shaders have refcounts > 1
	for (shaders_t::iterator i = g_ActiveShaders.begin(); 
		 i != g_ActiveShaders.end(); 
		 ++i) 
	{
		if (i->second->refcount() > 1) {
  			std::cerr << "Warning: shader \"" << i->first 
  					  << "\" still referenced." << std::endl;
		}
	}

  // reload shaders
  // empty the actives shaders list
  g_ActiveShaders.clear();
  g_shaderDefinitions.clear();
  g_ActiveShadersChangedNotify();
}

/**
 * Lookup a named shader and return its CShader object.
 */
CShader* Try_Shader_ForName(const std::string& name)
{
  {
    shaders_t::iterator i = g_ActiveShaders.find(name);
    if(i != g_ActiveShaders.end())
    {
      return (*i).second;
    }
  }

	// Search for a matching ShaderDefinition. If none is found, create a 
	// default one and return this instead (this is how unrecognised textures
	// get rendered with notex.bmp).
	ShaderDefinitionMap::iterator i = g_shaderDefinitions.find(name);
	if(i == g_shaderDefinitions.end()) {
		ShaderTemplatePtr shaderTemplate(new ShaderTemplate(name));

		// Create and insert new ShaderDefinition wrapper
		ShaderDefinition def(shaderTemplate, "");
		i = g_shaderDefinitions.insert(
							ShaderDefinitionMap::value_type(name, def)).first;
	}

	ShaderPointer pShader(new CShader(name, i->second));
	g_ActiveShaders.insert(shaders_t::value_type(name, pShader));
	g_ActiveShadersChangedNotify();
	return pShader;
}

/**
 * Parses the contents of a material definition. The shader name and opening
 * brace "{" will already have been removed when this function is called.
 * 
 * @param tokeniser
 * DefTokeniser to retrieve tokens from.
 * 
 * @param shaderTemplate
 * An empty ShaderTemplate which will parse the token stream and populate
 * itself.
 * 
 * @param filename
 * The name of the shader file we are parsing.
 */
void parseShaderDecl(parser::DefTokeniser& tokeniser, 
					  ShaderTemplatePtr shaderTemplate, 
					  const std::string& filename) 
{
	// Get the ShaderTemplate to populate itself by parsing tokens from the
	// DefTokeniser. This may throw exceptions.	
	shaderTemplate->parseDoom3(tokeniser);
	
	// Construct the ShaderDefinition wrapper class
	ShaderDefinition def(shaderTemplate, filename);
	
	// Get the parsed shader name
	std::string name = shaderTemplate->getName();
	
	// Insert into the definitions map, if not already present
    if (!g_shaderDefinitions.insert(
    				ShaderDefinitionMap::value_type(name, def)).second) 
	{
    	std::cout << "[shaders] " << filename << ": shader " << name
    			  << " already defined." << std::endl;
    }
}

/** Load the shaders from the MTR files.
 */
void Shaders_Load()
{
	// Get the shaders path and extension from the XML game file
	xml::NodeList nlShaderPath = 
		GlobalRegistry().findXPath("game/filesystem/shaders/basepath");
	if (nlShaderPath.size() != 1)
		throw shaders::MissingXMLNodeException(MISSING_BASEPATH_NODE);

	xml::NodeList nlShaderExt = 
		GlobalRegistry().findXPath("game/filesystem/shaders/extension");
	if (nlShaderExt.size() != 1)
		throw shaders::MissingXMLNodeException(MISSING_EXTENSION_NODE);

	// Load the shader files from the VFS
	std::string sPath = nlShaderPath[0].getContent();
	if (!boost::algorithm::ends_with(sPath, "/"))
		sPath += "/";
		
	std::string extension = nlShaderExt[0].getContent();
	
	// Load each file from the global filesystem
	shaders::ShaderFileLoader ldr(sPath);
	GlobalFileSystem().forEachFile(sPath.c_str(), 
								   extension.c_str(), 
								   makeCallback1(ldr), 
								   0);
}

void Shaders_Free()
{
	FreeShaders();
}

ModuleObservers g_observers;

std::size_t g_shaders_unrealised = 1; // wait until filesystem and is realised before loading anything
bool Shaders_realised()
{
  return g_shaders_unrealised == 0;
}
void Shaders_Realise()
{
  if(--g_shaders_unrealised == 0)
  {
    Shaders_Load();
    g_observers.realise();
  }
}
void Shaders_Unrealise()
{
  if(++g_shaders_unrealised == 1)
  {
    g_observers.unrealise();
    Shaders_Free();
  }
}

void Shaders_Refresh() 
{
  Shaders_Unrealise();
  Shaders_Realise();
}

class Quake3ShaderSystem : public ShaderSystem, public ModuleObserver
{
public:
  void realise()
  {
    Shaders_Realise();
  }
  void unrealise()
  {
    Shaders_Unrealise();
  }
  void refresh()
  {
    Shaders_Refresh();
  }

	// Is the shader system realised
	bool isRealised() {
		return g_shaders_unrealised == 0;
	}

	// Return a shader by name
	IShader* getShaderForName(const std::string& name) {
		IShader *pShader = Try_Shader_ForName(name.c_str());
		pShader->IncRef();
		return pShader;
	}

  void foreachShaderName(const ShaderNameCallback& callback)
  {
    for(ShaderDefinitionMap::const_iterator i = g_shaderDefinitions.begin(); i != g_shaderDefinitions.end(); ++i)
    {
      callback((*i).first.c_str());
    }
  }

  void beginActiveShadersIterator()
  {
    ActiveShaders_IteratorBegin();
  }
  bool endActiveShadersIterator()
  {
    return ActiveShaders_IteratorAtEnd();
  }
  IShader* dereferenceActiveShadersIterator()
  {
    return ActiveShaders_IteratorCurrent();
  }
  void incrementActiveShadersIterator()
  {
    ActiveShaders_IteratorIncrement();
  }
  void setActiveShadersChangedNotify(const Callback& notify)
  {
    g_ActiveShadersChangedNotify = notify;
  }

  void attach(ModuleObserver& observer)
  {
    g_observers.attach(observer);
  }
  void detach(ModuleObserver& observer)
  {
    g_observers.detach(observer);
  }

  void setLightingEnabled(bool enabled)
  {
    if(CShader::m_lightingEnabled != enabled)
    {
      for(shaders_t::const_iterator i = g_ActiveShaders.begin(); i != g_ActiveShaders.end(); ++i)
      {
        (*i).second->unrealiseLighting();
      }
      CShader::m_lightingEnabled = enabled;
      for(shaders_t::const_iterator i = g_ActiveShaders.begin(); i != g_ActiveShaders.end(); ++i)
      {
        (*i).second->realiseLighting();
      }
    }
  }

  const char* getTexturePrefix() const
  {
    return g_texturePrefix;
  }
};

Quake3ShaderSystem g_Quake3ShaderSystem;

ShaderSystem& GetShaderSystem()
{
  return g_Quake3ShaderSystem;
}

void Shaders_Construct()
{
  GlobalFileSystem().attach(g_Quake3ShaderSystem);
}
void Shaders_Destroy()
{
  GlobalFileSystem().detach(g_Quake3ShaderSystem);

  if(Shaders_realised())
  {
    Shaders_Free();
  }
}
