INTRO:
Welcome to Jonathan Pincus's Text Editor Program.
This is a Text Editor program inspired by the GNU nano editor. This program is unique in that only the portion of file on the screen at any given moment is loaded in Memory while the leading and trailing unloaded portions are stored in files. This program supports the following features:
	-C syntax highlighting
	-Find line by index
	-Jump to next occurrence of a string
	-Highlight to copy and Paste


Please watch the demonstration video!

 
HOW TO USE:
(1) Install Docker, how this is done can be found here: https://docs.docker.com/engine/install/
(2) Download my package using the following command: docker pull ghcr.io/jonathan-git-hub-dev/texteditor:latest
(3) Create a container from the Image: docker container create -i -t --name test ghcr.io/jonathan-git-hub-dev/texteditor
(4) Run the container: docker start -i test
(5) Compile the Program : make
(6) Run Program: ./edit (file name of your choice)
(7) The manual can be found by holding CTRL and pressing H


TESTING:
Some unit testing for the package functions can be found in the .c file of each package.
The main testing for this program is done in the form of use case testing. test1.c (implementation of Linux “ls” command) and test2.c (implementation of a Hash Map) were created exclusively using this text editor and with the inclusion of testing of specific problematic areas of the program I think it gives a somewhat definitive view of if there are bugs in the software.
Here is a list of a specific features and sources of errors that were deliberately tested, which displayed the same behaviour as CNU nano :
Errors resulting from file paths:
        - Trying to create a new file in a non-existent directory
        - Saving file to non-existent directory
        - Saving to file that already exists
        - Opening file from a parent directory
        - File name too long
Errors caused by a change in screen size:
        - Users changes screen size
        - User changes screen size when in special input mode (e.g. using CTRL-W)
        - User changes screen size while copying data
Small file specific Errors:
        - Using CTRL-G to move to line technically on screen but a line that should not be accessible because file ends before it
Improper file format:
        - Test on file that is not '\n' terminated
Line buffer Errors:
        - Original final contains larger than buffer
        - Deleting results in merge of lines that eclipses buffer
        - Backspace merge results in line longer than buffer
        - Paste results in line longer than buffer
File name too long to fit in header
Number of lines in file is longer then Size_t creating overflow error
Syntax highlighting incorrect


CLOSING STATEMENT:
Through the testing of the program, I have found it to be almost error free. The error I found and fixed is one that occasionally treats an arrow press as sperate inputs consisting of (ESC + [ + a letter) which would occur about one in every five hundred inputs. the patch seems to be working but to truly establish its effectiveness would require testing I am not willing to do considering that I would like to start my next project.
In conclusion the text editor fulfills my initial requirements.

Thank you for your time.
