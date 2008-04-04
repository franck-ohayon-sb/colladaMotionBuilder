/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_COLLADA_EFFECT_RENDERER_H_
#define _COLLADAFX_COLLADA_EFFECT_RENDERER_H_

class IDX8VertexShaderCache : public VertexShaderCache
{
public:
};

/**
A DirectX renderering utility for the ColladaEffect class.
*/
class ColladaEffectRenderer : 
	public IDX9VertexShader
,	public IStdDualVSCallback
{
	LPDIRECT3DDEVICE9 pd3dDevice;
	ID3D9GraphicsWindow * myGWindow;
	DWORD	dwVertexShader;
	bool bRefreshElements;
	LPD3DXEFFECT m_pErrorEffect;

	IStdDualVS *stdDualVS;
	ColladaEffect *mOwner;
	IRenderMeshCache * mpMeshCache;

	bool bTransparency;
	D3DXHANDLE hTechnique;
	TSTR techniqueName;
	bool bReEntrant;
	int ReEntrantCount;

	int m_CurCache;
	UINT m_DirectXVersion;

public:
	ColladaEffectRenderer(ColladaEffect * m);
	~ColladaEffectRenderer();

	ColladaEffect* GetColladaEffect(){return mOwner;}
	
	HRESULT Initialize(Mesh *mesh, INode *node);
	HRESULT Initialize(MNMesh *mnmesh, INode *node);

	// From IVertexShader
	HRESULT ConfirmDevice(ID3D9GraphicsWindow *d3dgw);
	HRESULT ConfirmPixelShader(IDX9PixelShader *pps);
	bool CanTryStrips();
	int GetNumMultiPass();

	LPDIRECT3DVERTEXSHADER9 GetVertexShaderHandle(int /*numPass*/) { return 0; }
	HRESULT SetVertexShader(ID3D9GraphicsWindow *d3dgw, int numPass);

	// Draw 3D mesh as TriStrips
	bool	DrawMeshStrips(ID3D9GraphicsWindow *d3dgw, MeshData *data);

	// Draw 3D mesh as wireframe
	bool	DrawWireMesh(ID3D9GraphicsWindow *d3dgw, WireMeshData *data);

	// Draw 3D lines
	void	StartLines(ID3D9GraphicsWindow *d3dgw, WireMeshData *data);
	void	AddLine(ID3D9GraphicsWindow *d3dgw, DWORD *vert, int vis);
	bool	DrawLines(ID3D9GraphicsWindow *d3dgw);
	void	EndLines(ID3D9GraphicsWindow *d3dgw, GFX_ESCAPE_FN fn);

	// Draw 3D triangles
	void	StartTriangles(ID3D9GraphicsWindow *d3dgw, MeshFaceData *data);
	void	AddTriangle(ID3D9GraphicsWindow *d3dgw, DWORD index, int *edgeVis);
	bool	DrawTriangles(ID3D9GraphicsWindow *d3dgw);
	void	EndTriangles(ID3D9GraphicsWindow *d3dgw, GFX_ESCAPE_FN fn);

	void SetTechnique(D3DXHANDLE handle, TCHAR * techniqueName, bool bDefault);

	// from IStdDualVSCallback
	ReferenceTarget *GetRefTarg();
	VertexShaderCache *CreateVertexShaderCache();
	HRESULT  InitValid(Mesh* mesh, INode *node);
	HRESULT  InitValid(MNMesh* mnmesh, INode *node);
#if (MAX_VERSION_MAJOR >= 9)
	void	 DeleteRenderMeshCache(INode * node);
#endif // MAX_VERSION

	void Draw();
	void DrawError();

	IHLSLCodeGenerator::CodeVersion GetPixelShaderSupport(LPDIRECT3DDEVICE9 pd3dDevice, DWORD & instSlots);

	static void NotificationCallback(void* param, NotifyInfo* info);

	IRenderMeshCache * GetMeshCache(){return mpMeshCache;}
};

#endif // _COLLADAFX_COLLADA_EFFECT_RENDERER_H_
