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

#ifndef _COLLADA_OPTIONS_H_
#define _COLLADA_OPTIONS_H_

class ColladaOptions
{
private:
	Interface* maxInterface;

	// options dialog state
	bool normals;			// export normals
	bool triangulate;		// convert Editable Polygons to triangles
	bool xrefs;				// export external references
	bool tangents;			// export tangents and binormals
	bool animations;		// export animations;
	bool sampleAnim;		// export sampled animation
	bool createClip;		// create one animation clip with all the animations of the scene
	bool bakeMatrices;		// export transforms as matrices
	bool relativePaths;		// export relative paths
	bool importUnits;		// import assets in their units
	bool importUpAxis;		// import assets in their up-axis
	TimeValue	animStart;	// start sample TimeValue(frame * GetTicksPerFrame)
	TimeValue	animEnd;	// end sample TimeValue  (frame * GetTicksPerFrame)

	// not-persistent export options
	bool selectionOnly;		// export only selected nodes/objects

public:
	ColladaOptions(Interface* maxInterface);
	~ColladaOptions();

	// Displays the exporter options dialog to allow the user to change the options.
	bool ShowDialog(bool exporter);

	// Access the export options
	bool ExportNormals() const { return normals; }		
	bool ExportEPolyAsTriangles() const { return triangulate; }
	bool ExportXRefs() const { return xrefs; }
	bool ExportTangents() const { return tangents; }
	bool ExportAnimations() const { return animations; }
	bool SampleAnim() const { return sampleAnim; }	
	bool ExportAnimClip() const { return createClip; }	
	bool BakeMatrices() const { return bakeMatrices; }
	bool ExportRelativePaths() const { return relativePaths; }	
	bool ExportSelected() const { return selectionOnly; }
	TimeValue AnimStart() const { return animStart; }		
	TimeValue AnimEnd() const { return animEnd; }

	// Access the import options
	bool ImportUnits() const { return importUnits; }
	bool ImportUpAxis() const { return importUpAxis; }

	// Change the animation start/end for sampling
	void SetAnimBounds(int start, int end) { animStart = start; animEnd = end; }
	void SetExportSelected(bool _selectionOnly) { selectionOnly = _selectionOnly; }

	bool DoSampleMatrices(INode* maxNode);
	bool IncludeObjOffset(INode* node);
private:
	// local methods
	INT_PTR ExportOptionsDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK ExportOptionsDlgProcS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR ImportOptionsDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK ImportOptionsDlgProcS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	// Save/Load the export options
	void SaveOptions();
	void LoadOptions();
};


#endif // _COLLADA_OPTIONS_H_