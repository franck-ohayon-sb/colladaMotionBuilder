import sys
import os
import os.path

# State-machine to parse and fix-up a TortoiseSVN changelog.
# USAGE: ChangeLogs <Folder> <start_revision>
# example: ChangeLogs FCollada 5201

folder = sys.argv[1];
start_revision = sys.argv[2];

os.system("svn log -r HEAD:" + start_revision + " " + folder + " > _CL.txt");
f = open("_CL.txt", "r+")

fullText = "";
line = "S";
while (line):

	# Start with a nice line: -----..
	line = f.readline()
	start = line.find("---------------------------------------")
	if (start < 0): continue
		
	# Next line has the author information.
	line = f.readline()
	start = line.find("|")
	if (start < 0): continue
	header = "[" + line[1:start-1] + " by " # retrieve the revision number
	line = line[start+2:-1] # skip the revision number
	start = line.find("|")
	if (start < 0): continue
	header = header + line[0:start-1] + "] " # retrieve the author's name
	# Skip the rest of this line

	newLine = header;

	# Finally, we have the message. We'll take out the newlines and most whitespaces
	firstLine = 1;
	while (line):
		line = f.readline()
		if (line.find("--------------------") >= 0): break

		# Try to conform all the messages.
		copyStart = 0;

		# Remove whitespaces are nasty list characters
		while (line[copyStart] == " "): copyStart = copyStart + 1
		while (line[copyStart] == "-" or line[copyStart] == "+"): copyStart = copyStart + 1
		while (line[copyStart] == " "): copyStart = copyStart + 1
		if (line[copyStart] == "\n"): continue
			
		if (firstLine == 1):
			if (line[copyStart] >= 'a' and line[copyStart] <= 'z'):
				line[copyStart].upper();
			firstLine = 0;
		
		newLine = newLine + line[copyStart:-1] + " "
		if (len(newLine) > 1024):
			fullText += newLine + "\n"
			newLine = header
	
	fullText += newLine + "\n"

f.close()

f = open("_CL.txt", "w")
f.write(fullText)
f.close()

os.system("notepad _CL.txt");
os.remove("_CL.txt");