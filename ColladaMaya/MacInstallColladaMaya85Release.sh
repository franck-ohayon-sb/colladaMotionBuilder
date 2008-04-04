sudo rm /Users/Shared/Autodesk/maya/8.5/plug-ins/COLLADA.bundle
sudo cp Output/Release/COLLADA_m85.bundle /Users/Shared/Autodesk/maya/8.5/plug-ins/COLLADA.bundle
sudo chmod 777 /Users/Shared/Autodesk/maya/8.5/plug-ins/COLLADA.bundle
sudo chmod 644 scripts/*
sudo cp scripts/AE* /Applications/Autodesk/maya8.5/Maya.app/Contents/scripts/AETemplates/
sudo cp scripts/collada* /Applications/Autodesk/maya8.5/Maya.app/Contents/scripts/others/
sudo cp scripts/*.xpm /Applications/Autodesk/maya8.5/Maya.app/Contents/icons/
