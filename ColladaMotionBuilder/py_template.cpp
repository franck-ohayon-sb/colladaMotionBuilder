
#include "StdAfx.h"
#include "py_common.h"

//--- Python module initialization functions.
BOOST_PYTHON_MODULE(ColladaMotionBuilder) //! Must exactly match the name of the module,usually it's the same name as $(TargetName).
{
	MobuColladaExporterInit();
}
//! Set the target extension to ".pyd" on property page of project, Or
//! Use post-build event "copy $(TargetPath)  ..\..\..\..\bin\$(PlatformName)\plugins\$(TargetName).pyd"
//! to rename the module by file extension “.pyd” into python directory to make sure the module could be found.

