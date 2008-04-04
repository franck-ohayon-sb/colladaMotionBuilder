/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

#include "ColladaEffect.h"
#include "ColladaEffectPass.h"
#include "ColladaEffectRenderer.h"

static LPDIRECT3DDEVICE9 GetDevice()
{
	GraphicsWindow		*GW;
	ViewExp				*View;
	LPDIRECT3DDEVICE9	Device;

	View = GetCOREInterface()->GetActiveViewport();

	if (View)
	{
		GW = View->getGW();

		if (GW)
		{
			ID3D9GraphicsWindow *D3DGW = (ID3D9GraphicsWindow *)GW->GetInterface(D3D9_GRAPHICS_WINDOW_INTERFACE_ID);

			if (D3DGW)
			{
				Device = D3DGW->GetDevice();

				return(Device);
			}
		}
	}
	return NULL;
}

ColladaEffectRenderer::ColladaEffectRenderer(ColladaEffect * m)
:	mOwner(m)
{
	stdDualVS = (IStdDualVS*) CreateInstance(REF_MAKER_CLASS_ID, STD_DUAL_VERTEX_SHADER);
	if (stdDualVS)
	{
		stdDualVS->SetCallback((IStdDualVSCallback*)this);
	}

	pd3dDevice = NULL;
	//pEffect = NULL;
	m_pErrorEffect = NULL;
	//pEffectParser = NULL;
	bTransparency = false;
	//bPreDeleteLight = false;
	mpMeshCache = IRenderMeshCache::GetRenderMeshCache();

#if (MAX_VERSION_MAJOR >= 9)
	mpMeshCache->SetMeshType(IRenderMesh::kMesh);
#else
	mpMeshCache->SetSizeOfCache(100,IRenderMesh::kMesh);
#endif // MAX_VERSION

    m_CurCache		= 0;
	//sceneLights.SetCount(0);
	bRefreshElements = false;
	bReEntrant = false;
	//ReEntrantCount = RENTRANT_COUNT;

	//UpdateSceneLights();
	RegisterNotification(NotificationCallback,this,NOTIFY_SCENE_ADDED_NODE);
	RegisterNotification(NotificationCallback,this,NOTIFY_SCENE_POST_DELETED_NODE);
	RegisterNotification(NotificationCallback,this,NOTIFY_SCENE_PRE_DELETED_NODE);
	RegisterNotification(NotificationCallback,this,NOTIFY_SCENE_UNDO);
	RegisterNotification(NotificationCallback,this,NOTIFY_SCENE_REDO);
	RegisterNotification(NotificationCallback,this,NOTIFY_FILE_POST_OPEN);
#if (MAX_VERSION_MAJOR >= 8)
	RegisterNotification(NotificationCallback,this,NOTIFY_SCENE_PRE_UNDO);
	RegisterNotification(NotificationCallback,this,NOTIFY_SCENE_PRE_REDO);
	RegisterNotification(NotificationCallback, this, NOTIFY_D3D_PRE_DEVICE_RESET);
	RegisterNotification(NotificationCallback, this, NOTIFY_D3D_POST_DEVICE_RESET);
#endif // MAX_VERSION
	// set Cg's D3D device, but no not broadcast to ColladaEffect's passes.
	
	cgD3D9SetDevice(GetDevice());
}

ColladaEffectRenderer::~ColladaEffectRenderer()
{
	if (stdDualVS)
		stdDualVS->DeleteThis();

	//need to the get the macros in here.
	SAFE_DELETE(mpMeshCache);
	SAFE_RELEASE(m_pErrorEffect);

	//if (pEffectParser)
	//{
	//	pEffectParser->DestroyParser();
	//	pEffectParser = NULL;
	//}



	//for (int f =0; f<sceneLights.Count();f++)
	//{
	//	delete sceneLights[f];
	//}


	UnRegisterNotification(NotificationCallback,this,NOTIFY_SCENE_ADDED_NODE);
	UnRegisterNotification(NotificationCallback,this,NOTIFY_SCENE_POST_DELETED_NODE);
	UnRegisterNotification(NotificationCallback,this,NOTIFY_SCENE_PRE_DELETED_NODE);
	UnRegisterNotification(NotificationCallback,this,NOTIFY_SCENE_UNDO);
	UnRegisterNotification(NotificationCallback,this,NOTIFY_SCENE_REDO);
	UnRegisterNotification(NotificationCallback,this,NOTIFY_FILE_POST_OPEN);
#if (MAX_VERSION_MAJOR >= 8)
	UnRegisterNotification(NotificationCallback,this,NOTIFY_SCENE_PRE_UNDO);
	UnRegisterNotification(NotificationCallback,this,NOTIFY_SCENE_PRE_REDO);
	UnRegisterNotification(NotificationCallback, this, NOTIFY_D3D_PRE_DEVICE_RESET);
	UnRegisterNotification(NotificationCallback, this, NOTIFY_D3D_POST_DEVICE_RESET);
#endif // MAX_VERSION
}

IHLSLCodeGenerator::CodeVersion ColladaEffectRenderer::GetPixelShaderSupport(LPDIRECT3DDEVICE9 pd3dDevice, DWORD & instSlots)
{
	D3DCAPS9	Caps;
	IHLSLCodeGenerator::CodeVersion code;
	pd3dDevice->GetDeviceCaps(&Caps);
	UINT major = D3DSHADER_VERSION_MAJOR(Caps.PixelShaderVersion);
#if (MAX_VERSION_MAJOR >= 8)
	UINT minor = D3DSHADER_VERSION_MINOR(Caps.PixelShaderVersion);
#endif // MAX_VERSION

	instSlots = 96;

	if (major < 2)
		code = IHLSLCodeGenerator::PS_1_X;
	else if (major == 2)
	{
		instSlots = Caps.PS20Caps.NumInstructionSlots;
#if (MAX_VERSION_MAJOR >= 8)
		if (minor > 0)
			code = IHLSLCodeGenerator::PS_2_X;
		else
#endif // MAX_VERSION
			code = IHLSLCodeGenerator::PS_2_0;
	}
	else if (major >=3)
	{
		instSlots = Caps.MaxPixelShader30InstructionSlots;
		code = IHLSLCodeGenerator::PS_3_0;
	}
	else
	{
		code = IHLSLCodeGenerator::PS_1_X;
	}

	return code;

}

HRESULT ColladaEffectRenderer::Initialize(Mesh *mesh, INode *node)
{

#if (MAX_VERSION_MAJOR >= 9)
	m_CurCache = mpMeshCache->SetCachedMesh(NULL,node,GetCOREInterface()->GetTime(),bRefreshElements);
#else
	m_CurCache = mpMeshCache->SetCachedMesh(mesh,node,GetCOREInterface()->GetTime(),bRefreshElements);
#endif // MAX_VERSION

	if (stdDualVS)
	{
		stdDualVS->Initialize(mesh, node);
	}
	Draw();

	return S_OK;
}

HRESULT ColladaEffectRenderer::Initialize(MNMesh *mnmesh, INode *node)
{

#if (MAX_VERSION_MAJOR >= 9)
	m_CurCache = mpMeshCache->SetCachedMNMesh(NULL,node,GetCOREInterface()->GetTime(),bRefreshElements);
#else
	Mesh mesh;
	mnmesh->OutToTri(mesh);
	m_CurCache = mpMeshCache->SetCachedMesh(&mesh,node,GetCOREInterface()->GetTime(),bRefreshElements);
#endif // MAX_VERSION

	if (stdDualVS)
	{
		stdDualVS->Initialize(mnmesh, node);
	}
	Draw();
	return S_OK;
}

HRESULT ColladaEffectRenderer::ConfirmDevice(ID3D9GraphicsWindow *d3dgw)
{

	myGWindow = d3dgw;
	return S_OK;
}

HRESULT ColladaEffectRenderer::ConfirmPixelShader(IDX9PixelShader* /*pps*/)
{
	return S_OK;
}

bool ColladaEffectRenderer::CanTryStrips()
{
	return true;
}

int ColladaEffectRenderer::GetNumMultiPass()
{
	return 1;
}

HRESULT ColladaEffectRenderer::SetVertexShader(ID3D9GraphicsWindow* /*d3dgw*/, int /*numPass*/)
{
	return S_OK;
}


bool ColladaEffectRenderer::DrawWireMesh(ID3D9GraphicsWindow* /*d3dgw*/, WireMeshData* /*data*/)
{
	return false;
}

void ColladaEffectRenderer::StartLines(ID3D9GraphicsWindow* /*d3dgw*/, WireMeshData* /*data*/)
{
}

void ColladaEffectRenderer::AddLine(ID3D9GraphicsWindow* /*d3dgw*/, DWORD* /*vert*/, int /*vis*/)
{
}

bool ColladaEffectRenderer::DrawLines(ID3D9GraphicsWindow* /*d3dgw*/)
{
	return false;
}

void ColladaEffectRenderer::EndLines(ID3D9GraphicsWindow* /*d3dgw*/, GFX_ESCAPE_FN /*fn*/)
{
}

void ColladaEffectRenderer::StartTriangles(ID3D9GraphicsWindow* /*d3dgw*/, MeshFaceData* /*data*/)
{
}

void ColladaEffectRenderer::AddTriangle(ID3D9GraphicsWindow* /*d3dgw*/, DWORD /*index*/, int* /*edgeVis*/)
{
}

bool ColladaEffectRenderer::DrawTriangles(ID3D9GraphicsWindow* /*d3dgw*/)
{
	return false;
}

void ColladaEffectRenderer::EndTriangles(ID3D9GraphicsWindow* /*d3dgw*/, GFX_ESCAPE_FN /*fn*/)
{
}

ReferenceTarget *ColladaEffectRenderer::GetRefTarg()
{
	return mOwner;
}

VertexShaderCache *ColladaEffectRenderer::CreateVertexShaderCache()
{
	return new IDX8VertexShaderCache;
}

HRESULT ColladaEffectRenderer::InitValid(Mesh *mesh, INode *node)
{
#if (MAX_VERSION_MAJOR >= 9)
	m_CurCache = mpMeshCache->SetCachedMesh(mesh,node,GetCOREInterface()->GetTime(),bRefreshElements);
#else
	(void)mesh;
	(void)node;
#endif // MAX_VERSION
	mpMeshCache->GetActiveRenderMesh(m_CurCache)->Invalidate();
	return(S_OK);
}

HRESULT ColladaEffectRenderer::InitValid(MNMesh *mnmesh, INode *node)
{
#if (MAX_VERSION_MAJOR >= 9)
	m_CurCache = mpMeshCache->SetCachedMNMesh(mnmesh,node,GetCOREInterface()->GetTime(),bRefreshElements);
#else
	(void)mnmesh;
	(void)node;
#endif // MAX_VERSION
	mpMeshCache->GetActiveRenderMesh(m_CurCache)->Invalidate();
    return S_OK;
}

#if (MAX_VERSION_MAJOR >= 9)
void ColladaEffectRenderer::DeleteRenderMeshCache(INode * node)
{
	mpMeshCache->DeleteRenderMeshCache(node);
}
#endif // MAX_VERSION

// Use the following define if you experience problem with Cg and Max
// Simply change the hard-coded shader in the Draw() method below.
//#define DEBUG_CG

void ColladaEffectRenderer::Draw()
{
	if (!pd3dDevice)
	{
		pd3dDevice = GetDevice();
		if (!pd3dDevice)
			return;
	}

	// FIRST PASS = 0x42 (DIFFUSE | XYZ)
	// SECOND PASS = 0x12 (NORMAL | XYZ)
	DWORD fvf = 0;
	pd3dDevice->GetFVF(&fvf);
	(void)fvf;

	// Max does rendering in 2 passes:
	//	pass 1: flat shade mode with no lights and z write enabled -> to fill the z buffer for cheap
	//	pass 2: gouraud shade mode with lighting (smooth and highlights) -> render color
	// We don't need that (in fact, there's a conflict with Cg), so just discard the first pass and
	// enable z writing in the second one.
	// NOTE: This will surely cause troubles with transparency, but for now it'll stay like this.
	DWORD zwrite;
	pd3dDevice->GetRenderState(D3DRS_ZWRITEENABLE, &zwrite);
	bool firstPass = (zwrite == D3DZB_TRUE);
	if (firstPass)
	{
		return;
	}

	ColladaEffectTechnique* tech = mOwner->getCurrentTechnique();
	if (tech == NULL || !tech->isValid())
	{
		DrawError();
		return;
	}

	// we're in the second pass here, enable z writing since we skipped the first one
	pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, D3DZB_TRUE);
	// the alpha func is currently set to LESS with an ALPHAREF of 254...
	pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
	// Max has a weird behavior when either other objects are selected or moved, it puts its shade model to flat.
	pd3dDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);

#ifndef DEBUG_CG

	// TODO.
	// Set the effect render states.

	// for TEXCOORDX
	Tab<int>mappingChannelTab;
	IRenderMesh* renderMesh = mpMeshCache->GetActiveRenderMesh(m_CurCache);
	Mesh* activeMesh = mpMeshCache->GetActiveMesh(m_CurCache);

	// retrieve all supported channels and feed them to the shader
	for (int i = 0; i < activeMesh->getNumMaps(); i++)
	{
		if (!activeMesh->mapSupport(i)) continue;
		if (mappingChannelTab.Append(1,&i) == 7)
			break; // maximum = 8
	}
	// Allows the app to provide upto 8 mapping channels, this is used to extract the texcoord data from the mesh
	renderMesh->SetMappingData(mappingChannelTab);

	// Setting the node parameter to non NULL, will make sure the Normals are transformed correctly
	renderMesh->Evaluate(pd3dDevice, activeMesh,0,false);

	// doesn't work on DX9, only on DX10
	//renderMesh->GetVertexFormat();

	// buffer the blend states
	DWORD alphaBlend, zFunc, blendSrc, blendDst;
	pd3dDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &alphaBlend);
	pd3dDevice->GetRenderState(D3DRS_ZFUNC, &zFunc);
	pd3dDevice->GetRenderState(D3DRS_SRCBLEND, &blendSrc);
	pd3dDevice->GetRenderState(D3DRS_DESTBLEND, &blendDst);

	size_t passCount = tech->getPassCount();
	bool rendered = false;
	for (size_t i = 0; i < passCount; ++i)
	{
		ColladaEffectPass* pass = tech->getPass(i);
		if (pass == NULL) continue;
		if (!pass->isValid()) continue;
		pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		if (i>0)
		{
			pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		}
		else
		{
			pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		}
		pass->begin(myGWindow, mpMeshCache->GetActiveNode(m_CurCache));
		mpMeshCache->GetActiveRenderMesh(m_CurCache)->Render(pd3dDevice);
		pass->end();
		rendered = true;
	}

	// reset the blend states
	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, alphaBlend);
	pd3dDevice->SetRenderState(D3DRS_ZFUNC, zFunc);
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, blendSrc);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, blendDst);

	// set the z writing state back to its original value
	pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, D3DZB_FALSE);
	// set the alpha test back to its original value
	pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_LESS);

	if (!rendered)
	{
		DrawError();
	}
#else
	// Create everything from scratch (warning: slow!)
	cgD3D9SetDevice(pd3dDevice);
	DebugCG();
	CGcontext context = cgCreateContext();
	DebugCG();
	cgD3D9RegisterStates(context);
	DebugCG();
	CGeffect effect = cgCreateEffectFromFile(context, "C:\\Program Files\\Autodesk\\3ds Max 9\\maps\\fx\\goochy.cgfx", NULL);
	DebugCG();

	// load vertex
	CGstateassignment vertAssign = cgGetNamedStateAssignment(cgGetFirstPass(cgGetFirstTechnique(effect)),"VertexProgram");
	DebugCG();
	CGprogram vertexProgram = cgGetProgramStateAssignmentValue(vertAssign);
	DebugCG();
	CGprofile vertexProfile = cgGetProfile(cgGetProgramString(vertexProgram, CG_PROGRAM_PROFILE));
	DebugCG();
	vertexProgram = cgCreateProgramFromEffect(effect, vertexProfile, cgGetProgramString(vertexProgram, CG_PROGRAM_ENTRY), NULL);
	DebugCG();

	CGbool shadowing = 0;
	/*
	D3DXSHADER_DEBUG	Insert debug filename, line numbers, and type and symbol information during shader compile.
	D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT	Force the compiler to compile against the next highest available software target for pixel shaders. This flag also turns optimizations off and debugging on.
	D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT	Force the compiler to compile against the next highest available software target for vertex shaders. This flag also turns optimizations off and debugging on.
	D3DXSHADER_PACKMATRIX_COLUMNMAJOR	Unless explicitly specified, matrices will be packed in column-major order (each vector will be in a single column) when passed to and from the shader. This is generally more efficient because it allows vector-matrix multiplication to be performed using a series of dot products.
	D3DXSHADER_PACKMATRIX_ROWMAJOR	Unless explicitly specified, matrices will be packed in row-major order (each vector will be in a single row) when passed to or from the shader.
	D3DXSHADER_PARTIALPRECISION	Force all computations in the resulting shader to occur at partial precision. This may result in faster evaluation of shaders on some hardware.
	D3DXSHADER_SKIPOPTIMIZATION	Instruct the compiler to skip optimization steps during code generation. Unless you are trying to isolate a problem in your code and you suspect the compiler, using this option is not recommended.
	D3DXSHADER_SKIPVALIDATION	Do not validate the generated code against known capabilities and constraints. This option is recommended only when compiling shaders that are known to work (that is, shaders that have compiled before without this option). Shaders are always validated by the runtime before they are set to the device.
	*/
	DWORD flags = 0;
	cgD3D9LoadProgram(vertexProgram, shadowing, flags);
	DebugCG();
	
	// load pixel
	CGstateassignment fragAssign = cgGetNamedStateAssignment(cgGetFirstPass(cgGetFirstTechnique(effect)),"FragmentProgram");
	DebugCG();
	CGprogram fragmentProgram = cgGetProgramStateAssignmentValue(fragAssign);
	DebugCG();
	CGprofile fragmentProfile = cgGetProfile(cgGetProgramString(fragmentProgram, CG_PROGRAM_PROFILE));
	DebugCG();
	fragmentProgram = cgCreateProgramFromEffect(effect, fragmentProfile, cgGetProgramString(fragmentProgram, CG_PROGRAM_ENTRY), NULL);
	DebugCG();
	cgD3D9LoadProgram(fragmentProgram, shadowing, flags);
	DebugCG();

	//// retrieve all render states (for debugging purposes only)
	//static const int RSCOUNT = 210;
	//static DWORD pass1States[RSCOUNT];
	//static DWORD pass2States[RSCOUNT];

	//for (size_t i = 0; i < RSCOUNT; ++i)
	//{
	//	DWORD* passStates = (firstPass) ? pass1States : pass2States;
	//	pd3dDevice->GetRenderState(D3DRENDERSTATETYPE(i), &(passStates[i]));
	//}
	//firstPass = !firstPass;

	// set parameters
	D3DXMATRIX world, view, proj, worldinversetranspose, worldviewproj;
	pd3dDevice->GetTransform(D3DTS_WORLD, &world);
	pd3dDevice->GetTransform(D3DTS_VIEW, &view);
	pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);

	D3DXMatrixTranspose(&worldinversetranspose, D3DXMatrixInverse(&worldinversetranspose, NULL, &world));
	// if we use the cgD3D9* API, we MUST transpose our matrices before setting them (row vs column major)
	// another method would be to use the row-major versions of the cg core API for setting matrices, but I guess
	// Cg does it internally.
	D3DXMatrixTranspose(&worldinversetranspose, &worldinversetranspose);

	D3DXMatrixMultiply(&worldviewproj, D3DXMatrixMultiply(&worldviewproj, &world, &view), &proj);
	D3DXMatrixTranspose(&worldviewproj, &worldviewproj);

	// set transformations, colors and positions
	CGparameter wit = cgGetNamedProgramParameter(vertexProgram, CG_GLOBAL, "WorldITXf");
	DebugCG();
	cgD3D9SetUniformMatrix(wit, &worldinversetranspose);
	DebugCG();
	CGparameter wvp = cgGetNamedProgramParameter(vertexProgram, CG_GLOBAL, "WorldViewProjXf");
	DebugCG();
	cgD3D9SetUniformMatrix(wvp, &worldviewproj);
	DebugCG();
	CGparameter w = cgGetNamedProgramParameter(vertexProgram, CG_GLOBAL, "WorldXf");
	DebugCG();
	D3DXMatrixTranspose(&world, &world); // LH transposition
	cgD3D9SetUniformMatrix(w, &world);

	D3DXCOLOR color;
	D3DXVECTOR4 position;

	CGparameter param = cgGetNamedProgramParameter(vertexProgram, CG_GLOBAL, "LiteColor");
	DebugCG();
	cgGetParameterValuefr(param, 3, (float*)color);
	cgD3D9SetUniform(param, (float*)color);
	DebugCG();

	param = cgGetNamedProgramParameter(vertexProgram, CG_GLOBAL, "DarkColor");
	DebugCG();
	cgGetParameterValuefr(param, 3, (float*)color);
	cgD3D9SetUniform(param, (float*)color);
	DebugCG();

	param = cgGetNamedProgramParameter(vertexProgram, CG_GLOBAL, "WarmColor");
	DebugCG();
	cgGetParameterValuefr(param, 3, (float*)color);
	cgD3D9SetUniform(param, (float*)color);
	DebugCG();

	param = cgGetNamedProgramParameter(vertexProgram, CG_GLOBAL, "CoolColor");
	DebugCG();
	cgGetParameterValuefr(param, 3, (float*)color);
	cgD3D9SetUniform(param, (float*)color);
	DebugCG();

	param = cgGetNamedProgramParameter(vertexProgram, CG_GLOBAL, "LightPos");
	DebugCG();
	cgGetParameterValuefr(param, 4, (float*)position);
	cgD3D9SetUniform(param, (float*)position);
	DebugCG();

	// bind programs
	cgD3D9BindProgram(vertexProgram);
	DebugCG();
	cgD3D9BindProgram(fragmentProgram);
	DebugCG();

	// draw!
	mpMeshCache->GetActiveRenderMesh(m_CurCache)->Evaluate(pd3dDevice, mpMeshCache->GetActiveMesh(m_CurCache),0,false);
	mpMeshCache->GetActiveRenderMesh(m_CurCache)->Render(pd3dDevice);

	// Since we are recreating all our programs based on the device type,
    // we destroy our programs every time we destroy the device
    cgDestroyContext(context);
	DebugCG();
    // Tell the runtime to remove its reference to the D3D device.
    cgD3D9SetDevice(NULL);
	DebugCG();

	// set the z writing state back to its original value
	pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, D3DZB_FALSE);
	// set the alpha test back to its original value
	pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_LESS);
	
#endif // DEBUG CG
}
void ColladaEffectRenderer::DrawError()
{
	HRESULT hr = S_OK;
	LPD3DXBUFFER error;
	if (!pd3dDevice)
		pd3dDevice = GetDevice();

	if (!m_pErrorEffect)
	{
		hr = D3DXCreateEffectFromResource(pd3dDevice,hInstance,MAKEINTRESOURCE(IDR_RCDATA1),NULL,NULL,0L,NULL,&m_pErrorEffect,&error);
		if (FAILED(hr))
			return;
	}
	D3DXMATRIX projworldview, temp;

	if (!myGWindow)
		return;

	ID3D9GraphicsWindow * d3d9 = static_cast<ID3D9GraphicsWindow*>(myGWindow);

	D3DXMatrixMultiply(&(temp),&(d3d9->GetWorldXform()), &(d3d9->GetViewXform()));
	D3DXMatrixMultiply(&(projworldview),&(temp),&(d3d9->GetProjXform()));

	D3DXHANDLE invProjH = m_pErrorEffect->GetParameterByName(NULL, "wvp");
	hr = m_pErrorEffect->SetMatrix(invProjH,&projworldview);

	int DefaultMapChannel = 1;
	Tab<int>mappingChannelTab;

	mappingChannelTab.Append(1,&DefaultMapChannel);
	mpMeshCache->GetActiveRenderMesh(m_CurCache)->SetMappingData(mappingChannelTab);
	mpMeshCache->GetActiveRenderMesh(m_CurCache)->Evaluate(pd3dDevice, mpMeshCache->GetActiveMesh(m_CurCache),0,false);

	UINT uPasses = 0;
	hr = m_pErrorEffect->SetTechnique(_T("t0"));
	hr = m_pErrorEffect->Begin(&uPasses, 0);

	for (UINT uPass = 0; uPass < uPasses; uPass++)
	{
		hr = m_pErrorEffect->BeginPass(uPass);
		mpMeshCache->GetActiveRenderMesh(m_CurCache)->Render(pd3dDevice);
		hr = m_pErrorEffect->EndPass();
	}
	hr = m_pErrorEffect->CommitChanges();
	hr = m_pErrorEffect->End();	

	hr;

}

bool ColladaEffectRenderer::DrawMeshStrips(ID3D9GraphicsWindow* /*d3dgw*/, MeshData* /*data*/)
{
	//return IsDxMaterialEnabled(map);
	return false;
}

void ColladaEffectRenderer::SetTechnique(D3DXHANDLE handle, TCHAR * techniqueName, bool /*bDefault*/)
{
	hTechnique = handle;
	this->techniqueName = TSTR(techniqueName);
}

void ColladaEffectRenderer::NotificationCallback(void* param, NotifyInfo* info) {

	ColladaEffectRenderer* renderer = static_cast<ColladaEffectRenderer*>(param);
	//INode * newNode  = (INode*)info->callParam;
	//TimeValue t = GetCOREInterface()->GetTime();

//#ifndef FORCE_SMOKETEST
//	if (!IsDxMaterialEnabled(shader->GetStdMat()))
//		return;
//#endif
	

	//if (info->intcode == NOTIFY_SCENE_ADDED_NODE || info->intcode == NOTIFY_SCENE_PRE_DELETED_NODE)
	//{
	//	if (newNode && !newNode->IsTarget())
	//	{
	//		if (!inHold)
	//		{
	//		
	//			ObjectState Os   = newNode->EvalWorldState(t);

	//			if (Os.obj->SuperClassID() == LIGHT_CLASS_ID)
	//			{
	//				//Update Effect File
	//				if (info->intcode == NOTIFY_SCENE_ADDED_NODE)
	//				{
	//					shader->UpdateSceneLights();
	//					shader->CreateAndLoadEffectData();
	//				}
	//				else
	//					shader->bPreDeleteLight = true;
	//			}
	//		}
	//	}
	//}

	//if (info->intcode == NOTIFY_FILE_POST_OPEN)
	//{
	//	if (!inHold){
	//		shader->UpdateSceneLights();
	//		shader->CreateAndLoadEffectData();
	//	}
	//}

	//if (info->intcode == NOTIFY_SCENE_POST_DELETED_NODE && shader->bPreDeleteLight){
	//	if (!inHold){
	//		shader->UpdateSceneLights();
	//		shader->CreateAndLoadEffectData();
	//		shader->bPreDeleteLight = false;
	//	}
	//}
	//if (info->intcode == NOTIFY_SCENE_PRE_UNDO || info->intcode == NOTIFY_SCENE_PRE_REDO)
	//{
	//	inHold = TRUE;
	//}
	//
	//// no real option for now - but we need to rebuild the effect
	//if (info->intcode == NOTIFY_SCENE_POST_UNDO || info->intcode == NOTIFY_SCENE_POST_REDO)
	//{
	//	if (shader){
	//		shader->UpdateSceneLights();
	//		shader->CreateAndLoadEffectData();
	//	}
	//	inHold = FALSE;
	//}


#if (MAX_VERSION_MAJOR > 7)	
	if (info->intcode == NOTIFY_D3D_PRE_DEVICE_RESET)
	{
		// All D3D resources for programs previously loaded with cgD3D9LoadProgram are destroyed. 
		// However, these programs are still considered managed by the expanded interface, 
		// so if a new D3D device is set later these programs will be recreated using the new D3D device.
		cgD3D9SetDevice(NULL);
		DebugCG();

		// Release the textures allocated on D3DPOOL_DEFAULT.
		if (renderer != NULL)
		{
			renderer->GetColladaEffect()->releaseTextureParameters();
		}
	}

	if (info->intcode == NOTIFY_D3D_POST_DEVICE_RESET)
	{
		cgD3D9SetDevice(GetDevice());
		DebugCG();

		// re-create the texture parameters.
		if (renderer != NULL)
		{
			renderer->GetColladaEffect()->recreateTextureParameters(GetDevice());
		}
	}
#else
	(void) info;
	(void) renderer;
#endif // MAX_VERSION
}

