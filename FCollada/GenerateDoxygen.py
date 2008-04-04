import os

# Run the Doxygen application to generate the documentation.
os.system("rd /q/s \"Documentation\"")
os.system("mkdir \"Documentation\"")
os.system("doxygen.exe Doxyfile")

# Generate a second index file which simply points to the inner one.
indexFile = open("Documentation/FCollada.html", "w")
indexFile.write("<title>FCollada Documentation</title>")
indexFile.write("<frameset><frame src=\"html/index.html\"></frameset>")
indexFile.close()

# Open the Doxygen log file to users so they know what's missing.
os.system("write \"Output/Doxygen_log.txt\"")
