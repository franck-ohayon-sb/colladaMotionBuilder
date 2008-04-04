/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/*
	Based on the 3dsMax COLLADA Tools:
	Copyright (C) 2005-2006 Autodesk Media Entertainment
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _CONTROLLER_EXPORTER_H_
#define _CONTROLLER_EXPORTER_H_

#ifndef _BASE_EXPORTER_H_
#include "BaseExporter.h"
#endif // _BASE_EXPORTER_H_

class FCDSceneNode;
class FCDEntity;
class FCDController;
class FCDSkinController;
class FCDMorphController;
class FCDEntityInstance;
class FCDControllerInstance;
class ColladaSkin;
class ColladaModifier;
class ColladaMorph;
class INode;

typedef fm::pvector<INode> NodeList;
typedef fm::map<Object*, FCDEntity*> ExportedControllerMap;
typedef fm::pvector<ColladaModifier> ColladaModifierStack;

struct ControllerInfo
{
public:
	INode* node;
	Modifier *modifier;
	ControllerInfo(INode* _node, Modifier *_modifier) : node(_node), modifier(_modifier) {}
};

class ControllerExporter : public BaseExporter
{
private:
	ColladaModifierStack modifiers;
	int boneIndex;

public:
	ControllerExporter(DocumentExporter* document);
	virtual ~ControllerExporter();

	FCDEntity* ExportController(INode* node, Object* object);

	void LinkControllers();
	bool LinkController(FCDController* controller, INode* inode, FCDControllerInstance* instance);
	void LinkSkin(FCDSkinController *colladaSkin, FCDControllerInstance* instance, INode* node, ColladaSkin* modifier);
	void LinkMorph(FCDMorphController* colladaMorph, FCDControllerInstance* instance, INode* node, ColladaMorph* modifier);

	void CalculateForcedNodes(INode* node, Object* object, NodeList& forcedNodes);

	bool IsSkinned(Object* obj);

	// method that checks for Skin or Morph modifiers
	bool DoOverrideXRefExport(Object *obj);

	void ResolveModifiers(Object* obj);

	// Done at the very end of the export process: iterate over the known
	// controllers and re-active the skinning UI
	void ReactivateSkins();
};

#endif // _CONTROLLER_EXPORTER_H_