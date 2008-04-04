ColladaMotionBuilder Exporter
--------------------------------
Written by Feeling Software Inc.
Uses the latest version of FCollada.
http://www.feelingsoftware.com/


Features:
-----------
- Export of scene graph.
-- Geometry, controller, camera and light instances.
-- Visibility, Translation, Rotation and Scale transforms.
-- All constraints are automatically sampled and exported as <matrix> transforms.
-- Scene nodes that contain a camera instance will automatically get one <matrix> transform
   at the top of the transform stack for their node, regardless of whether they are constrained.
   If this node is also constrained, the node will contain only one <matrix> transform.

- Export of lights.
-- Point, directional, spot and the one global ambient light, with COLLADA parameters only.

- Export of cameras.
-- Both orthographic and projection, with COLLADA parameters only.
--- Orthographic always have a <ymag> and an <aspect_ratio> parameters. Only the <ymag> is animatable.
--- Perspective cameras will have <xfov>&<yfov>, <xfov>&<aspect_ratio> or <yfov>&<aspect_ratio>, depending
    on the "ApertureMode" property. The MotionBuilder API is not allowing us to support the "FocalLength"
    aperture mode. 
-- Export of system cameras through an export option.

- Export of geometries: meshes only: 
-- All three inner channels: positions, normals, texcoords.
-- Multiple materials supported, but not with disconnected textures.
-- Polygons and triangle strips tessellation support through an export option.
-- 3D or 4D position/normals support through an export option.

- Export of animations on most parameters:
-- no clip or pose support for this version.
-- TCB, Bézier, linear and step interpolation.
--- We convert Cardinal into Bezier and we calculate the 2D tangent points (Motion Builder has 1D tangents).
--- We export all three TCB parameters but do not guarantee inter-operability with other packages.

- Export of textured materials: profile_COMMON only.
-- Support for multiple materials on a mesh.
-- We do our best to attempt to match MotionBuilder's texture channels with FCollada's.

- Export of skin controllers.
-- No morphers in Motion Builder.


UI options:
---------------
1) Geometry: triangle-strip meshes. Uses the Motion Builder triangle strip tessellator
   instead of the polygon tessellator. Expect the <tristrips> element instead of the <polylist>/<triangles> elements.
   Defaults to FALSE.
2) Geometry: 3D data [/4D data]. When enabled, automatically removes the 4th coordinate on
   mesh position and normal data arrays. Defaults to TRUE.
3) Cameras: export system cameras. When enabled, the system cameras: "Producer perspective" and
   "Producer back/front/..." are exported within invisible nodes.

Support and Contact Info:
-----------------------------
If you experience issues with the exporter, please post on our forums.
We also welcome all feedback and ideas.

http://www.feelingsoftware.com/

Guillaume Laforte
glaforte@feelingsoftware.com
Main Code Monkey.
Feeling Software Inc.
