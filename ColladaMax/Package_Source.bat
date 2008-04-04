rem Use this batch file to package up a new version of ColladaMax

rem Clean up the old version package files, if present
rm -f ColladaMax_Source.zip
rm -R -f ColladaMax

rem Copy all the interesting files: DLL, LIB, H, TXT over
rem to a temporary folder
mkdir ColladaMax
mkdir ColladaMax\IMorpher
copy "IMorpher\*.h" ColladaMax\IMorpher
copy "IMorpher\*.lib" ColladaMax\IMorpher
copy *.cpp ColladaMax
copy *.h ColladaMax
copy *.rc ColladaMax
copy *.vcproj ColladaMax
copy *.sln ColladaMax
copy *.def ColladaMax
copy *.txt ColladaMax

rem Build the FCollada source zip file, unzip it and include that.
cd ..\FCollada
call Package_Source.bat
cd ..\ColladaMax
mkdir FCollada
unzip ..\FCollada\FCollada_Source.zip -d FCollada

rem Zip up the temporary directory
zip -9 -r ColladaMax_Source.zip ColladaMax
zip -9 -r ColladaMax_Source.zip FCollada
zip -9 -r ColladaMax_Source.zip *.doc

rem Clean up
rm -R -f ColladaMax
rm -R -f FCollada
