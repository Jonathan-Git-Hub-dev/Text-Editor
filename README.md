INTRO:
Welcome to Jonathan Pincus's Text Editor Program.
This is a Text Editor program inspired by the GNU nano editor. This program is unique in that only the portion of file on the screen at any given moment is loaded in Memory while the leading and trailing unloaded portions are stored in files. This program supports the following features:
	-C syntax highlighting
	-Jump to line (int)
	-Jump to next occurrence of (string)
	-Highlight to copy and Paste

 
HOW TO USE:
(1) Install Docker, how this is done can be found here: https://docs.docker.com/engine/install/
(2) Download my package using the following command: docker pull ghcr.io/jonathan-git-hub-dev/texteditor:latest
(3) Create a container from the Image: docker container create -i -t --name test ghcr.io/jonathan-git-hub-dev/texteditor
(4) Run the container: docker start -i test
(5) Compile the Program : make
(6) Run Program: ./edit (file name of your choice)
(7) If help is needed hold CTRL and press H
