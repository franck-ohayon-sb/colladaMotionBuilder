/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

const char* CFXRenderState::CG_RENDER_STATE_NAMES[] = {
	"AlphaFunc",
	"AlphaTestEnable",
	"AutoNormalEnable",

	"BlendFunc",
	"BlendFuncSeparate",
	"BlendEquation",
	"BlendEquationSeparate",
	"BlendColor",
	"BlendEnable",

	"ClearColor",
	"ClearStencil",
	"ClearDepth",
	"ClipPlane",
	"ColorMask",
	"ColorMatrix",
	"ColorMaterial",
	"CullFace",
	"ClipPlaneEnable",
	"ColorLogicOpEnable",
	"CullFaceEnable",

	"DepthBounds",
	"DepthFunc",
	"DepthMask",
	"DepthRange",
	"DepthBoundsEnable",
	"DepthClampEnable",
	"DepthTestEnable",
	"DitherEnable",

	"FogMode",
	"FogEnable",
	"FogDensity",
	"FogStart",
	"FogEnd",
	"FogColor",
	"FragmentEnvParameter",
	"FragmentLocalParameter",
	"FogCoordSrc",
	"FogDistanceMode",
	"FragmentProgram",
	"FrontFace",

	"LightModelAmbient",
	"LightAmbient",
	"LightDiffuse",
	"LightSpecular",
	"LightConstantAttenuation",
	"LightLinearAttenuation",
	"LightQuadraticAttenuation",
	"LightPosition",
	"LightSpotCutoff",
	"LightSpotDirection",
	"LightSpotExponent",
	"LightModelColorControl",
	"LineStipple",
	"LineWidth",
	"LogicOp",
	"LogicOpEnable",
	"LightEnable",
	"LightingEnable",
	"LightModelLocalViewerEnable",
	"LightModelTwoSideEnable",
	"LineSmoothEnable",
	"LineStippleEnable",

	"MaterialAmbient",
	"MaterialDiffuse",
	"MaterialEmission",
	"MaterialShininess",
	"MaterialSpecular",
	"ModelViewMatrix",
	"MultisampleEnable",

	"NormalizeEnable",

	"PointDistanceAttenuation",
	"PointFadeThresholdSize",
	"PointSize",
	"PointSizeMin",
	"PointSizeMax",
	"PointSpriteCoordOrigin",
	"PointSpriteCoordReplace",
	"PointSpriteRMode",
	"PolygonMode",
	"PolygonOffset",
	"ProjectionMatrix",
	"PointSmoothEnable",
	"PointSpriteEnable",
	"PolygonOffsetFillEnable",
	"PolygonOffsetLineEnable",
	"PolygonOffsetPointEnable",
	"PolygonSmoothEnable",
	"PolygonStippleEnable",

	"RescaleNormalEnable",

	"Scissor",
	"ShadeModel",
	"StencilFunc",
	"StencilMask",
	"StencilOp",
	"StencilFuncSeparate",
	"StencilMaskSeparate",
	"StencilOpSeparate",
	"SampleAlphaToCoverageEnable",
	"SampleAlphaToOneEnable",
	"SampleCoverageEnable",
	"ScissorTestEnable",
	"StencilTestEnable",

	"TexGenSMode",
	"TexGenTMode",
	"TexGenRMode",
	"TexGenQMode",
	"TexGenSEyePlane",
	"TexGenTEyePlane",
	"TexGenREyePlane",
	"TexGenQEyePlane",
	"TexGenSObjectPlace",
	"TexGenTObjectPlace",
	"TexGenRObjectPlace",
	"TexGenQObjectPlace",
	"Texture1D",
	"Texture2D",
	"Texture3D",
	"TextureRectangle",
	"TextureCubeMap",
	"TextureEnvColor",
	"TextureEnvMode",
	"TexGenSEnable",
	"TexGenTEnable",
	"TexGenREnable",
	"TexGenQEnable",
	"Texture1DEnable",
	"Texture2DEnable",
	"Texture3DEnable",
	"TextureRectangleEnable",
	"TextureCubeMapEnable",

	"VertexEnvParameter",
	"VertexLocalParameter",
	"VertexProgram"
};

const FUDaePassState::State CFXRenderState::CG_RENDER_STATES_XREF[] = {
	FUDaePassState::ALPHA_FUNC, //AlphaFunc
	FUDaePassState::ALPHA_TEST_ENABLE, //AlphaTestEnable
	FUDaePassState::AUTO_NORMAL_ENABLE, //AutoNormalEnable

	FUDaePassState::BLEND_FUNC, //BlendFunc
	FUDaePassState::BLEND_FUNC_SEPARATE, //BlendFuncSeparate
	FUDaePassState::BLEND_EQUATION, //BlendEquation
	FUDaePassState::BLEND_EQUATION_SEPARATE, //BlendEquationSeparate
	FUDaePassState::BLEND_COLOR, //BlendColor
	FUDaePassState::BLEND_ENABLE, //BlendEnable

	FUDaePassState::CLEAR_COLOR, //ClearColor
	FUDaePassState::CLEAR_STENCIL, //ClearStencil
	FUDaePassState::CLEAR_DEPTH, //ClearDepth
	FUDaePassState::CLIP_PLANE, //ClipPlane
	FUDaePassState::COLOR_MASK, //ColorMask
	FUDaePassState::INVALID, //ColorMatrix
	FUDaePassState::COLOR_MATERIAL, //ColorMaterial
	FUDaePassState::CULL_FACE, //CullFace
	FUDaePassState::CLIP_PLANE_ENABLE, //ClipPlaneEnable
	FUDaePassState::COLOR_LOGIC_OP_ENABLE, //ColorLogicOpEnable
	FUDaePassState::CULL_FACE_ENABLE, //CullFaceEnable

	FUDaePassState::DEPTH_BOUNDS, //DepthBounds
	FUDaePassState::DEPTH_FUNC, //DepthFunc
	FUDaePassState::DEPTH_MASK, //DepthMask
	FUDaePassState::DEPTH_RANGE, //DepthRange
	FUDaePassState::DEPTH_BOUNDS_ENABLE, //DepthBoundsEnable
	FUDaePassState::DEPTH_CLAMP_ENABLE, //DepthClampEnable
	FUDaePassState::DEPTH_TEST_ENABLE, //DepthTestEnable
	FUDaePassState::DITHER_ENABLE, //DitherEnable

	FUDaePassState::FOG_MODE, //FogMode
	FUDaePassState::FOG_ENABLE, //FogEnable
	FUDaePassState::FOG_DENSITY, //FogDensity
	FUDaePassState::FOG_START, //FogStart
	FUDaePassState::FOG_END, //FogEnd
	FUDaePassState::FOG_COLOR, //FogColor
	FUDaePassState::INVALID, //FragmentEnvParameter
	FUDaePassState::INVALID, //FragmentLocalParameter
	FUDaePassState::FOG_COORD_SRC, //FogCoordSrc
	FUDaePassState::INVALID, //FogDistanceMode
	FUDaePassState::INVALID, //FragmentProgram
	FUDaePassState::FRONT_FACE, //FrontFace

	FUDaePassState::LIGHT_MODEL_AMBIENT, //LightModelAmbient
	FUDaePassState::LIGHT_AMBIENT, //LightAmbient
	FUDaePassState::LIGHT_DIFFUSE, //LightDiffuse
	FUDaePassState::LIGHT_SPECULAR, //LightSpecular
	FUDaePassState::LIGHT_CONSTANT_ATTENUATION, //LightConstantAttenuation
	FUDaePassState::LIGHT_LINEAR_ATTENUATION, //LightLinearAttenuation
	FUDaePassState::LIGHT_QUADRATIC_ATTENUATION, //LightQuadraticAttenuation
	FUDaePassState::LIGHT_POSITION, //LightPosition
	FUDaePassState::LIGHT_SPOT_CUTOFF, //LightSpotCutoff
	FUDaePassState::LIGHT_SPOT_DIRECTION, //LightSpotDirection
	FUDaePassState::LIGHT_SPOT_EXPONENT, //LightSpotExponent
	FUDaePassState::LIGHT_MODEL_COLOR_CONTROL, //LightModelColorControl
	FUDaePassState::LINE_STIPPLE, //LineStipple
	FUDaePassState::LINE_WIDTH, //LineWidth
	FUDaePassState::LOGIC_OP, //LogicOp
	FUDaePassState::LOGIC_OP_ENABLE, //LogicOpEnable
	FUDaePassState::LIGHT_ENABLE, //LightEnable
	FUDaePassState::LIGHTING_ENABLE, //LightingEnable
	FUDaePassState::LIGHT_MODEL_LOCAL_VIEWER_ENABLE, //LightModelLocalViewerEnable
	FUDaePassState::LIGHT_MODEL_TWO_SIDE_ENABLE, //LightModelTwoSideEnable
	FUDaePassState::LINE_SMOOTH_ENABLE, //LineSmoothEnable
	FUDaePassState::LINE_STIPPLE_ENABLE, //LineStippleEnable

	FUDaePassState::MATERIAL_AMBIENT, //MaterialAmbient
	FUDaePassState::MATERIAL_DIFFUSE, //MaterialDiffuse
	FUDaePassState::MATERIAL_EMISSION, //MaterialEmission
	FUDaePassState::MATERIAL_SHININESS, //MaterialShininess
	FUDaePassState::MATERIAL_SPECULAR, //MaterialSpecular
	FUDaePassState::MODEL_VIEW_MATRIX, //ModelViewMatrix
	FUDaePassState::MULTISAMPLE_ENABLE, //MultisampleEnable

	FUDaePassState::NORMALIZE_ENABLE, //NormalizeEnable

	FUDaePassState::POINT_DISTANCE_ATTENUATION, //PointDistanceAttenuation
	FUDaePassState::POINT_FADE_THRESHOLD_SIZE, //PointFadeThresholdSize
	FUDaePassState::POINT_SIZE, //PointSize
	FUDaePassState::POINT_SIZE_MIN, //PointSizeMin
	FUDaePassState::POINT_SIZE_MAX, //PointSizeMax
	FUDaePassState::INVALID, //PointSpriteCoordOrigin
	FUDaePassState::INVALID, //PointSpriteCoordReplace
	FUDaePassState::INVALID, //PointSpriteRMode
	FUDaePassState::POLYGON_MODE, //PolygonMode
	FUDaePassState::POLYGON_OFFSET, //PolygonOffset
	FUDaePassState::PROJECTION_MATRIX, //ProjectionMatrix
	FUDaePassState::POINT_SMOOTH_ENABLE, //PointSmoothEnable
	FUDaePassState::INVALID, //PointSpriteEnable
	FUDaePassState::POLYGON_OFFSET_FILL_ENABLE, //PolygonOffsetFillEnable
	FUDaePassState::POLYGON_OFFSET_LINE_ENABLE, //PolygonOffsetLineEnable
	FUDaePassState::POLYGON_OFFSET_POINT_ENABLE, //PolygonOffsetPointEnable
	FUDaePassState::POLYGON_SMOOTH_ENABLE, //PolygonSmoothEnable
	FUDaePassState::POLYGON_STIPPLE_ENABLE, //PolygonStippleEnable

	FUDaePassState::RESCALE_NORMAL_ENABLE, //RescaleNormalEnable

	FUDaePassState::SCISSOR, //Scissor
	FUDaePassState::SHADE_MODEL, //ShadeModel
	FUDaePassState::STENCIL_FUNC, //StencilFunc
	FUDaePassState::STENCIL_MASK, //StencilMask
	FUDaePassState::STENCIL_OP, //StencilOp
	FUDaePassState::STENCIL_FUNC_SEPARATE, //StencilFuncSeparate
	FUDaePassState::STENCIL_MASK_SEPARATE, //StencilMaskSeparate
	FUDaePassState::STENCIL_OP_SEPARATE, //StencilOpSeparate
	FUDaePassState::SAMPLE_ALPHA_TO_COVERAGE_ENABLE, //SampleAlphaToCoverageEnable
	FUDaePassState::SAMPLE_ALPHA_TO_ONE_ENABLE, //SampleAlphaToOneEnable
	FUDaePassState::SAMPLE_COVERAGE_ENABLE, //SampleCoverageEnable
	FUDaePassState::SCISSOR_TEST_ENABLE, //ScissorTestEnable
	FUDaePassState::STENCIL_TEST_ENABLE, //StencilTestEnable

	FUDaePassState::INVALID, //TexGenSMode
	FUDaePassState::INVALID, //TexGenTMode
	FUDaePassState::INVALID, //TexGenRMode
	FUDaePassState::INVALID, //TexGenQMode
	FUDaePassState::INVALID, //TexGenSEyePlane
	FUDaePassState::INVALID, //TexGenTEyePlane
	FUDaePassState::INVALID, //TexGenREyePlane
	FUDaePassState::INVALID, //TexGenQEyePlane
	FUDaePassState::INVALID, //TexGenSObjectPlace
	FUDaePassState::INVALID, //TexGenTObjectPlace
	FUDaePassState::INVALID, //TexGenRObjectPlace
	FUDaePassState::INVALID, //TexGenQObjectPlace
	FUDaePassState::TEXTURE1D, //Texture1D
	FUDaePassState::TEXTURE2D, //Texture2D
	FUDaePassState::TEXTURE3D, //Texture3D
	FUDaePassState::TEXTURERECT, //TextureRectangle
	FUDaePassState::TEXTURECUBE, //TextureCubeMap
	FUDaePassState::TEXTURE_ENV_COLOR, //TextureEnvColor
	FUDaePassState::TEXTURE_ENV_MODE, //TextureEnvMode
	FUDaePassState::INVALID, //TexGenSEnable
	FUDaePassState::INVALID, //TexGenTEnable
	FUDaePassState::INVALID, //TexGenREnable
	FUDaePassState::INVALID, //TexGenQEnable
	FUDaePassState::TEXTURE1D_ENABLE, //Texture1DEnable
	FUDaePassState::TEXTURE2D_ENABLE, //Texture2DEnable
	FUDaePassState::TEXTURE3D_ENABLE, //Texture3DEnable
	FUDaePassState::TEXTURERECT_ENABLE, //TextureRectangleEnable
	FUDaePassState::TEXTURECUBE_ENABLE, //TextureCubeMapEnable

	FUDaePassState::INVALID, //VertexEnvParameter
	FUDaePassState::INVALID, //VertexLocalParameter
	FUDaePassState::INVALID, //VertexProgram
};
