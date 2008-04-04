/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_TEXTURE_LIBRARY_INCLUDED__
#define __DAE_TEXTURE_LIBRARY_INCLUDED__

#include "TranslatorHelpers/CShaderHelper.h"

class DaeDoc;
class FCDImage;
class FCDTexture;
class FCDEffect;
class FCDEffectParameterSurface;

class DaeTextureLibrary
{
private:
	// Since the 2D placements are after the file object
	// (rather than before as in COLLADA), we have to ensure that they are
	// the exact same before re-using a file texture node.
	typedef fm::map<MString, FCDImage*> NameImageMap;
	typedef fm::map<MString, fm::vector<MObject> > ImportedFileMap;

public:
	DaeTextureLibrary(DaeDoc* doc);
	~DaeTextureLibrary();

	// Add a texture/image to this library and return the export Id
	void ExportTexture(FCDTexture* colladaTexture, const MObject& texture, int blendMode, int textureIndex);
	FCDImage* ExportImage(FCDocument* colladaDocument, const MObject& texture);

	// Create/find a texture/image
	bool ImportTexture(DaeTexture& texture, const FCDTexture* sampler);
	MObject ImportImage(const FCDImage* image, const FCDTexture* sampler = NULL);

	// Helpers to export/import placement nodes
	void Add2DPlacement(FCDTexture* colladaTexture, MObject texture);
	void Add3DProjection(FCDTexture* colladaTexture, MObject projection);
	void Import2DPlacement(const FCDTexture* sampler, MObject& placementNode, fm::vector<MObject>& existingFileNodes);
	MObject Import3DProjection(const FCDTexture* sampler, MObject texture);

private:
	DaeDoc* doc;

	// A lookup table of elements we've already processed
	NameImageMap exportedImages;
	ImportedFileMap importedImages;

	void ExportCubeFace(FCDENode* colladaExtraNode, const MObject& cubeEnv, const char* faceName);
	void ExportSphereMap(FCDENode* colladaExtraNode, const MObject& sphereEnv);
};

#endif 
