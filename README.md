INTRO:
Welcome to Jonathan Pincus's Text Editor Program.
This is a Text Editor program inspired by the GNU nano editor. This program is unique in that only the portion of file on the screen at any given moment is loaded in Memory while the leading and trailing unloaded portions are stored in files. This program supports the following features:
	-C syntax highlighting
	-Jump to line (int)
	-Jump to next occurrence of (string)
	-Highlight to copy and Paste


Please watch the demonstration video.

 
HOW TO USE:
(1) Install Docker, how this is done can be found here: https://docs.docker.com/engine/install/
(2) Download my package using the following command: docker pull ghcr.io/jonathan-git-hub-dev/texteditor:latest
(3) Create a container from the Image: docker container create -i -t --name test ghcr.io/jonathan-git-hub-dev/texteditor
(4) Run the container: docker start -i test
(5) Compile the Program : make
(6) Run Program: ./edit (file name of your choice)
(7) The shortcut manual can be found by holding CTRL and pressing H


TESTING:
-Testing
Some unit testing for the package functions can be found in the .c file of each package.
The main testing for this program is done in the form of a use case testing. test1.c and test2.c were created exclusively using this text editor and with the inclusion of testing of specific features I think it gives a pretty definative view of the flaws of the software.
here is a list of a specif features and sources of errors that were tested:
problematic file paths:
        - trying to create an new file in a nonexistant directory
        - saving file to nonexistant directory
        - save to file name of already existant file
        - opening file from a parent directory
	- File name too long
Changes of screen size:
        - users changes screen size
        - user changes screen size when in special input mode (eg: using CTRL-W)
        - user changes screen size while copying data
        - make sure when screen is too big/small program exits

Screen size too small or large on opening of program

Small files
        - move to line technically on screen but not accessable becuase file is too short

Improper file format
        - test on file that is not '\n' terminated

line too long
        - original final has line too long
        - delete merge results in a line too long
        - backspace merge results in line too long
        - paste results in line too long

File name too long to fit in header

number of lines in file is longer then size_t used for indexing

highlighting:
        -is highlighting done properly when multiple different elements


CLOSING STATEMENT:
through the testing of the program i have found that it to be almost error free. The error I found which I fixed is one that treats an arrow press as sperate inputs constisting of (ESC + [ + a letter) which would occur about one in every five hundred inputs. the patch seems to be working but to truley establish its effectiveness would require testing i am not willing to do considering that i would like to start my next project.
in conclusion the text editor fulfills my intial requirements.

thank you for your time.
