HoverRace
=========

<http://www.hoverrace.com/>

HoverRace is an online racing game originally written by Grokksoft in the mid-1990s. After Grokksoft stopped maintaining the game in the late 1990s, HoverRace was abandonware for a number of years before the original developer, Richard Langlois, opened up the source code to the public. Since then, development has been ongoing to bring HoverRace into the 21st century.

Features
--------

 * Fast, free, and fun!
 * Single-player and multiplayer (split-screen and internet).
 * Hundreds of user-created tracks available for download.

Portability
-----------

HoverRace currently runs on Windows, but the code is slowly being rewritten to be portable.  A single-player proof-of-concept runs on Linux.

Links
-----

Download and play the latest release: <http://www.hoverrace.com/>

Project hosted at GitHub: <https://github.com/HoverRace/HoverRace/>

Source documentation: <http://hoverrace.github.com/API/>

HoverRace wiki: <https://github.com/HoverRace/HoverRace/wiki>


Compilation Instructions
-----------

Installation of Visual Studio 2019 with the correct components (Read all steps  for VS installation first). 

1. Download the Visual Studio 2019 installer from Microsoft

2. During the install, when the workloads selctor comes up, make sure you include the Desktop C++ workloads from the workloads selector

3. IMPORTANT: Select the 'individual components' tab. Type 'MFC' into the searchbox and make sure you check C++ v14.23 MFC for v142 build tools (x86 & x64) 

4. Click 'install' and let Visual Studio install itself


Installing Boost Libraries (Required by the HoverRace Sourcecode)

1. Download Install the Boost Library Binaries. At time of writing I used the boost_1_72_0-msvc-14.2-32.exe installer from https://sourceforge.net/projects/boost/files/boost-binaries/1.72.0/boost_1_72_0-msvc-14.2-32.exe/download


Opening the HoverRace Classic sourcecode in Visual Studio and Configure it:

1. Start Visual Studio 2019 by clicking it's icon
2. Select to clone or checkout code
3. Paste https://github.com/johnsie/HoverNet.git in as the url
4. Download the project and in the  "Team Explorer" window click the 'Home' icon open the "hoverrace.sln" which should appear in that Window 
5. Go to View->Other Windows and click "Property Manager"
6. When the Property Manager window comes up, select all of the projects by clicking the top one and using the shift button to select all 7 projects in the solution, right click and click "Properties"
7. When the "Property Pages" window comes up, go to "VC++ Directories" using the menu on the left side of that window
8. Click the dropdown for "Include Directories" and "edit". Add a new line using the new folder icon. This new line should be the location where boost was installed. Eg. c:\local\boost_1_72_0. Press Ok
9. Addd an entry into "Libraries Directories" using the dropdown for that and "edit". This time the new line should be the location of the boost lib files which should be something like c:\local\boost_1_72_0\lib32-mscv-14.2. Hit ok.
10. IMPORTANT: Click the 'Apply' button on the Property Pages window before clicking ok, otherwise it will forget your settings and you'll have to do it again.

Congratualtions you've set everything up for debug mode. Now for Release mode:

1. At the top of the Visual Studio you'll see a dropdown that says 'Debug'. Click that and select 'Release'
2. Repeat steos 6 to 9 from above to set the Boost libary directories again
3. On the left of the Property Pages Window select 'Linker->Advanced->Inage Has Safe Exception Hanlders' and set that to No.
4. Click "Apply" and "Ok"

Now you've set up the Boost Libraries in both the Debug and Release versions

HoverRace should be ready to compile. Any issues or queries please get in touch. Happy Coding!!!


