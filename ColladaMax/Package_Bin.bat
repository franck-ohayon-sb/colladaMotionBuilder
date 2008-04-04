rem Use this batch file to package up a new version of the FS import classes

rem Clean up the old version package files, if present
rm -f ColladaMax.zip
rm -R -f Package_Temp

rem Copy all the interesting files: DLL, LIB, H, TXT over
rem to a temporary folder
mkdir Package_Temp
mkdir Package_Temp\3dsmax7
mkdir Package_Temp\3dsmax8
copy "Output\Release 7\ColladaMax\ColladaMax.dle" Package_Temp\3dsmax7
copy "Output\Release 8\ColladaMax\ColladaMax.dle" Package_Temp\3dsmax8
copy *.doc Package_Temp
copy *.txt Package_Temp

rem Zip up the temporary directory
cd Package_Temp
zip -9 -r ..\ColladaMax.zip .
cd ..

rem Clean up
rm -R -f Package_Temp
