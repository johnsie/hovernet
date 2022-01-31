#include <iostream>
#include <winsock.h>

#include <Mmsystem.h>

#include "../../engine/MainCharacter/MainCharacter.h"

#pragma comment(lib, "ws2_32.lib")

int main()
{
	

	SOCKET mUDPRecvSocket;				/// the UDP socket we listen on
	SOCKADDR_IN mUDPRemoteAddr;
	mUDPRecvSocket = socket(PF_INET, SOCK_DGRAM, 0);
	//ASSERT(mUDPRecvSocket != INVALID_SOCKET);

	// make it non-blocking
	int lCode;
	unsigned long lNonBlock = TRUE;
	lCode = ioctlsocket(mUDPRecvSocket, FIONBIO, &lNonBlock);
	//	ASSERT(lCode != SOCKET_ERROR);

		// Bind the socket to the default port
	SOCKADDR_IN lLocalAddr;
	lLocalAddr.sin_family = AF_INET;
	lLocalAddr.sin_addr.s_addr = INADDR_ANY;
	lLocalAddr.sin_port = 984400;
	lCode = bind(mUDPRecvSocket, (LPSOCKADDR)&lLocalAddr, sizeof(lLocalAddr));
	//	ASSERT(lCode != SOCKET_ERROR);



	while (1)
	{	std::cout << "Hello World!";
		//server.Update(-1, true);
	ReadNet();
//	SUPER::Process(pSpeedFactor);
	WriteNet();
	ReadNet();

	}



	return 0;
}

void ReadNet()
{

}


void  WriteNet() {



}
