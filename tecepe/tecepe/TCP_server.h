#pragma once
#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <stack>
#include <iostream>

#pragma comment (lib, "Ws2_32.lib")


#define BUFFER_LENGTH 72
#define PORT "27015"
#define MAX_THREADS 10
#define MAX_NUM_OF_USERS 10


class TCP_server
{
private:
	WSADATA _wsaData{};
	SOCKET _ListenSocket;
	addrinfo* _result;
	addrinfo _hints{};

	void _initialize_Variables();
	int _initialize_Winsock();
	int _create_Socket();
	int _listen();
	int _main_Loop();
	int _shutdown();

	static int __stdcall _client_Loop(SOCKET ClientSocket);
	static int _login(SOCKET ClientSocket, int& idx);
	static int _logout(SOCKET ClientSocket, int idx);
	static int _pass_Message(char* mess, int idx);
	static int _back_Info(const char* mess, SOCKET ClientSocket, int idx);

public:
	TCP_server();
	virtual ~TCP_server();

	int run();
};

