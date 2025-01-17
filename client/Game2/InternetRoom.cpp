// InternetRoom.cpp
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

#include <richedit.h>
 
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "../../engine/Model/Track.h"
#include "../../engine/Util/Config.h"
#include "../../engine/Util/Str.h"

#include "InternetRoom.h"
#include "MatchReport.h"
#include "SelectRoomDialog.h"
#include "CheckUpdateServerDialog.h"
#include "Rulebook.h"
#include "TrackDownloadDialog.h"
#include "resource.h"

#define MRM_DNS_ANSWER        (WM_USER + 1)
#define MRM_NET_EVENT         (WM_USER + 7)
#define MRM_DLG_END_ADD       (WM_USER + 10)
#define MRM_DLG_END_JOIN      (WM_USER + 11)

#define MRM_BIN_BUFFER_SIZE    25000			  // 25 K this is BIG enough

#define REFRESH_DELAY        200
#define REFRESH_TIMEOUT    11000
#define OP_TIMEOUT         22000
#define FAST_OP_TIMEOUT     6000
#define CHAT_TIMEOUT       18000
#define SCORE_OP_TIMEOUT   12000

#define IMMEDIATE                 1
#define REFRESH_EVENT             1
#define REFRESH_TIMEOUT_EVENT     2
#define CHAT_TIMEOUT_EVENT        3
#define OP_TIMEOUT_EVENT          4

#define LOAD_BANNER_TIMEOUT_EVENT     8
#define ANIM_BANNER_TIMEOUT_EVENT     9

#define MR_IR_LIST_PORT 80

// #endif

#define MR_MAX_SERVER_ENTRIES  12
#define MR_MAX_BANNER_ENTRIES  10

#define MR_HTTP_SCORE_SERVER     0
#define MR_HTTP_ROOM_SERVER      1
#define MR_HTTP_LADDER_ROOM      2
#define MR_NREG_BANNER_SERVER    8
#define MR_REG_BANNER_SERVER     9

using namespace HoverRace;
using namespace HoverRace::Parcel;
using namespace HoverRace::Util;

namespace HoverRace {
namespace Client {

InternetRoom *InternetRoom::mThis = NULL;

static std::string MR_Pad(const char *pSrc);
static std::string GetLine(const char *pSrc);
static int GetLineLen(const char *pSrc);
static const char *GetNextLine(const char *pSrc);

static int FindFocusItem(HWND pWindow);

// InternetRequest

InternetRequest::InternetRequest()
{
	mSocket = INVALID_SOCKET;
	mBinMode = FALSE;
	mBinBuffer = NULL;
	mBinIndex = 0;
}

InternetRequest::~InternetRequest()
{
	Close();

	if(mBinBuffer != NULL) {
		delete[] mBinBuffer;
	}
}

void InternetRequest::SetBin()
{
	mBinMode = TRUE;

	if(mBinBuffer == NULL) {
		mBinBuffer = new char[MRM_BIN_BUFFER_SIZE];
	}
}

void InternetRequest::Close()
{
	if(mSocket != INVALID_SOCKET) {
		closesocket(mSocket);
		mSocket = INVALID_SOCKET;
	}
}

BOOL InternetRequest::Working() const
{
	return (mSocket != INVALID_SOCKET);
}

void InternetRequest::Clear()
{
	Close();
	mBuffer.clear();
	mRequest.clear();
	mBinIndex = 0;

}

BOOL InternetRequest::Send(HWND pWindow, unsigned long pIP, unsigned int pPort, const char *pURL, const char *pCookie)
{
	BOOL lReturnValue = FALSE;

	if(!Working()) {
		Clear();

		mStartTime = time(NULL);

		const char *lURL = strchr(pURL, '/');
		char lReqBuffer[1024];

		if((lURL == NULL) || mBinMode) {
			lURL = pURL;
		}

		const std::string &ua = Config::GetInstance()->GetUserAgentId();
		if(pCookie == NULL) {
			sprintf(lReqBuffer, "GET %s HTTP/1.0\r\n"
				"Accept: */*\r\n"
				"User-Agent: %s\r\n"
				"\r\n" "\r\n",
				lURL, ua.c_str());
		}
		else {
			sprintf(lReqBuffer, "GET %s HTTP/1.0\r\n"
				"Accept: */*\r\n"
				"User-Agent: %s\r\n"
				"Cookie: %s\r\n"
				"\r\n" "\r\n",
				lURL, ua.c_str(), pCookie);
		}

		mRequest = lReqBuffer;

		mSocket = socket(PF_INET, SOCK_STREAM, 0);

		ASSERT(mSocket != INVALID_SOCKET);

		SOCKADDR_IN lAddr;

		lAddr.sin_family = AF_INET;
		lAddr.sin_addr.s_addr = htonl(pIP);
		lAddr.sin_port = htons((unsigned short) pPort);

		WSAAsyncSelect(mSocket, pWindow, MRM_NET_EVENT, FD_CONNECT | FD_READ | FD_CLOSE);

		connect(mSocket, (struct sockaddr *) &lAddr, sizeof(lAddr));

		lReturnValue = TRUE;
	}
	return lReturnValue;
}

BOOL InternetRequest::ProcessEvent(WPARAM pWParam, LPARAM pLParam)
{
	// static variables required to patch E-On/ICE protocol

	BOOL lReturnValue = FALSE;

	if(Working() && (pWParam == mSocket)) {
		lReturnValue = TRUE;

		switch (WSAGETSELECTEVENT(pLParam)) {
			case FD_CONNECT:
				// We are now connected, send the request
					{
						send(mSocket, mRequest.c_str(), mRequest.length(), 0);
					}
					mRequest = "";
					break;

			case FD_READ:
			case FD_CLOSE:

				int lNbRead;

				if(mBinMode) {
					if(mBinIndex >= MRM_BIN_BUFFER_SIZE) {
						Close();
					}
					else {
						lNbRead = recv(mSocket, mBinBuffer + mBinIndex, MRM_BIN_BUFFER_SIZE - mBinIndex, 0);

						if(lNbRead >= 0) {
							mBinIndex += lNbRead;
						}
						else {
							Close();
						}
					}

				}
				else {
					char lReadBuffer[1025];

					lNbRead = recv(mSocket, lReadBuffer, 1024, 0);

					if(lNbRead > 0) {
						lReadBuffer[lNbRead] = 0;

						{
							mBuffer += lReadBuffer;
						}
					}
				}
	
				if(WSAGETSELECTEVENT(pLParam) == FD_CLOSE) {
					Close();
				}
				break;

		}
	}

	return lReturnValue;

}

const char *InternetRequest::GetBuffer() const
{
	return mBuffer.c_str();
}

const char *InternetRequest::GetBinBuffer(int &pSize) const
{
	pSize = mBinIndex;
	return mBinBuffer;
}

BOOL InternetRequest::IsReady() const
{
	return (((mBinIndex != 0) || !mBuffer.empty()) && !Working());
}

// InternetRoom

InternetRoom::InternetRoom(const std::string &pMainServer, bool mustCheckUpdates) :
	mMainServer(pMainServer), chatLog(NULL),
	lastMessageReceivedSoundTs(0), checkUpdates(mustCheckUpdates)
{
	int lCounter;

	mBannerRequest.SetBin();

	for(lCounter = 0; lCounter < eMaxClient; lCounter++) {
		mClientList[lCounter].mValid = FALSE;
	}

	for(lCounter = 0; lCounter < eMaxGame; lCounter++) {
		mGameList[lCounter].mValid = FALSE;
	}

	mCurrentLocateRequest = NULL;
	mModelessDlg = NULL;

	// Init WinSock
	WORD lVersionRequested = MAKEWORD(1, 1);
	WSADATA lWsaData;

	if(WSAStartup(lVersionRequested, &lWsaData)) {
		ASSERT(FALSE);
	}

	mNbSuccessiveRefreshTimeOut = 0;
	mCurrentBannerIndex = 0;

}

InternetRoom::~InternetRoom()
{
	// Close WinSock
	WSACleanup();

	if (chatLog != NULL) {
		chatLog->close();
		delete chatLog;
	}

	ASSERT(mModelessDlg == NULL);
}

int InternetRoom::ParseState(const char *pAnswer)
{
	int lReturnValue = 0;
	const char *lLinePtr;

	lLinePtr = pAnswer;

	while(lLinePtr != NULL) {
		if(!strncmp(lLinePtr, "TIME_STAMP", 10)) {
			sscanf(lLinePtr, "TIME_STAMP %d", &mLastRefreshTimeStamp);
		}
		else if(!strncmp(lLinePtr, "USER", 4)) {
			int lEntry;
			char lOp[10];

			if(sscanf(lLinePtr, "USER %d %9s", &lEntry, lOp) == 2) {
				if((lEntry >= 0) && (lEntry < eMaxClient)) {
					if(!strcmp(lOp, "DEL")) {
						lReturnValue |= eUsersModified;
						mClientList[lEntry].mValid = FALSE;
					}
					else if(!strcmp(lOp, "NEW")) {
						lLinePtr = GetNextLine(lLinePtr);

						if(lLinePtr != NULL) {
							lReturnValue |= eUsersModified;

							mClientList[lEntry].mMajorID = -1;
							mClientList[lEntry].mMinorID = -1;

							sscanf(GetLine(lLinePtr).c_str(), "%d-%d", &mClientList[lEntry].mMajorID, &mClientList[lEntry].mMinorID);

							mClientList[lEntry].mValid = TRUE;
							mClientList[lEntry].mGame = -1;

							lLinePtr = GetNextLine(lLinePtr);

							mClientList[lEntry].mName = GetLine(lLinePtr);
						}
					}
				}
			}
		}
		else if(!strncmp(lLinePtr, "GAME", 4)) {
			int lEntry;
			char lOp[10];
			int lId = -1;

			if(sscanf(lLinePtr, "GAME %d %9s %u", &lEntry, lOp, &lId) >= 2) {
				if((lEntry >= 0) && (lEntry < eMaxGame)) {
					if(!strcmp(lOp, "DEL")) {
						lReturnValue |= eGamesModified;
						mGameList[lEntry].mValid = FALSE;
					}
					else if(!strcmp(lOp, "NEW")) {
						lLinePtr = GetNextLine(lLinePtr);
					//	MessageBox(0, "New", "MessageBox caption", MB_OK);
						if(lLinePtr != NULL) {
							lReturnValue |= eGamesModified;

							mGameList[lEntry].mValid = TRUE;
							mGameList[lEntry].mId = lId;
							mGameList[lEntry].mNbClient = 0;
							mGameList[lEntry].mNbLap = 1;
							mGameList[lEntry].mAllowWeapons = FALSE;
							mGameList[lEntry].mPort = (unsigned) -1;
							mGameList[lEntry].mName = GetLine(lLinePtr);

							lLinePtr = GetNextLine(lLinePtr);
							mGameList[lEntry].mTrack = GetLine(lLinePtr);
							mGameList[lEntry].mAvailCode = Config::GetInstance()->GetTrackBundle()->CheckAvail(mGameList[lEntry].mTrack.c_str());

							lLinePtr = GetNextLine(lLinePtr);
							mGameList[lEntry].mIPAddr = GetLine(lLinePtr);

							lLinePtr = GetNextLine(lLinePtr);

							int lNbClient;
							int lDummyBool;
							string lOptAllowWeapons;
							string lOptAllowMines;
							string lOptAllowCans;
							string lOptAllowBasic;
							string lOptAllowBi;
								string lOptAllowCX;
								string lOptAllowEON;
							if(sscanf(lLinePtr, "%u %d %d %d %d %d %d %d %d %d %d %d", &mGameList[lEntry].mPort, &mGameList[lEntry].mNbLap, &lDummyBool, &lNbClient, 
								&lOptAllowWeapons,
							 &lOptAllowMines,
							 &lOptAllowCans,
						 &lOptAllowBasic,
							 &lOptAllowBi,
							 &lOptAllowCX,
							&lOptAllowEON) == 11) {
							
								
								/* just for reference
								((lGameOpts & OPT_ALLOW_WEAPONS) ? 'W' : '_') %
									((lGameOpts & OPT_ALLOW_MINES) ? 'M' : '_') %
									((lGameOpts & OPT_ALLOW_CANS) ? 'C' : '_') %
									((lGameOpts & OPT_ALLOW_BASIC) ? 'B' : '_') %
									((lGameOpts & OPT_ALLOW_BI) ? '2' : '_') %
									((lGameOpts & OPT_ALLOW_CX) ? 'C' : '_') %
									((lGameOpts & OPT_ALLOW_EON) ? 'E' : '_'));
									*/
								
								//old way
								mGameList[lEntry].mAllowWeapons = lDummyBool;
								

								//We are using strings in to set game options
								//new way
								mGameList[lEntry].jAllowWeapons = lOptAllowWeapons;

								mGameList[lEntry].mOptAllowMines = lOptAllowMines;
								mGameList[lEntry].mOptAllowCans = lOptAllowCans;
								mGameList[lEntry].mOptAllowBasic = lOptAllowBasic;
								mGameList[lEntry].mOptAllowBi = lOptAllowBi;
								mGameList[lEntry].mOptAllowCX = lOptAllowCX;
								mGameList[lEntry].mOptAllowEON = lOptAllowEON;


								if(lNbClient > eMaxPlayerGame) {
									lNbClient = eMaxPlayerGame;
								}

								if(lNbClient != 0) {
									lLinePtr = GetNextLine(lLinePtr);

									if(lLinePtr != NULL) {
										const char *lPtr = lLinePtr;

										for(int lCounter = 0; lCounter < lNbClient; lCounter++) {
											if(lPtr != NULL) {
												int lUserIndex = atoi(lPtr);

												if((lUserIndex >= 0) && (lUserIndex < eMaxClient)) {
													mGameList[lEntry].mClientList[mGameList[lEntry].mNbClient++] = lUserIndex;

													mClientList[lUserIndex].mGame = lEntry;
												}
												lPtr = strchr(lPtr, ' ');

												if(lPtr != NULL) {
													lPtr++;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}

		}
		else if(!strncmp(lLinePtr, "CHAT", 4)) {
			// Next line is a chat message
			lLinePtr = GetNextLine(lLinePtr);

			if(lLinePtr != NULL) {
				lReturnValue |= eChatModified;
				AddChatLine(GetLine(lLinePtr).c_str());
			}
		}

		lLinePtr = GetNextLine(lLinePtr);
	}

	return lReturnValue;

}

BOOL InternetRoom::AddUserOp(HWND pParentWindow)
{
	BOOL lReturnValue = FALSE;

	mThis = this;

	mNetOpString = _("Connecting to the Internet Meeting Room...");

	mNetOpRequest = boost::str(boost::format("%s?=ADD_USER%%%%%d-%d%%%%1%%%%%u%%%%%u%%%%%s") %
		roomList->GetSelectedRoom()->path %
		-1 % -2 % 0 % 0 %
		MR_Pad(mUser.c_str()).c_str());

	lReturnValue = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NET_PROGRESS), pParentWindow, NetOpCallBack) == IDOK;

	if(lReturnValue) {
		const char *lData = mOpRequest.GetBuffer();

		while((lData != NULL) && strncmp(lData, "SUCCESS", 7)) {
			lData = GetNextLine(lData);
		}
	if(lData == NULL) {
			ASSERT(FALSE);
			lReturnValue = FALSE;
		}
		else {
			lData = GetNextLine(lData);

			sscanf(lData, "USER_ID %d-%u", &mCurrentUserIndex, &mCurrentUserId);

			AddChatLine(_("You are connected"));
			AddChatLine(_("Welcome to the HoverRace Internet Meeting Room"));

			ParseState(lData);
	}
	}

	mOpRequest.Clear();

	return lReturnValue;
}

BOOL InternetRoom::DelUserOp(HWND pParentWindow, BOOL pFastMode)
{
	BOOL lReturnValue = FALSE;

	mThis = this;

	mNetOpString = _("Disconnecting from the Internet Meeting Room...");

	mNetOpRequest = boost::str(boost::format("%s?=DEL_USER%%%%%d-%u") %
		roomList->GetSelectedRoom()->path %
		//(const char *) gServerList[gCurrentServerEntry].mURL,
		mCurrentUserIndex % mCurrentUserId);

	lReturnValue = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NET_PROGRESS), pParentWindow, pFastMode ? FastNetOpCallBack : NetOpCallBack) == IDOK;

	mOpRequest.Clear();

	return lReturnValue;
}






BOOL InternetRoom::AddGameOp(HWND pParentWindow, const char *pGameName, const char *pTrackName, int pNbLap, char pGameOpts, unsigned pPort)
{
	BOOL lReturnValue = FALSE;

	mThis = this;

	//std::string s(20,pGameOpts);





	mNetOpString = _("Registering game with the Internet Meeting Room...");



	mNetOpRequest = boost::str(boost::format("%s?=ADD_GAME%%%%%d-%u%%%%%s%%%%%s%%%%%d%%%%J%%%%%d%%%%%d%%%%%d%%%%%d%%%%%d%%%%%d%%%%%d%%%%%d") %
		roomList->GetSelectedRoom()->path %
		//(const char *) gServerList[gCurrentServerEntry].mURL,
		mCurrentUserIndex % mCurrentUserId % MR_Pad(pGameName) %
		MR_Pad(pTrackName) % pNbLap % 
	    pPort %
		((pGameOpts & OPT_ALLOW_WEAPONS) ? 'W' : '_') %
		((pGameOpts & OPT_ALLOW_MINES) ? 'M' : '_') %
		((pGameOpts & OPT_ALLOW_CANS) ? 'C' : '_') %
		((pGameOpts & OPT_ALLOW_BASIC) ? 'B' : '_') %
		((pGameOpts & OPT_ALLOW_BI) ? '2' : '_') %
		((pGameOpts & OPT_ALLOW_CX) ? 'C' : '_') %
		((pGameOpts & OPT_ALLOW_EON) ? 'E' : '_')
	);

	lReturnValue = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NET_PROGRESS), pParentWindow, NetOpCallBack) == IDOK;

	if(lReturnValue) {
		const char *lData = mOpRequest.GetBuffer();

		while((lData != NULL) && strncmp(lData, "SUCCESS", 7)) {
			lData = GetNextLine(lData);
		}
		if(lData == NULL) {
			ASSERT(FALSE);
			lReturnValue = FALSE;
		}
		else {
			lData = GetNextLine(lData);

			sscanf(lData, "GAME_ID %d-%u", &mCurrentGameIndex, &mCurrentGameId);
		}
	}

	mOpRequest.Clear();

	return lReturnValue;
}

BOOL InternetRoom::DelGameOp(HWND pParentWindow)
{
	BOOL lReturnValue = FALSE;

	mThis = this;

	mNetOpString = _("Unregistering game from the Internet Meeting Room...");

	mNetOpRequest = boost::str(boost::format("%s?=DEL_GAME%%%%%d-%u%%%%%d-%u") %
		roomList->GetSelectedRoom()->path %
		//(const char *) gServerList[gCurrentServerEntry].mURL,
		mCurrentGameIndex % mCurrentGameId % mCurrentUserIndex % mCurrentUserId);

	lReturnValue = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NET_PROGRESS), pParentWindow, NetOpCallBack) == IDOK;

	mOpRequest.Clear();

	return lReturnValue;
}

BOOL InternetRoom::JoinGameOp(HWND pParentWindow, int pGameIndex)
{
	BOOL lReturnValue = FALSE;

	mThis = this;

	mNetOpString = _("Registering with the Internet Meeting Room...");

	mCurrentGameIndex = pGameIndex;
	mCurrentGameId = mGameList[pGameIndex].mId;

	mNetOpRequest = boost::str(boost::format("%s?=JOIN_GAME%%%%%d-%u%%%%%d-%u%%%%%u%%%%%u") %
		roomList->GetSelectedRoom()->path.c_str() %
		//(const char *) gServerList[gCurrentServerEntry].mURL,
		mCurrentGameIndex % mCurrentGameId % mCurrentUserIndex % mCurrentUserId %
		Config::GetInstance()->net.tcpRecvPort %
		Config::GetInstance()->net.udpRecvPort);

	lReturnValue = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NET_PROGRESS), pParentWindow, NetOpCallBack) == IDOK;

	mOpRequest.Clear();

	return lReturnValue;
}

BOOL InternetRoom::LeaveGameOp(HWND pParentWindow)
{
	BOOL lReturnValue = FALSE;

	mThis = this;

	mNetOpString = _("Unregistering from the Internet Meeting Room...");

	mNetOpRequest = boost::str(boost::format("%s?=LEAVE_GAME%%%%%d-%u%%%%%d-%u") %
		roomList->GetSelectedRoom()->path %
		//(const char *) gServerList[gCurrentServerEntry].mURL,
		mCurrentGameIndex % mCurrentGameId % mCurrentUserIndex % mCurrentUserId);

	lReturnValue = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NET_PROGRESS), pParentWindow, NetOpCallBack) == IDOK;

	mOpRequest.Clear();

	return lReturnValue;
}

BOOL InternetRoom::AddMessageOp(HWND pParentWindow, const char *pMessage, int pHours, int pMinutes)
{
	BOOL lReturnValue = FALSE;

	mThis = this;

	mNetOpString = _("Sending message to the Internet Meeting Room...");

	mNetOpRequest = boost::str(boost::format("%s?=MESSAGE%%%%%d-%u%%%%%d:%d%%%%%s") %
		roomList->GetSelectedRoom()->path %
		//(const char *) gServerList[gCurrentServerEntry].mURL,
		mCurrentUserIndex % mCurrentUserId %
		pHours % pMinutes %
		MR_Pad(pMessage));

	lReturnValue = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NET_PROGRESS), pParentWindow, NetOpCallBack) == IDOK;

	mOpRequest.Clear();

	return lReturnValue;
}

BOOL InternetRoom::AskRoomParams(HWND pParentWindow, BOOL pShouldRecheckServer)
{
	// check for updates, if we need to; don't check if this was not built by the buildserver (and version is not marked)
	if(checkUpdates && (Config::GetInstance()->GetBuild() != 0)) {
		CheckUpdateServerDialog(Config::GetInstance()->net.updateServer).ShowModal(NULL, pParentWindow);
		checkUpdates = false;
	}

	if (roomList == NULL || pShouldRecheckServer) {
		SelectRoomDialog dlg(mUser);
		roomList = dlg.ShowModal(NULL, pParentWindow);
		if (roomList != NULL) {
			mUser = dlg.GetPlayerName();
		}
	}
	return roomList != NULL;
}


/**
 * This function is called to initiate the connection to the IMR
 */
BOOL InternetRoom::DisplayChatRoom(HWND pParentWindow, NetworkSession *pSession, VideoServices::VideoBuffer *pVideoBuffer, BOOL pShouldRecheckServer)
{
	mUser = pSession->GetPlayerName();

	BOOL lReturnValue = AskRoomParams(pParentWindow, pShouldRecheckServer);

	if(lReturnValue) {
		mThis = this;

		mSession = pSession;
		mVideoBuffer = pVideoBuffer;

		mSession->SetPlayerName(mUser.c_str());

		lReturnValue = DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_INTERNET_MEETING_PUB), pParentWindow, RoomCallBack) == IDOK;
	}
	return lReturnValue;
}

/**
 * Initialize the chat log.
 * If the chat log could not be opened, then an error message is written to
 * the the message window.
 */
void InternetRoom::OpenChatLog()
{
	namespace fs = boost::filesystem;

	const Config *cfg = Config::GetInstance();
	if (!cfg->net.logChats) return;

	OS::path_t logPath(cfg->net.logChatsPath);
	if (logPath.empty()) return;

	try {
		if (!fs::exists(logPath)) {
			fs::create_directories(logPath);
		}
	}
	catch (OS::fs_error_t &ex) {
		AddChatLine(_("Unable to create chat log file:"));
		AddChatLine(_("Unable to create directory:"));
		AddChatLine(Str::PU(logPath));
		AddChatLine(ex.what());
		return;
	}

	tm now;
	const time_t curTime = time(NULL);
	memcpy(&now, localtime(&curTime), sizeof(now));
	char filename[128] = { 0 };
	strftime(filename, 128, "Chat %Y-%m-%d %H%M %z.txt", &now);
	char timestamp[128] = { 0 };
	strftime(timestamp, 128, "%Y-%m-%d %H:%M:%S %z", &now);

	logPath /= Str::UP(filename);

	chatLog = new fs::ofstream(logPath, std::ios_base::app | std::ios_base::out);
	if (chatLog->fail()) {
		delete chatLog;
		chatLog = NULL;
		AddChatLine(_("Unable to create chat log file:"));
		AddChatLine(_("Unable to open file for writing:"));
		AddChatLine(Str::PU(logPath));
		return;
	}

	// Shouldn't actually be necessary, but better to be safe.
	chatLog->imbue(OS::stdLocale);

	// We intentionally include some UTF-8 text in the header so that
	// Windows apps (such as Notepad) can try to auto-detect the encoding.
	// The first bit is equivalent to HTML: &raquo;&rsaquo;
	*chatLog << "\302\273\342\200\272 " << _("Chat log started at:") << " " <<
		timestamp << std::endl << std::endl;

	AddChatLine(_("Saving chat session to:"), true);
	AddChatLine(Str::PU(logPath), true);
}

/***
 * Add a line to the chat dialog (but this does not refresh that dialog).
 * @param pText The text (UTF-8).
 * @param neverLog @c true to prevent writing to the chat log file.
 */
void InternetRoom::AddChatLine(const char *pText, bool neverLog)
{
	if (!mChatBuffer.empty()) {
		mChatBuffer += "\r\n";
	}
	mChatBuffer += pText;

	if (!neverLog && chatLog != NULL) {
		*chatLog << pText << std::endl;
	}

	// Determine if we must cut some lines from the buffer
	while (mChatBuffer.length() > (40 * 40)) {
		size_t idx = mChatBuffer.find_first_of('\n');

		if (idx == std::string::npos) {
			break;
		}
		else {
			mChatBuffer.erase(0, idx + 1);
		}
	}
}

void InternetRoom::SelectGameForUser(HWND pWindow)
{
	int lFocus = FindFocusItem(GetDlgItem(pWindow, IDC_USER_LIST));

	if(lFocus != -1) {
		if((mClientList[lFocus].mValid) && (mClientList[lFocus].mGame != -1)) {
			HWND lList = GetDlgItem(pWindow, IDC_GAME_LIST);
			LV_FINDINFO lInfo;

			lInfo.flags = LVFI_PARAM;
			lInfo.lParam = mClientList[lFocus].mGame;

			int lGameIndex = ListView_FindItem(lList, -1, &lInfo);

			if(lGameIndex != -1) {
				ListView_SetItemState(lList, lGameIndex, LVIS_FOCUSED, LVIS_FOCUSED);
			}
		}
	}
}

void InternetRoom::RefreshGameSelection(HWND pWindow)
{
	int lGameIndex = FindFocusItem(GetDlgItem(pWindow, IDC_GAME_LIST));

	if(lGameIndex == -1) {
		SetDlgItemTextW(pWindow, IDC_TRACK_NAME, Str::UW(_("[no selection]")));
		SetDlgItemText(pWindow, IDC_NB_LAP, "");
		SetDlgItemText(pWindow, IDC_WEAPONS, "");
		SetDlgItemText(pWindow, IDC_AVAIL_MESSAGE, "");
		SetDlgItemText(pWindow, IDC_PLAYER_LIST, "");

		SendMessage(GetDlgItem(pWindow, IDC_JOIN), WM_ENABLE, FALSE, 0);
	}
	else {
		std::string lAvailString;
		std::string lPlayerList;

		switch (mGameList[lGameIndex].mAvailCode) {
			case eTrackAvail:
				lAvailString = _("Available");
				break;

			case eTrackNotFound:
				lAvailString = _("Join game to download from hoverrace.org");
				break;
		}

		for(int lCounter = 0; lCounter < mGameList[lGameIndex].mNbClient; lCounter++) {
			int lClientIndex = mGameList[lGameIndex].mClientList[lCounter];

			if(mClientList[lClientIndex].mValid) {
				if (!lPlayerList.empty()) {
					lPlayerList += "\r\n";
				}
				lPlayerList += mClientList[lClientIndex].mName;
			}

		}

		SetDlgItemTextW(pWindow, IDC_TRACK_NAME, Str::UW(mGameList[lGameIndex].mTrack.c_str()));
		SetDlgItemInt(pWindow, IDC_NB_LAP, mGameList[lGameIndex].mNbLap, FALSE);
		SetDlgItemText(pWindow, IDC_WEAPONS, mGameList[lGameIndex].mAllowWeapons ? "on" : "off");
		SetDlgItemTextW(pWindow, IDC_AVAIL_MESSAGE, Str::UW(lAvailString.c_str()));
		SetDlgItemTextW(pWindow, IDC_PLAYER_LIST, Str::UW(lPlayerList.c_str()));

		SendMessage(GetDlgItem(pWindow, IDC_JOIN), WM_ENABLE, mGameList[lGameIndex].mAvailCode == eTrackAvail, 0);
	}

}

void InternetRoom::RefreshGameList(HWND pWindow)
{
	HWND lList = GetDlgItem(pWindow, IDC_GAME_LIST);

	if(lList != NULL) {
		// Get selection
		int lSelected = FindFocusItem(lList);

		// Clear the content of the list box
		ListView_DeleteAllItems(lList);

		// Refill
		int lIndex = 0;
		for(int lCounter = 0; lCounter < eMaxGame; lCounter++) {
	
			
		//	MessageBox(0, "GL", "MessageBox caption", MB_OK);
			if(mGameList[lCounter].mValid) {
				LV_ITEM lItem;

				lItem.mask = LVIF_TEXT | LVIF_PARAM;
				lItem.iItem = lIndex++;
				lItem.iSubItem = 0;
				lItem.pszText = const_cast<char*>(mGameList[lCounter].mName.c_str());
				lItem.lParam = lCounter;

				if(lCounter == lSelected) {
					lItem.mask |= LVIF_STATE;
					lItem.state = LVIS_FOCUSED;
					lItem.stateMask = LVIS_FOCUSED;
				}

				ListView_InsertItem(lList, &lItem);
			}
		}
	}
	RefreshGameSelection(pWindow);
}

void InternetRoom::RefreshUserList(HWND pWindow)
{
	HWND lList = GetDlgItem(pWindow, IDC_USER_LIST);

	if(lList != NULL) {
		// Get selection
		int lSelected = FindFocusItem(lList);

		// Clear the content of the list box
		ListView_DeleteAllItems(lList);

		// Refill
		int lIndex = 0;
		for(int lCounter = 0; lCounter < eMaxClient; lCounter++) {
			if(mClientList[lCounter].mValid) {
				std::string &lName = mClientList[lCounter].mName;

				if(mClientList[lCounter].mMajorID != -1) {
					lName += boost::str(boost::format("[%d-%d]") %
						mClientList[lCounter].mMajorID %
						mClientList[lCounter].mMinorID);
				}
				LV_ITEM lItem;

				lItem.mask = LVIF_TEXT | LVIF_PARAM;
				lItem.iItem = lIndex++;
				lItem.iSubItem = 0;
				lItem.pszText = const_cast<char*>(lName.c_str());
				lItem.lParam = lCounter;

				if(lCounter == lSelected) {
					lItem.mask |= LVIF_STATE;
					lItem.state = LVIS_FOCUSED;
					lItem.stateMask = LVIS_FOCUSED;
				}

				int lCode = ListView_InsertItem(lList, &lItem);

				ASSERT(lCode != -1);

			}
		}

	}

}

/***
 * Refresh the chat buffer.
 * Keep in mind that mChatBuffer is internally UTF-8 (not wide), so Str::UW must be used before displaying.
 */
void InternetRoom::RefreshChatOut(HWND pWindow)
{
	HWND pDest = GetDlgItem(pWindow, IDC_CHAT_OUT);

	static SETTEXTEX textInfo = { ST_DEFAULT, 1200 };  // Replace all using Unicode.
	SendMessage(pDest, EM_SETTEXTEX, (WPARAM)&textInfo,
		(LPARAM)(const wchar_t*)Str::UW(mChatBuffer.c_str()));

	//SendMessage(pDest, EM_LINESCROLL, 0, 1000);
	SendMessage(pDest, WM_VSCROLL, SB_BOTTOM, 0);
}

/**
 * Play the "message received" notification sound.
 * @param wnd The dialog window handle.
 */
void InternetRoom::PlayMessageReceivedSound(HWND wnd) {
	Config *cfg = Config::GetInstance();

	if (!cfg->net.messageReceivedSound) return;

	// Only play if not foreground window.
	if (cfg->net.messageReceivedSoundOnlyBg && GetForegroundWindow() == wnd) return;

	// Play the sound at most once per second.
	OS::timestamp_t curTime = OS::Time();
	if (OS::TimeDiff(curTime, lastMessageReceivedSoundTs) < 1000) return;
	lastMessageReceivedSoundTs = curTime;

	//TODO: Preload sounds into memory and use SND_MEMORY.
	PlaySoundW(Str::PW(cfg->GetMediaPath("sounds/imr/message.wav")), NULL, SND_FILENAME);
}

BOOL InternetRoom::VerifyError(HWND pParentWindow, const char *pAnswer)
{ 
	BOOL lReturnValue = FALSE;
	int lCode = -1;

	const char *lLinePtr = pAnswer;

	while(lLinePtr != NULL) {
		if(!strncmp(lLinePtr, "SUCCESS", 7)) {
			lReturnValue = TRUE;
			break;
		}
		else if(!strncmp(lLinePtr, "ERROR", 5)) {
			sscanf(lLinePtr, "ERROR %d", &lCode);
			lReturnValue = FALSE;
			break;
		}
		lLinePtr = GetNextLine(lLinePtr);
	}

	if(!lReturnValue && (pParentWindow != NULL)) {
		BOOL lPopDlg = TRUE;
		std::string lMessage;

		if(lCode == -1) {
			ASSERT(FALSE);
			lMessage = _("Communication error");
		}

		while (lMessage.empty()) {
			switch (lCode) {
				case 100:
					lMessage = _("Unable to add user");
					break;

				case 101:
					lMessage = _("No more Shareware users allowed");
					break;

				case 102:
					lMessage = _("No more user allowed");
					break;

				case 103:
					lMessage = _("Incompatible version");
					break;

				case 104:
					lMessage = _("Expired key");
					break;

				case 105:
					lMessage = _("Already used key (report for investigation)");
					break;

				case 200:
					lMessage = _("Unable to send refresh info");
					break;

				case 201:
					lMessage = _("Not online");
					break;

				case 300:
					lMessage = _("Unable to add chat message");
					break;

				case 301:
					lMessage = _("Not online");
					break;

				case 400:
					lMessage = _("Unable to add game");
					break;

				case 401:
					lMessage = _("Not online");
					break;

				case 402:
					lMessage = _("Entry is no longer available");
					break;

				case 500:
					lMessage = _("Unable to add user");
					break;

				case 501:
					lMessage = _("Not online");
					break;

				case 502:
					lMessage = _("Game is no longer available");
					break;

				case 503:
					lMessage = _("Game is full");
					break;

				case 600:
					lMessage = _("Unable to delete game");
					break;

				case 601:
					lMessage = _("Not online");
					lPopDlg = FALSE;
					break;

				case 602:
					lMessage = _("Game is no longer available");
					break;

				case 603:
					lMessage = _("Not owner");
					break;

				case 700:
					lMessage = _("Unable to leave game");
					break;

				case 701:
					lMessage = _("Not online");
					lPopDlg = FALSE;
					break;

				case 702:
					lMessage = _("Game is no longer available");
					lPopDlg = FALSE;
					break;

				case 703:
					lMessage = _("Must join first");
					break;

				case 800:
					lMessage = _("Unable to delete user");
					break;

				case 801:
					lMessage = _("Not online");
					lPopDlg = FALSE;
					break;

				case 900:
					lMessage = _("Unable to start game");
					break;

				case 901:
					lMessage = _("Not online");
					break;

				case 902:
					lMessage = _("Game is no longer available");
					break;

				case 903:
					lMessage = _("Not owner");
					break;

				case 1000:
					lMessage = _("Unable to add message");
					break;

				case 1001:
					lMessage = _("Not online");
					break;

				case 1002:
					lMessage = _("Not authorized");
					break;

			}

			if (lMessage.empty()) {
				if((lCode % 100) == 0) {
					lMessage = boost::str(boost::format("%s %d") %
						_("Unknown error code") %
						lCode).c_str();
				}
				else {
					// Restart the sequence but only with the genic number
					lCode = lCode - (lCode % 100);
				}
			}
		}

		if(lPopDlg) {
			MessageBoxW(pParentWindow, Str::UW(lMessage.c_str()),
				Str::UW(_("Internet Meeting Room")),
				MB_ICONSTOP | MB_OK | MB_APPLMODAL);
		}
	}
	return lReturnValue;
}

int InternetRoom::LoadBanner(HWND pWindow, const char *pBuffer, int pBufferLen)
{
	ASSERT(pWindow != NULL);

	HWND lWindow = GetDlgItem(pWindow, IDC_PUB);

	if(lWindow == NULL) {
		return 0;								  // no more refresh
	}
	else {
		mBanner.Decode((unsigned char *) pBuffer, pBufferLen);

		HDC hdc = GetDC(lWindow);

		HPALETTE lOldPalette = SelectPalette(hdc, mBanner.GetGlobalPalette(), FALSE);

		int lNbColors = RealizePalette(hdc);

		if(lOldPalette != NULL) {
			SelectPalette(hdc, mBanner.GetGlobalPalette(), TRUE);
		}

		ReleaseDC(lWindow, hdc);

		//TRACE("Colors2 %d  %d\n", lNbColors, GetLastError());

		mCurrentBannerIndex = 0;
		SendMessage(lWindow, BM_SETIMAGE, IMAGE_BITMAP, (long) mBanner.GetImage(0));

		return mBanner.GetDelay(0);
	}
}

int InternetRoom::RefreshBanner(HWND pWindow)
{
	ASSERT(pWindow != NULL);

	HWND lWindow = GetDlgItem(pWindow, IDC_PUB);

	if((lWindow == NULL) || (mBanner.GetImageCount() == 0)) {
		return 0;								  // no more refresh
	}
	else {
		mCurrentBannerIndex = (mCurrentBannerIndex + 1) % mBanner.GetImageCount();

		SendMessage(lWindow, BM_SETIMAGE, IMAGE_BITMAP, (long) mBanner.GetImage(mCurrentBannerIndex));

		return mBanner.GetDelay(mCurrentBannerIndex);

	}

}

void InternetRoom::TrackOpenFailMessageBox(HWND parent, const std::string &name,
                                              const std::string &details)
{
	std::string msg = boost::str(boost::format(_("Unable to load track \"%s\".  Error details:")) % name);
	msg += "\r\n\r\n";
	msg += details;
	MessageBoxW(parent, Str::UW(msg.c_str()), PACKAGE_NAME_L, MB_ICONWARNING);
}

BOOL CALLBACK InternetRoom::RoomCallBack(HWND pWindow, UINT pMsgId, WPARAM pWParam, LPARAM pLParam)
{
	BOOL lReturnValue = FALSE;

	switch (pMsgId) {
		// Catch environment modification events
		case WM_INITDIALOG:
			{
				// i18n
				// so we don't need to have things translated multiple times
			//	std::string title = boost::str(boost::format("%s %s") %
		//			PACKAGE_NAME %
			//		_("Internet Meeting Room"));
				
			std::string title = "Internet Meeting Room";

				SetWindowTextW(pWindow, Str::UW(title.c_str()));
				SetDlgItemTextW(pWindow, IDC_GAME_LIST_C, Str::UW(_("Game list")));
				SetDlgItemTextW(pWindow, IDC_USERS_LIST_C, Str::UW(_("User list")));
				SetDlgItemTextW(pWindow, IDC_GAME_DETAILS_C, Str::UW(_("Game details")));
				SetDlgItemTextW(pWindow, IDC_TRACK_NAME_C, Str::UW(_("Track name:")));
				SetDlgItemTextW(pWindow, IDC_LAPS_C, Str::UW(_("Laps:")));
				SetDlgItemTextW(pWindow, IDC_WEAPONS_C, Str::UW(_("Weapons:")));
				SetDlgItemTextW(pWindow, IDC_AVAILABILITY_C, Str::UW(_("Availability:")));
				SetDlgItemTextW(pWindow, IDC_PLAYERS_LIST_C, Str::UW(_("Player list:")));
				SetDlgItemTextW(pWindow, IDC_CHAT_SECTION_C, Str::UW(_("Chat section")));
				SetDlgItemTextW(pWindow, IDC_JOIN, Str::UW(_("Join Game...")));
				SetDlgItemTextW(pWindow, IDC_ADD, Str::UW(_("New Game...")));
				SetDlgItemTextW(pWindow, IDCANCEL, Str::UW(_("Quit")));

				// Enable word-wrapping in chat box.
				SendDlgItemMessageW(pWindow, IDC_CHAT_OUT, EM_SETTARGETDEVICE, NULL, 0);
				
				// Set the color to the same as the old control
				// I suppose this isn't always guaranteed to be gray but it's the best I can find...
				// there's no COLOR_DISABLEDTEXTBOX or something
				SendDlgItemMessageW(pWindow, IDC_CHAT_OUT, EM_SETBKGNDCOLOR, 0, GetSysColor(COLOR_BTNFACE));

				// Enable link handling in chat box.
				DWORD mask = SendDlgItemMessageW(pWindow, IDC_CHAT_OUT, EM_GETEVENTMASK, 0, 0);
				SendDlgItemMessageW(pWindow, IDC_CHAT_OUT, EM_SETEVENTMASK, 0, mask | ENM_LINK);
				SendDlgItemMessageW(pWindow, IDC_CHAT_OUT, EM_AUTOURLDETECT, TRUE, 0);

				RECT lRect;
				HWND lList;
				LV_COLUMN lSpec;
	
				// Adjust dlg items
				lList = GetDlgItem(pWindow, IDC_USER_LIST);
				GetClientRect(lList, &lRect);
	
				// Create list columns
				lSpec.mask = LVCF_SUBITEM | LVCF_WIDTH | LVCF_FMT;
	
				lSpec.fmt = LVCFMT_LEFT;
				lSpec.cx = lRect.right - 1 - GetSystemMetrics(SM_CXVSCROLL);
				lSpec.iSubItem = 0;
	
				ListView_InsertColumn(lList, 0, &lSpec);
	
				lList = GetDlgItem(pWindow, IDC_GAME_LIST);
				GetClientRect(lList, &lRect);
	
				lSpec.mask = LVCF_SUBITEM | LVCF_WIDTH | LVCF_FMT;
	
				lSpec.fmt = LVCFMT_LEFT;
				lSpec.cx = lRect.right - 1 - GetSystemMetrics(SM_CXVSCROLL);
				lSpec.iSubItem = 0;
	
				ListView_InsertColumn(lList, 0, &lSpec);

				// Start chat logging.
				mThis->OpenChatLog();
	
				// Connect to server
	
				{
					if(!mThis->AddUserOp(pWindow)) {
						EndDialog(pWindow, IDCANCEL);
					}
					else {
						// Start the automatic refresh sequence
						SetTimer(pWindow, REFRESH_EVENT, 2 * REFRESH_DELAY, NULL);
	
						// Init dialog lists
						mThis->RefreshGameList(pWindow);
						mThis->RefreshUserList(pWindow);
						mThis->RefreshChatOut(pWindow);
					}
				}

				// Subclass the banner button so we can control the behavior.
				mThis->oldBannerProc = (WNDPROC)SetWindowLong(
					GetDlgItem(pWindow, IDC_PUB),
					GWL_WNDPROC,
					(LONG)BannerCallBack);
				
			}
			lReturnValue = TRUE;
	
			// Initiate banners loading in 1 seconds
			if (mThis->roomList->HasBanners()) {
				SetTimer(pWindow, LOAD_BANNER_TIMEOUT_EVENT, 1000, NULL);
			}
	
			break;

		case WM_QUERYNEWPALETTE:
			if(mThis->mBanner.GetGlobalPalette() != NULL) {
				HWND lBitmapCtl = GetDlgItem(pWindow, IDC_PUB);

				if(lBitmapCtl != NULL) {
					TRACE("PALSET\n");

					HDC hdc = GetDC(lBitmapCtl);

					// UnrealizeObject( mThis->mBanner.GetGlobalPalette() );
					HPALETTE lOldPalette = SelectPalette(hdc, mThis->mBanner.GetGlobalPalette(), FALSE);

					if(RealizePalette(hdc) > 0) {
						lReturnValue = TRUE;
					}

					if(lOldPalette != NULL) {
						SelectPalette(hdc, lOldPalette, FALSE);
					}

					ReleaseDC(lBitmapCtl, hdc);

					// InvalidateRgn( pWindow, NULL, TRUE );
					InvalidateRgn(lBitmapCtl, NULL, TRUE);
					// UpdateWindow( pWindow );
					// UpdateWindow( lBitmapCtl );

					// lReturnValue = TRUE;
				}
			}
			break;

		case WM_PALETTECHANGED:
			if((mThis->mBanner.GetGlobalPalette() != NULL) && ((HWND) pWParam != pWindow)) {
				HWND lBitmapCtl = GetDlgItem(pWindow, IDC_PUB);

				if((pWParam != (int) lBitmapCtl) && (lBitmapCtl != NULL)) {
					TRACE("PALCHANGE\n");
					HDC hdc = GetDC(lBitmapCtl);

					// UnrealizeObject( mThis->mBanner.GetGlobalPalette() );
					HPALETTE lOldPalette = SelectPalette(hdc, mThis->mBanner.GetGlobalPalette(), TRUE);

					if(RealizePalette(hdc) > 0) {
						lReturnValue = TRUE;
					}

					if(lOldPalette != NULL) {
						SelectPalette(hdc, lOldPalette, FALSE);
					}

					ReleaseDC(lBitmapCtl, hdc);

					// InvalidateRgn( pWindow, NULL, TRUE );
					InvalidateRgn(lBitmapCtl, NULL, TRUE);
					// UpdateWindow( pWindow );
					// UpdateWindow( lBitmapCtl );

					// lReturnValue = TRUE;

				}
			}
			break;

		case WM_TIMER:
			lReturnValue = TRUE;

			KillTimer(pWindow, pWParam);

			switch (pWParam) {
				case REFRESH_TIMEOUT_EVENT:
					{
						// Cancel the pending call
						mThis->mRefreshRequest.Clear();
						mThis->mNbSuccessiveRefreshTimeOut++;
	
						// Warn the user
						if(mThis->mNbSuccessiveRefreshTimeOut >= 2) {
							mThis->AddChatLine(_("Warning: communication timeout"));
							mThis->RefreshChatOut(pWindow);
						}
						// Initiate a new refresh
						SetTimer(pWindow, REFRESH_EVENT, 1 /*REFRESH_DELAY */ , NULL);
	
					}
					break;

				case REFRESH_EVENT:
					{
						std::string lRequest;
	
						lRequest = boost::str(boost::format("%s?=REFRESH%%%%%d-%u%%%%%d") %
							mThis->roomList->GetSelectedRoom()->path %
							//(const char *) gServerList[gCurrentServerEntry].mURL,
							mThis->mCurrentUserIndex %
							mThis->mCurrentUserId %
							mThis->mLastRefreshTimeStamp);
						mThis->mRefreshRequest.Send(pWindow,
							mThis->roomList->GetSelectedRoom()->addr,
							//gServerList[gCurrentServerEntry].mAddress,
							mThis->roomList->GetSelectedRoom()->port,
							//gServerList[gCurrentServerEntry].mPort,
							lRequest.c_str());
	
						// Activate timeout
						SetTimer(pWindow, REFRESH_TIMEOUT_EVENT, REFRESH_TIMEOUT, NULL);
					}
					break;

				case CHAT_TIMEOUT_EVENT:
					{
						// Cancel the pending call
						mThis->mChatRequest.Clear();
	
						// Warn the user
						mThis->AddChatLine(_("Warning: communication timeout"));
						mThis->RefreshChatOut(pWindow);
					}
					break;

				case LOAD_BANNER_TIMEOUT_EVENT:
					{
						// Already start timer for next load
						if (mThis->roomList->HasBanners()) {
	
							RoomList::Banner *nextBanner =
								mThis->roomList->PeekNextBanner();
	
							SetTimer(pWindow, LOAD_BANNER_TIMEOUT_EVENT,
								nextBanner->delay * 1000, NULL);
	
							// Initiate the loading of a new banner
	
							if(mThis->mBannerRequest.Send(pWindow,
								nextBanner->addr,
								nextBanner->port,
								nextBanner->path.c_str()))
							{
								// No timeout on that one
							}
						}
	
					}
					break;

				case ANIM_BANNER_TIMEOUT_EVENT:
					{
						TRACE("RefreshBanner\n");
						int lNextRefresh = mThis->RefreshBanner(pWindow);
	
						if(lNextRefresh != 0) {
							SetTimer(pWindow, ANIM_BANNER_TIMEOUT_EVENT, lNextRefresh, NULL);
						}
					}
					break;

			}
			break;

		case MRM_NET_EVENT:
			if(mThis->mChatRequest.ProcessEvent(pWParam, pLParam)) {
				if(mThis->mChatRequest.IsReady()) {
					// Simply reset
					KillTimer(pWindow, CHAT_TIMEOUT_EVENT);
					mThis->mChatRequest.Clear();
				}
			}

			if(mThis->mRefreshRequest.ProcessEvent(pWParam, pLParam)) {
				if(mThis->mRefreshRequest.IsReady()) {
					KillTimer(pWindow, REFRESH_TIMEOUT_EVENT);

					const char *lAnswer = mThis->mRefreshRequest.GetBuffer();

					mThis->mNbSuccessiveRefreshTimeOut = 0;

					if(!mThis->VerifyError(pWindow, lAnswer)) {
						EndDialog(pWindow, IDCANCEL);
					}
					else {
						// We must now parse the answer
						int lToRefresh = mThis->ParseState(lAnswer);

						if(lToRefresh & eGamesModified) {
							mThis->RefreshGameList(pWindow);
						}

						if(lToRefresh & eUsersModified) {
							mThis->RefreshUserList(pWindow);
						}

						if(lToRefresh & eChatModified) {
							mThis->RefreshChatOut(pWindow);
							mThis->PlayMessageReceivedSound(pWindow);
						}

						// Schedule a new refresh
						SetTimer(pWindow, REFRESH_EVENT, REFRESH_DELAY, NULL);
					}
					mThis->mRefreshRequest.Clear();
				}
			}

			if(mThis->mBannerRequest.ProcessEvent(pWParam, pLParam)) {
				//TRACE("LoadBanner\n");

				if(mThis->mBannerRequest.IsReady()) {
					int lBufferSize;
					const char *lBuffer = mThis->mBannerRequest.GetBinBuffer(lBufferSize);

					if(lBuffer != NULL) {
						// Kill animation timer
						KillTimer(pWindow, ANIM_BANNER_TIMEOUT_EVENT);

						const char *lGifBuf = NULL;
						// Find the GIF8 string indicating start of buffer
						// I hope that they wont create a GIF97 format

						RoomList::Banner *banner = mThis->roomList->NextBanner();
						//int lEntry = (gCurrentBannerEntry + 1) % gNbBannerEntries;

						banner->cookie = "";

						for(int lCounter = 0; lCounter < min(lBufferSize - 30, 400); lCounter++) {
							if(lBuffer[lCounter] == 'S') {
								if(!strncmp(lBuffer + lCounter, "Set-Cookie:", 11)) {
									// of we found a cookie
									// skip spaces
									lCounter += 11;
									while(isspace(lBuffer[lCounter])) {
										lCounter++;
									}

									if(!banner->cookie.empty()) {
										banner->cookie += "; ";
									}

									while((lBuffer[lCounter] != '\n') && (lBuffer[lCounter] != '\r') && (lBuffer[lCounter] != ';')) {
										banner->cookie += lBuffer[lCounter++];
									}
								}
							}

							if(lBuffer[lCounter] == 'G') {
								if(!strncmp(lBuffer + lCounter, "GIF8", 4)) {
									lBufferSize -= lCounter;
									lGifBuf = lBuffer + lCounter;
									break;
								}
							}
						}

						if(lGifBuf != NULL) {
							int lNextRefresh = mThis->LoadBanner(pWindow, lGifBuf, lBufferSize);

							//gCurrentBannerEntry = lEntry;

							if(lNextRefresh != 0) {
								SetTimer(pWindow, ANIM_BANNER_TIMEOUT_EVENT, lNextRefresh, NULL);
							}
						}
					}
				}
			}

			if(mThis->mClickRequest.ProcessEvent(pWParam, pLParam)) {
				TRACE("ClickBannerReady\n");

				if(mThis->mClickRequest.IsReady()) {
					// Find the location URL and load it
					const char *lLocation = strstr(mThis->mClickRequest.GetBuffer(), "Location:");

					if(lLocation != NULL) {
						char lURLBuffer[300];

						lURLBuffer[0] = 0;
						sscanf(lLocation + 9, " %299s", lURLBuffer);
						lURLBuffer[299] = 0;

						if(strlen(lURLBuffer) > 0) {
							OS::OpenLink(lURLBuffer);
						}
					}
					mThis->mClickRequest.Clear();
				}
			}

			break;

		case WM_NOTIFY:
			{
				NMHDR *lNotMessage = (NMHDR *) pLParam;
	
				switch (lNotMessage->idFrom) {
					case IDC_GAME_LIST:
						if(lNotMessage->code == LVN_ITEMCHANGED) {
							lReturnValue = TRUE;
							mThis->RefreshGameSelection(pWindow);
						}
						break;

					case IDC_USER_LIST:
						if(lNotMessage->code == LVN_ITEMCHANGED) {
							lReturnValue = TRUE;

							// Select the game corresponding to the selected
							mThis->SelectGameForUser(pWindow);
						}
						break;

					case IDC_CHAT_OUT:
						// User clicked on a link.
						if (lNotMessage->code == EN_LINK) {
							ENLINK *linkInfo = (ENLINK*)pLParam;
							if (linkInfo->msg == WM_LBUTTONUP) {
								CHARRANGE *chrg = &linkInfo->chrg;
								int len = chrg->cpMax - chrg->cpMin;
								wchar_t url[512] = { 0 };
								TEXTRANGEW range;
								range.chrg.cpMin = chrg->cpMin;
								range.chrg.cpMax = (len > 511) ? chrg->cpMin + 511 : chrg->cpMax;
								range.lpstrText = url;
								SendDlgItemMessageW(pWindow, IDC_CHAT_OUT, EM_GETTEXTRANGE, 0, (LPARAM)&range);

								// Open the URL in the browser.
								OS::OpenLink(Str::WU(range.lpstrText));
							}
						}
						break;
				}
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(pWParam)) {
				case IDOK:
					if(GetFocus() == GetDlgItem(pWindow, IDC_CHAT_IN)) {
						wchar_t lBuffer[200];

						lReturnValue = TRUE;

						GetDlgItemTextW(pWindow, IDC_CHAT_IN, lBuffer, sizeof(lBuffer));

						std::string lRequest;

						const RoomList::Server *room = mThis->roomList->GetSelectedRoom();

						lRequest = boost::str(boost::format("%s?=ADD_CHAT%%%%%d-%u%%%%%s") %
							room->path %
							//(const char *) gServerList[gCurrentServerEntry].mURL,
							mThis->mCurrentUserIndex % mThis->mCurrentUserId %
							MR_Pad(Str::WU(lBuffer)));

						if (mThis->mChatRequest.Send(pWindow, room->addr, room->port, lRequest.c_str())) {
							SetDlgItemText(pWindow, IDC_CHAT_IN, "");

							// Activate timeout
							SetTimer(pWindow, CHAT_TIMEOUT_EVENT, CHAT_TIMEOUT, NULL);
						}
						lReturnValue = TRUE;
					}
					break;

				case IDCANCEL:
					mThis->DelUserOp(pWindow);
					EndDialog(pWindow, IDCANCEL);
					lReturnValue = TRUE;
					break;

				case IDC_JOIN:
					lReturnValue = TRUE;

					if(mThis->mModelessDlg == NULL) {
						// First verify if the selected track can be played
						int lFocus = FindFocusItem(GetDlgItem(pWindow, IDC_GAME_LIST));

						if(lFocus != -1) {
							BOOL lSuccess = FALSE;

							// Register to the InternetServer
							lSuccess = mThis->JoinGameOp(pWindow, lFocus);

							if(lSuccess) {
								// Try to load the track
								// Load the track
								std::string lCurrentTrack(mThis->mGameList[lFocus].mTrack);
								Model::TrackPtr track;
								try {
									track = Config::GetInstance()->
										GetTrackBundle()->OpenTrack(lCurrentTrack.c_str());
								}
								catch (Parcel::ObjStreamExn&) {
									// Ignore -- force a re-download.
								}
								if (track.get() == NULL) {
									try {
										OutputDebugString("Track not found; downloading: ");
										OutputDebugString(lCurrentTrack.c_str());
										OutputDebugString("\n");

										lSuccess = TrackDownloadDialog(lCurrentTrack).ShowModal(GetModuleHandle(NULL), pWindow);
										if (lSuccess) {
											Model::TrackPtr track = Config::GetInstance()->
												GetTrackBundle()->OpenTrack(lCurrentTrack.c_str());
											if (track.get() == NULL) {
												throw Parcel::ObjStreamExn("Track failed to download.");
											}
										}
									}
									catch (Parcel::ObjStreamExn &ex) {
										TrackOpenFailMessageBox(pWindow, lCurrentTrack, ex.what());
										lSuccess = false;
									}
								}
								if (lSuccess) {
									lSuccess = mThis->mSession->LoadNew(mThis->mGameList[lFocus].mTrack.c_str(),
										track->GetRecordFile(), mThis->mGameList[lFocus].mNbLap,
										mThis->mGameList[lFocus].mAllowWeapons,
										mThis->mVideoBuffer);
								}

								if(lSuccess) {
									std::string curTrack = boost::str(boost::format("%s  %d laps") %
										mThis->mGameList[lFocus].mTrack %
										mThis->mGameList[lFocus].mNbLap);

									lSuccess = mThis->mSession->ConnectToServer(pWindow,
										mThis->mGameList[lFocus].mIPAddr.c_str(),
										mThis->mGameList[lFocus].mPort,
										lCurrentTrack.c_str(),
										&mThis->mModelessDlg, MRM_DLG_END_JOIN);
								}

								if(!lSuccess) {
									// Unregister from Game
									mThis->LeaveGameOp(pWindow);
								}
							}
						}
					}

					break;

				case IDC_ADD:
					lReturnValue = TRUE;

					if(mThis->mModelessDlg == NULL) {
						bool lSuccess = false;

						// Ask the user to select a track
						std::string lCurrentTrack;
						int lNbLap;
						char lGameOpts;

						RulebookPtr rules = TrackSelectDialog().ShowModal(GetModuleHandle(NULL), pWindow);
						if ((lSuccess = (rules.get() != NULL))) {
							lCurrentTrack = rules->GetTrackName();
							lNbLap = rules->GetLaps();
							lGameOpts = rules->GetGameOpts();
						}

						if(lSuccess) {
							// Load the track
							try {
								Model::TrackPtr track = Config::GetInstance()->
									GetTrackBundle()->OpenTrack(lCurrentTrack.c_str());
								if (track.get() == NULL)
									throw Parcel::ObjStreamExn("Track does not exist.");
								lSuccess = (mThis->mSession->LoadNew(
									lCurrentTrack.c_str(), track->GetRecordFile(), lNbLap,
									lGameOpts, mThis->mVideoBuffer) != FALSE);
							}
							catch (Parcel::ObjStreamExn &ex) {
								TrackOpenFailMessageBox(pWindow, lCurrentTrack, ex.what());
								lSuccess = false;
							}
						}

						if(lSuccess) {
							// Register to the InternetServer
							
							lSuccess = (mThis->AddGameOp(pWindow,
								lCurrentTrack.c_str(), lCurrentTrack.c_str(),
								lNbLap, lGameOpts,
								Config::GetInstance()->net.tcpServPort) != FALSE);

							if(lSuccess) {
								// Wait client registration
								std::string lTrackName;

								lTrackName = boost::str(boost::format("%s  %d %s; options %c%c%c, %c%c%c%c") %
									lCurrentTrack % lNbLap %
									((lNbLap == 1) ? "lap" : "laps") %
									((lGameOpts & OPT_ALLOW_WEAPONS) ? 'W' : '_') %
									((lGameOpts & OPT_ALLOW_MINES)   ? 'M' : '_') %
									((lGameOpts & OPT_ALLOW_CANS)    ? 'C' : '_') %
									((lGameOpts & OPT_ALLOW_BASIC)   ? 'B' : '_') %
									((lGameOpts & OPT_ALLOW_BI)      ? '2' : '_') %
									((lGameOpts & OPT_ALLOW_CX)      ? 'C' : '_') %
									((lGameOpts & OPT_ALLOW_EON)     ? 'E' : '_'));

								lSuccess = (mThis->mSession->WaitConnections(pWindow,
									lTrackName.c_str(), FALSE,
									Config::GetInstance()->net.tcpServPort,
									&mThis->mModelessDlg, MRM_DLG_END_ADD) != FALSE);

								if(!lSuccess) {
									// Unregister Game
									mThis->DelGameOp(pWindow);
								}
							}
						}
					}
					break;

				case IDC_PUB:
					{
						RoomList::Banner *banner = mThis->roomList->GetCurrentBanner();
						if(!banner->clickUrl.empty()) {
							if(banner->indirectClick) {
								mThis->mClickRequest.Clear();
								mThis->mClickRequest.Send(pWindow,
									banner->addr,
									banner->port,
									banner->clickUrl.c_str(),
									banner->cookie.c_str());
	
							}
							else {
								OS::OpenLink(banner->clickUrl.c_str());
							}
						}
					}
					break;

			}
			break;

		case MRM_DLG_END_ADD:
		case MRM_DLG_END_JOIN:
			lReturnValue = TRUE;

			mThis->mModelessDlg = NULL;

			if(pWParam == IDOK) {
				// Unregister user and game
				mThis->DelUserOp(pWindow, TRUE);

				// Quit with a success
				EndDialog(pWindow, IDOK);

				// Blink for user notification
				if(GetForegroundWindow() != GetParent(pWindow)) {
					FLASHWINFO lFlash;
					lFlash.cbSize = sizeof(lFlash);
					lFlash.hwnd = GetParent(pWindow);
					lFlash.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
					lFlash.uCount = 5;
					lFlash.dwTimeout = 0;

					FlashWindowEx(&lFlash);
				}
			}
			else {
				// Unregister Game
				if(pMsgId == MRM_DLG_END_ADD) {
					mThis->DelGameOp(pWindow);
				}
				else {
					mThis->LeaveGameOp(pWindow);
				}
			}
			break;

		case WM_DESTROY:
			{
				if(mThis->mModelessDlg != NULL) {
					// DestroyWindow( mThis->mModelessDlg );
					mThis->mModelessDlg = NULL;
				}
				mThis->mOpRequest.Clear();
				mThis->mChatRequest.Clear();
				mThis->mRefreshRequest.Clear();
			}
			break;
	}

	return lReturnValue;
}

BOOL CALLBACK InternetRoom::BannerCallBack(HWND pWindow, UINT pMsgId, WPARAM pWParam, LPARAM pLParam)
{
	switch (pMsgId) {
		case WM_SETCURSOR:
			if (LOWORD(pLParam) == HTCLIENT) {
				SetCursor(LoadCursor(NULL, IDC_HAND));
			}
			return TRUE;

		default:
			return CallWindowProc(mThis->oldBannerProc, pWindow, pMsgId, pWParam, pLParam);
	}
}

BOOL CALLBACK InternetRoom::NetOpCallBack(HWND pWindow, UINT pMsgId, WPARAM pWParam, LPARAM pLParam)
{
	BOOL lReturnValue = FALSE;

	switch (pMsgId) {
		// Catch environment modification events
		case WM_INITDIALOG:
			{
				// Setup message
				SetWindowTextW(pWindow, Str::UW(_("Network Transaction Progress")));
				SetDlgItemTextW(pWindow, IDC_TEXT, Str::UW(mThis->mNetOpString.c_str()));
	
				// Initiate the request
				mThis->mOpRequest.Send(pWindow,
					mThis->roomList->GetSelectedRoom()->addr,
					//gServerList[gCurrentServerEntry].mAddress,
					mThis->roomList->GetSelectedRoom()->port,
					//gServerList[gCurrentServerEntry].mPort,
					mThis->mNetOpRequest.c_str());
	
				// start a timeout timer
				SetTimer(pWindow, OP_TIMEOUT_EVENT, OP_TIMEOUT, NULL);
			}
			break;

		case WM_TIMER:
			{
				KillTimer(pWindow, pWParam);
				lReturnValue = TRUE;
	
				MessageBoxW(pWindow, Str::UW(_("Connection timeout")), 
					Str::UW(_("Internet Meeting Room")), MB_ICONSTOP | MB_OK | MB_APPLMODAL);
	
				EndDialog(pWindow, IDCANCEL);
	
				mThis->mOpRequest.Clear();
	
			}
			break;

		case MRM_NET_EVENT:
			mThis->mOpRequest.ProcessEvent(pWParam, pLParam);

			if(mThis->mOpRequest.IsReady()) {
				KillTimer(pWindow, OP_TIMEOUT_EVENT);

				BOOL lError = mThis->VerifyError(pWindow, mThis->mOpRequest.GetBuffer());

				EndDialog(pWindow, lError ? IDOK : IDCANCEL);
			}
			lReturnValue = TRUE;
			break;

		case WM_COMMAND:
			switch (LOWORD(pWParam)) {
				case IDCANCEL:
					EndDialog(pWindow, IDCANCEL);
					KillTimer(pWindow, OP_TIMEOUT_EVENT);
					mThis->mOpRequest.Clear();

					lReturnValue = TRUE;
					break;
			}
			break;
	}
	return lReturnValue;

}

BOOL CALLBACK InternetRoom::FastNetOpCallBack(HWND pWindow, UINT pMsgId, WPARAM pWParam, LPARAM pLParam)
{
	BOOL lReturnValue = FALSE;

	switch (pMsgId) {
		// Catch environment modification events
		case WM_INITDIALOG:
			{
				// Setup message
				SetWindowTextW(pWindow, Str::UW(_("Network Transaction Progress")));
				SetDlgItemTextW(pWindow, IDC_TEXT, Str::UW(mThis->mNetOpString.c_str()));
	
				// Initiate the request
				mThis->mOpRequest.Send(pWindow,
					mThis->roomList->GetSelectedRoom()->addr,
					//gServerList[gCurrentServerEntry].mAddress,
					mThis->roomList->GetSelectedRoom()->port,
					//gServerList[gCurrentServerEntry].mPort,
					mThis->mNetOpRequest.c_str());
	
				// start a timeout timer
				SetTimer(pWindow, OP_TIMEOUT_EVENT, FAST_OP_TIMEOUT, NULL);
			}
			break;

		case WM_TIMER:
			{
				KillTimer(pWindow, pWParam);
				lReturnValue = TRUE;
				EndDialog(pWindow, IDCANCEL);
			}
			break;

		case MRM_NET_EVENT:
			mThis->mOpRequest.ProcessEvent(pWParam, pLParam);

			if(mThis->mOpRequest.IsReady()) {
				KillTimer(pWindow, OP_TIMEOUT_EVENT);

				// BOOL lError = mThis->VerifyError( pWindow, mThis->mOpRequest.GetBuffer() );

				EndDialog(pWindow, IDOK);		  // humm always return IDOK
			}
			lReturnValue = TRUE;
			break;

		case WM_COMMAND:
			switch (LOWORD(pWParam)) {
				case IDCANCEL:
					EndDialog(pWindow, IDCANCEL);
					KillTimer(pWindow, OP_TIMEOUT_EVENT);
					mThis->mOpRequest.Clear();

					lReturnValue = TRUE;
					break;
			}
			break;
	}
	return lReturnValue;
}

std::string gScoreRequestStr;
InternetRequest gScoreRequest;

BOOL CALLBACK UpdateScoresCallBack(HWND pWindow, UINT pMsgId, WPARAM pWParam, LPARAM pLParam)
{
	BOOL lReturnValue = FALSE;

	switch (pMsgId) {
		// Catch environment modification events
		case WM_INITDIALOG:
			{
				RoomList *roomList = reinterpret_cast<RoomList*>(pLParam);
				SetWindowLong(pWindow, GWL_USERDATA, pLParam);

				// Setup message
				SetWindowTextW(pWindow, Str::UW(_("Network Transaction Progress")));
				SetDlgItemTextW(pWindow, IDC_TEXT, Str::UW(_("Registering your best lap time to the server")));
	
				// Initiate the request
				gScoreRequest.Send(pWindow,
					roomList->GetScoreServer().addr,
					//gScoreServer.mAddress,
					roomList->GetScoreServer().port,
					//gScoreServer.mPort,
					gScoreRequestStr.c_str());
	
				// start a timeout timer
				SetTimer(pWindow, OP_TIMEOUT_EVENT, SCORE_OP_TIMEOUT, NULL);
			}
			break;

		case WM_TIMER:
			{
				// Timeout
				RoomList *roomList = reinterpret_cast<RoomList*>(
					GetWindowLong(pWindow, GWL_USERDATA));

				gScoreRequest.Clear();
				KillTimer(pWindow, pWParam);
	
				// Ask the user if he want to retry
				if(MessageBoxW(pWindow, Str::UW(_("Connection timeout")), PACKAGE_NAME_L, 
					MB_ICONSTOP | MB_RETRYCANCEL | MB_APPLMODAL) == IDRETRY)
				{
					// Initiate the request
					gScoreRequest.Send(pWindow,
						roomList->GetScoreServer().addr,
						//gScoreServer.mAddress,
						roomList->GetScoreServer().port,
						//gScoreServer.mPort,
						gScoreRequestStr.c_str());

					// start a timeout timer
					SetTimer(pWindow, OP_TIMEOUT_EVENT, SCORE_OP_TIMEOUT + 3000, NULL);
				}
				else {
					EndDialog(pWindow, IDCANCEL);
				}
				lReturnValue = TRUE;
			}
			break;

		case MRM_NET_EVENT:
			gScoreRequest.ProcessEvent(pWParam, pLParam);

			if(gScoreRequest.IsReady()) {
				KillTimer(pWindow, OP_TIMEOUT_EVENT);

				EndDialog(pWindow, IDOK);		  // humm always return IDOK
			}
			lReturnValue = TRUE;
			break;

		case WM_COMMAND:
			switch (LOWORD(pWParam)) {
				case IDCANCEL:
					EndDialog(pWindow, IDCANCEL);
					KillTimer(pWindow, OP_TIMEOUT_EVENT);
					gScoreRequest.Clear();

					lReturnValue = TRUE;
					break;
			}
			break;
	}
	return lReturnValue;
}

BOOL MR_SendRaceResult(HWND pParentWindow, const char *pTrack,
                       int pBestLapTime, int pMajorID, int pMinorID,
                       const char *pAlias, unsigned int pTrackSum,
                       int pHoverModel, int pTotalTime,
                       int pNbLap, int pNbPlayer, RoomListPtr roomList)
{
	BOOL lReturnValue = FALSE;

	if (roomList != NULL && !roomList->GetScoreServer().path.empty()) {
		// Create the RequestStrign
		gScoreRequestStr = boost::str(
			boost::format("%s?=RESULT%%%%%u%%%%%s%%%%%s%%%%%u%%%%%d%%%%%d%%%%%d%%%%%d") %
			roomList->GetScoreServer().path % pBestLapTime %
			MR_Pad(pTrack) % MR_Pad(pAlias) %
			pTrackSum % pHoverModel % pTotalTime % pNbLap % pNbPlayer);

		lReturnValue = DialogBoxParamW(GetModuleHandle(NULL),
			MAKEINTRESOURCEW(IDD_NET_PROGRESS),
			pParentWindow, UpdateScoresCallBack,
			reinterpret_cast<LPARAM>(roomList.get())) == IDOK;

	}
	return lReturnValue;
}

std::string MR_Pad(const char *pStr)
{
	static boost::format nfmt("%%%02x");
	std::string lReturnValue;

	while(*pStr != 0) {
		/*
		if(isalnum(*(const unsigned char *) pStr)) {
			lReturnValue += *pStr;
		}
		*/
		/*
		else if((*(const unsigned char *) pStr) <= 32) {
			lReturnValue += "%20";
		}
		*/
		/*
		else {
		*/
			/*
			switch (*(const unsigned char *) pStr) {
				case '$':
				case '-':
				case '_':
				case '.':
				case '+':
				case '!':
				case '*':
				case '\'':
				case '(':
				case ')':
				case ',':
				case ':':
				case '@':
				case '&':
				case '=':
				case 187: // don't strip the prompt character anymore

					lReturnValue += *pStr;
					break;

				default:
				{
					*/
					unsigned char c = *pStr;
					lReturnValue += (nfmt % (int)c).str();
					/*
				}
			}
			*/
		/*
		}
		*/
		pStr++;
	}

	return lReturnValue;

}

std::string GetLine(const char *pSrc)
{
	return std::string(pSrc, GetLineLen(pSrc));
}

/**
 * Count the number of characters in the line up to the first newline.
 * @param pSrc The string (may by @c NULL).
 * @return The number of characters (never negative).
 */
int GetLineLen(const char *pSrc)
{
	int lReturnValue = 0;

	if(pSrc != NULL) {
		const char *s = pSrc;
		for (;;) {
			char c = *s;
			if (c == '\0' || c == '\r' || c == '\n') break;
			++lReturnValue;
			++s;
		}
	}
	return lReturnValue;
}

const char *GetNextLine(const char *pSrc)
{
	const char *lReturnValue = NULL;

	if(pSrc != NULL) {
		lReturnValue = pSrc + GetLineLen(pSrc);

		if(*lReturnValue == 0) {
			lReturnValue = NULL;
		}
		else {
			lReturnValue++;
		}
	}
	return lReturnValue;
}

int FindFocusItem(HWND pWindow)
{
	int lReturnValue = -1;

	int lCount = ListView_GetItemCount(pWindow);

	for(int lCounter = 0; (lReturnValue == -1) && (lCounter < lCount); lCounter++) {
		if(ListView_GetItemState(pWindow, lCounter, LVIS_FOCUSED) == LVIS_FOCUSED) {
			LV_ITEM lItemData;

			lItemData.mask = LVIF_PARAM;
			lItemData.iItem = lCounter;
			lItemData.iSubItem = 0;

			if(ListView_GetItem(pWindow, &lItemData)) {
				lReturnValue = lItemData.lParam;
			}
		}
	}

	return lReturnValue;
}

}  // namespace Client
}  // namespace HoverRace
