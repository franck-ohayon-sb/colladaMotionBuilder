sudo rm /Users/Shared/Autodesk/maya/2008/plug-ins/COLLADA.bundle
sudo cp Output/Release/COLLADA_m2008.bundle /Users/Shared/Autodesk/maya/2008/plug-ins/COLLADA.bundle
sudo chmod 777 /Users/Shared/Autodesk/maya/2008/plug-ins/COLLADA.bundle
sudo chmod 644 scripts/*
sudo cp scripts/AE* /Applications/Autodesk/maya2008/Maya.app/Contents/scripts/AETemplates/
sudo cp scripts/collada* /Applications/Autodesk/maya2008/Maya.app/Contents/scripts/others/
sudo cp scripts/*.xpm /Applications/Autodesk/maya2008/Maya.app/Contents/icons/
