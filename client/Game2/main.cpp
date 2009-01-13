// main.cpp              Fireball game entry point
//
// Copyright (c) 1995-1998 - Richard Langlois and Grokksoft Inc.
//
// Licensed under GrokkSoft HoverRace SourceCode License v1.0(the "License");
// you may not use this file except in compliance with the License.
//
// A copy of the license should have been attached to the package from which
// you have taken this file. If you can not find the license you can not use
// this file.
//
//
// The author makes no representations about the suitability of
// this software for any purpose.  It is provided "as is" "AS IS",
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
//
// See the License for the specific language governing permissions
// and limitations under the License.
//
#include "StdAfx.h"

#ifdef _WIN32
#include "GameApp.h"
#endif

#ifdef _WIN32
#	include <direct.h>
#endif

#include <iostream>

#include <curl/curl.h>

// Entry point
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE pInstance, HINSTANCE pPrevInstance, LPSTR /* pCmdLine */ , int pCmdShow)
#else
int main(int argc, char** argv)
#endif
{
	// initialize return variables
	BOOL lReturnValue = TRUE;
	int lErrorCode = EXIT_SUCCESS;

#ifdef _WIN32
	char exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH - 1);

	// Change the working directory to the app's directory.
	char *appPath = strdup(exePath);
	char *appDiv = strrchr(appPath, '\\');
	*appDiv = '\0';
	chdir(appPath);
	free(appPath);
#endif

	// Gettext initialization.
	setlocale(LC_ALL, "");
	bindtextdomain("hoverrace", "../share/locale");
	textdomain("hoverrace");

	// Library initialization.
	curl_global_init(CURL_GLOBAL_ALL);

#ifdef _WIN32
	MR_GameApp lGame(pInstance);

	// Allow only one instance of HoverRace; press CAPS_LOCK to bypass
	GetAsyncKeyState(VK_CAPITAL);				  // Reset the function
	if(!GetAsyncKeyState(VK_CAPITAL))
		lReturnValue = lGame.IsFirstInstance();

	if(lReturnValue && (pPrevInstance == NULL))
		lReturnValue = lGame.InitApplication();

	if(lReturnValue)
		lReturnValue = lGame.InitGame();

	// this is where the game actually takes control
	if(lReturnValue)
		lErrorCode = lGame.MainLoop();
#else
	std::cout << boost::format(_("HoverRace for Linux is under development!\n"
		"Please visit %s to learn how to\n"
		"contribute to this project.")) % "http://svn.igglybob.com/hoverrace/" <<
		std::endl;
#endif

	// Library cleanup.
	curl_global_cleanup();

	return lErrorCode;
}
