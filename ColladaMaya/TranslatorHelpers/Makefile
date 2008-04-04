# original file from http://www.collada.org/public_forum/viewtopic.php?t=48
# update Sébastien Kuntz ( sebastien.kuntz [a.t] nowan.net )

#--Global-Configuration-----------
SHELL= /bin/sh
MAKE= make
APP= libTranslatorHelpers.a

#--OS-Specific-Configuration-----------
CC = g++
AR = ar

INC = -I../libxml/include -I/usr/aw/maya/include -I. -I..
LIB = -L/usr/aw/maya/lib/ -L../libxml/lib -lxml2 -lz -lOpenMaya -lFoundation -lOpenMayaAnim -lOpenMayaFX -lOpenMayaUI -lOpenMayaRender -shared --Wl,-rpath=/usr/aw/maya/lib/

OBJS =  CAnimationHelper.o CDagHelper.o CMeshHelper.o  \
	CSceneWalker.o CShaderHelper.o HIAnimCache.o MayaHelper.o
                                                                                                                                                             
ARFLAGS = -r
CFLAGS = -D_BOOL -DLINUX -fno-gnu-keywords -mcpu=pentium4
CCFLAGS = -D_BOOL

#--Suffix--Rules-----------
.SUFFIXES: .o .cpp .h .a

.cpp.o: $*.h
	@ echo $(CC) $<
	@ $(CC) $(INC) $(CCFLAGS) -c $<

#--Make-Application-----------
$(APP): $(OBJS)
	@ echo Creating archive $@...
	@ $(AR) $(ARFLAGS) $@ $(OBJS)

#--Make-All-----------
all:
	$(MAKE) depend
	$(MAKE) clean
	$(MAKE) $(APP)

#--Make-Depend-----------
depend:
	makedepend *.cpp *.cxx *.c *.h -Y 2> ./MAKEDEPEND-MESSAGES
                                                                                                                                                             
#--Make-Docs-----------
docs: FORCE_REBUILD
	doxygen
 
 
#--Clean-----------
clean:
	rm -f core *.core *.gz *.a $(OBJS) $(APP)
 
 
#--Clean-App-----------
cleanapp:
	rm -f core *.core *.gz *.a 
# DO NOT DELETE

CAnimationHelper.o: StdAfx.h ../StdAfx.h ../TranslatorHelpers/MayaHelper.h
CAnimationHelper.o: ../FUtils/FUtils.h ../TranslatorHelpers/MayaConvert.h
CAnimationHelper.o: ../DaeUtils.h ../DaeSceneElement.h ../DaeAnimationTypes.h
CAnimationHelper.o: ../FUtils/DAESyntax.h ../DaeDoc.h
CAnimationHelper.o: ../TranslatorHelpers/CSceneVisitor.h
CAnimationHelper.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
CAnimationHelper.o: CAnimationHelper.h ../DaeAnimationLibrary.h
CAnimationHelper.o: ../DaeCameraLibrary.h ../DaeBaseLibrary.h
CAnimationHelper.o: ../DaeControllerLibrary.h ../DaeGeometryLibrary.h
CAnimationHelper.o: ../DaeLightLibrary.h ../DaeMaterialLibrary.h
CAnimationHelper.o: ../DaeInput.h ../DaePhysicsMaterialLibrary.h
CAnimationHelper.o: ../DaePhysicsModelLibrary.h ../DaePhysicsSceneLibrary.h
CAnimationHelper.o: ../DaePhysicsScene.h ../DaeAgeiaPhysicsScene.h
CAnimationHelper.o: ../DaeNativePhysicsScene.h ../DaeSceneGraph.h
CAnimationHelper.o: ../DaeTextureLibrary.h
CAnimationHelper.o: ../TranslatorHelpers/CShaderHelper.h ../CExportOptions.h
CAnimationHelper.o: CDagHelper.h HIAnimCache.h
CDagHelper.o: StdAfx.h ../StdAfx.h ../TranslatorHelpers/MayaHelper.h
CDagHelper.o: ../FUtils/FUtils.h ../TranslatorHelpers/MayaConvert.h
CDagHelper.o: ../DaeUtils.h ../DaeSceneElement.h ../DaeAnimationTypes.h
CDagHelper.o: ../FUtils/DAESyntax.h ../DaeDoc.h
CDagHelper.o: ../TranslatorHelpers/CSceneVisitor.h
CDagHelper.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
CDagHelper.o: CAnimationHelper.h ../DaeAnimationLibrary.h
CDagHelper.o: ../DaeCameraLibrary.h ../DaeBaseLibrary.h
CDagHelper.o: ../DaeControllerLibrary.h ../DaeGeometryLibrary.h
CDagHelper.o: ../DaeLightLibrary.h ../DaeMaterialLibrary.h ../DaeInput.h
CDagHelper.o: ../DaePhysicsMaterialLibrary.h ../DaePhysicsModelLibrary.h
CDagHelper.o: ../DaePhysicsSceneLibrary.h ../DaePhysicsScene.h
CDagHelper.o: ../DaeAgeiaPhysicsScene.h ../DaeNativePhysicsScene.h
CDagHelper.o: ../DaeSceneGraph.h ../DaeTextureLibrary.h
CDagHelper.o: ../TranslatorHelpers/CShaderHelper.h CDagHelper.h
CMeshHelper.o: StdAfx.h ../StdAfx.h ../TranslatorHelpers/MayaHelper.h
CMeshHelper.o: ../FUtils/FUtils.h ../TranslatorHelpers/MayaConvert.h
CMeshHelper.o: ../DaeUtils.h ../DaeSceneElement.h ../DaeAnimationTypes.h
CMeshHelper.o: ../FUtils/DAESyntax.h ../DaeDoc.h
CMeshHelper.o: ../TranslatorHelpers/CSceneVisitor.h
CMeshHelper.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
CMeshHelper.o: CAnimationHelper.h ../DaeAnimationLibrary.h
CMeshHelper.o: ../DaeCameraLibrary.h ../DaeBaseLibrary.h
CMeshHelper.o: ../DaeControllerLibrary.h ../DaeGeometryLibrary.h
CMeshHelper.o: ../DaeLightLibrary.h ../DaeMaterialLibrary.h ../DaeInput.h
CMeshHelper.o: ../DaePhysicsMaterialLibrary.h ../DaePhysicsModelLibrary.h
CMeshHelper.o: ../DaePhysicsSceneLibrary.h ../DaePhysicsScene.h
CMeshHelper.o: ../DaeAgeiaPhysicsScene.h ../DaeNativePhysicsScene.h
CMeshHelper.o: ../DaeSceneGraph.h ../DaeTextureLibrary.h
CMeshHelper.o: ../TranslatorHelpers/CShaderHelper.h CMeshHelper.h
CMeshHelper.o: CDagHelper.h ../CExportOptions.h
CSceneWalker.o: StdAfx.h ../StdAfx.h ../TranslatorHelpers/MayaHelper.h
CSceneWalker.o: ../FUtils/FUtils.h ../TranslatorHelpers/MayaConvert.h
CSceneWalker.o: ../DaeUtils.h ../DaeSceneElement.h ../DaeAnimationTypes.h
CSceneWalker.o: ../FUtils/DAESyntax.h ../DaeDoc.h
CSceneWalker.o: ../TranslatorHelpers/CSceneVisitor.h
CSceneWalker.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
CSceneWalker.o: CAnimationHelper.h ../DaeAnimationLibrary.h
CSceneWalker.o: ../DaeCameraLibrary.h ../DaeBaseLibrary.h
CSceneWalker.o: ../DaeControllerLibrary.h ../DaeGeometryLibrary.h
CSceneWalker.o: ../DaeLightLibrary.h ../DaeMaterialLibrary.h ../DaeInput.h
CSceneWalker.o: ../DaePhysicsMaterialLibrary.h ../DaePhysicsModelLibrary.h
CSceneWalker.o: ../DaePhysicsSceneLibrary.h ../DaePhysicsScene.h
CSceneWalker.o: ../DaeAgeiaPhysicsScene.h ../DaeNativePhysicsScene.h
CSceneWalker.o: ../DaeSceneGraph.h ../DaeTextureLibrary.h
CSceneWalker.o: ../TranslatorHelpers/CShaderHelper.h CSceneWalker.h
CSceneWalker.o: CSetHelper.h CDagHelper.h HIAnimCache.h ../CExportOptions.h
CSceneWalker.o: ../DaeDoc.h
CSetHelper.o: StdAfx.h ../StdAfx.h ../TranslatorHelpers/MayaHelper.h
CSetHelper.o: ../FUtils/FUtils.h ../TranslatorHelpers/MayaConvert.h
CSetHelper.o: ../DaeUtils.h ../DaeSceneElement.h ../DaeAnimationTypes.h
CSetHelper.o: ../FUtils/DAESyntax.h ../DaeDoc.h
CSetHelper.o: ../TranslatorHelpers/CSceneVisitor.h
CSetHelper.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
CSetHelper.o: CAnimationHelper.h ../DaeAnimationLibrary.h
CSetHelper.o: ../DaeCameraLibrary.h ../DaeBaseLibrary.h
CSetHelper.o: ../DaeControllerLibrary.h ../DaeGeometryLibrary.h
CSetHelper.o: ../DaeLightLibrary.h ../DaeMaterialLibrary.h ../DaeInput.h
CSetHelper.o: ../DaePhysicsMaterialLibrary.h ../DaePhysicsModelLibrary.h
CSetHelper.o: ../DaePhysicsSceneLibrary.h ../DaePhysicsScene.h
CSetHelper.o: ../DaeAgeiaPhysicsScene.h ../DaeNativePhysicsScene.h
CSetHelper.o: ../DaeSceneGraph.h ../DaeTextureLibrary.h
CSetHelper.o: ../TranslatorHelpers/CShaderHelper.h CSetHelper.h
CShaderHelper.o: StdAfx.h ../StdAfx.h ../TranslatorHelpers/MayaHelper.h
CShaderHelper.o: ../FUtils/FUtils.h ../TranslatorHelpers/MayaConvert.h
CShaderHelper.o: ../DaeUtils.h ../DaeSceneElement.h ../DaeAnimationTypes.h
CShaderHelper.o: ../FUtils/DAESyntax.h ../DaeDoc.h
CShaderHelper.o: ../TranslatorHelpers/CSceneVisitor.h
CShaderHelper.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
CShaderHelper.o: CAnimationHelper.h ../DaeAnimationLibrary.h
CShaderHelper.o: ../DaeCameraLibrary.h ../DaeBaseLibrary.h
CShaderHelper.o: ../DaeControllerLibrary.h ../DaeGeometryLibrary.h
CShaderHelper.o: ../DaeLightLibrary.h ../DaeMaterialLibrary.h ../DaeInput.h
CShaderHelper.o: ../DaePhysicsMaterialLibrary.h ../DaePhysicsModelLibrary.h
CShaderHelper.o: ../DaePhysicsSceneLibrary.h ../DaePhysicsScene.h
CShaderHelper.o: ../DaeAgeiaPhysicsScene.h ../DaeNativePhysicsScene.h
CShaderHelper.o: ../DaeSceneGraph.h ../DaeTextureLibrary.h
CShaderHelper.o: ../TranslatorHelpers/CShaderHelper.h CDagHelper.h
CShaderHelper.o: CShaderHelper.h
HIAnimCache.o: StdAfx.h ../StdAfx.h ../TranslatorHelpers/MayaHelper.h
HIAnimCache.o: ../FUtils/FUtils.h ../TranslatorHelpers/MayaConvert.h
HIAnimCache.o: ../DaeUtils.h ../DaeSceneElement.h ../DaeAnimationTypes.h
HIAnimCache.o: ../FUtils/DAESyntax.h ../DaeDoc.h
HIAnimCache.o: ../TranslatorHelpers/CSceneVisitor.h
HIAnimCache.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
HIAnimCache.o: CAnimationHelper.h ../DaeAnimationLibrary.h
HIAnimCache.o: ../DaeCameraLibrary.h ../DaeBaseLibrary.h
HIAnimCache.o: ../DaeControllerLibrary.h ../DaeGeometryLibrary.h
HIAnimCache.o: ../DaeLightLibrary.h ../DaeMaterialLibrary.h ../DaeInput.h
HIAnimCache.o: ../DaePhysicsMaterialLibrary.h ../DaePhysicsModelLibrary.h
HIAnimCache.o: ../DaePhysicsSceneLibrary.h ../DaePhysicsScene.h
HIAnimCache.o: ../DaeAgeiaPhysicsScene.h ../DaeNativePhysicsScene.h
HIAnimCache.o: ../DaeSceneGraph.h ../DaeTextureLibrary.h
HIAnimCache.o: ../TranslatorHelpers/CShaderHelper.h ../CExportOptions.h
HIAnimCache.o: HIAnimCache.h CDagHelper.h
MayaHelper.o: StdAfx.h ../StdAfx.h ../TranslatorHelpers/MayaHelper.h
MayaHelper.o: ../FUtils/FUtils.h ../TranslatorHelpers/MayaConvert.h
MayaHelper.o: ../DaeUtils.h ../DaeSceneElement.h ../DaeAnimationTypes.h
MayaHelper.o: ../FUtils/DAESyntax.h ../DaeDoc.h
MayaHelper.o: ../TranslatorHelpers/CSceneVisitor.h
MayaHelper.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
MayaHelper.o: CAnimationHelper.h ../DaeAnimationLibrary.h
MayaHelper.o: ../DaeCameraLibrary.h ../DaeBaseLibrary.h
MayaHelper.o: ../DaeControllerLibrary.h ../DaeGeometryLibrary.h
MayaHelper.o: ../DaeLightLibrary.h ../DaeMaterialLibrary.h ../DaeInput.h
MayaHelper.o: ../DaePhysicsMaterialLibrary.h ../DaePhysicsModelLibrary.h
MayaHelper.o: ../DaePhysicsSceneLibrary.h ../DaePhysicsScene.h
MayaHelper.o: ../DaeAgeiaPhysicsScene.h ../DaeNativePhysicsScene.h
MayaHelper.o: ../DaeSceneGraph.h ../DaeTextureLibrary.h
MayaHelper.o: ../TranslatorHelpers/CShaderHelper.h
CAnimationHelper.o: ../DaeAnimationTypes.h
HIAnimCache.o: CSceneVisitor.h CAnimationHelper.h ../DaeAnimationTypes.h
StdAfx.o: ../StdAfx.h ../TranslatorHelpers/MayaHelper.h ../FUtils/FUtils.h
StdAfx.o: ../TranslatorHelpers/MayaConvert.h ../DaeUtils.h
StdAfx.o: ../DaeSceneElement.h ../DaeAnimationTypes.h ../FUtils/DAESyntax.h
StdAfx.o: ../DaeDoc.h ../TranslatorHelpers/CSceneVisitor.h
StdAfx.o: ../TranslatorHelpers/HIAnimCache.h CSceneVisitor.h
StdAfx.o: CAnimationHelper.h ../DaeAnimationLibrary.h ../DaeCameraLibrary.h
StdAfx.o: ../DaeBaseLibrary.h ../DaeControllerLibrary.h
StdAfx.o: ../DaeGeometryLibrary.h ../DaeLightLibrary.h
StdAfx.o: ../DaeMaterialLibrary.h ../DaeInput.h
StdAfx.o: ../DaePhysicsMaterialLibrary.h ../DaePhysicsModelLibrary.h
StdAfx.o: ../DaePhysicsSceneLibrary.h ../DaePhysicsScene.h
StdAfx.o: ../DaeAgeiaPhysicsScene.h ../DaeNativePhysicsScene.h
StdAfx.o: ../DaeSceneGraph.h ../DaeTextureLibrary.h
StdAfx.o: ../TranslatorHelpers/CShaderHelper.h
