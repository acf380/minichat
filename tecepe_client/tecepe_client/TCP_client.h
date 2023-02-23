#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <iostream>



// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define BUFFER_LENGTH 72
#define PORT "27015"
#define ADDRESS_IP "127.0.0.1"
#define MAX_LINES 20;
#define TYPE_LINE 22;


class TCP_client
{
private:
	void _initialize_Variables();
	int _setup_Conection();
	int _main_loop();
	static int __stdcall _send_data();
	static int  __stdcall _recv_data();
	int _shutdown();
	int _get_login();
	int _greeting();
	static void _make_chat_design();


public:
	TCP_client();
	virtual ~TCP_client();

	int run();
};

