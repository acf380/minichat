#include "TCP_server.h"


SOCKET users[MAX_NUM_OF_USERS];
std::stack<int> free_indexes;
HANDLE send_protect[MAX_NUM_OF_USERS];
HANDLE wrSemaphore, rsSemaphore;
int readers;


void TCP_server::_initialize_Variables()
{
	_ListenSocket = INVALID_SOCKET;
	_result = NULL;

    for (int i = MAX_NUM_OF_USERS - 1; i >= 0; i--) free_indexes.push(i);

    wrSemaphore = CreateSemaphore(NULL, 1,  1,  NULL);
    if (wrSemaphore == NULL)
    {
        printf("CreateSemaphore error: %d\n", GetLastError());
        exit(-1);
    }
    rsSemaphore = CreateSemaphore(NULL, 1, 1, NULL);
    if (rsSemaphore == NULL)
    {
        printf("CreateSemaphore error: %d\n", GetLastError());
        exit(-1);
    }

    for (int i = 0; i < MAX_NUM_OF_USERS; i++)
    {
        send_protect[i] = CreateSemaphore(NULL, 1, 1, NULL);
        if (send_protect[i] == NULL)
        {
            printf("CreateSemaphore error: %d\n", GetLastError());
            exit(-1);
        }
    }
}

int TCP_server::_initialize_Winsock()
{
    int iResult = 0;
    iResult = WSAStartup(MAKEWORD(2, 2), &_wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&_hints, sizeof(_hints));
    _hints.ai_family = AF_INET;
    _hints.ai_socktype = SOCK_STREAM;
    _hints.ai_protocol = IPPROTO_TCP;
    _hints.ai_flags = AI_PASSIVE;

	return 0;
}

int TCP_server::_create_Socket()
{
    int iResult = 0;
    iResult = getaddrinfo(NULL, PORT, &_hints, &_result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    _ListenSocket = socket(_result->ai_family, _result->ai_socktype, _result->ai_protocol);
    if (_ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(_result);
        WSACleanup();
        return 1;
    }

    iResult = bind(_ListenSocket, _result->ai_addr, (int)_result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(_result);
        closesocket(_ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(_result);

    return 0;
}

int TCP_server::_listen()
{
    int iResult = 0;
    iResult = listen(_ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(_ListenSocket);
        WSACleanup();
        return 1;
    }
    return 0;
}

int TCP_server::_main_Loop()
{
    HANDLE hThreadArray[MAX_THREADS]{};
    for (int i=0; i< MAX_THREADS; i++)
    {
        SOCKET ClientSocket = accept(_ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(_ListenSocket);
            WSACleanup();
            return 1;
        }

        hThreadArray[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_client_Loop, (void*)ClientSocket, 0, 0);
        if (hThreadArray[i] == NULL)
        {
            puts("Thread ERROR!");
            ExitProcess(1);
        }
        
    }

    WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);
    for (int i = 0; i < MAX_THREADS; i++) CloseHandle(hThreadArray[i]);
    
    closesocket(_ListenSocket);

    return 0;
}

int TCP_server::_shutdown()
{
    WSACleanup();
    return 0;
}

int __stdcall TCP_server::_client_Loop(SOCKET ClientSocket)
{
    bool access = true;
    int idx = -1;
    
    if (_login(ClientSocket, idx) != 0) access = false;

    if (access)
    {
        _back_Info("Welcome to chat room!\n", ClientSocket, idx);
        int iSendResult = 0;
        char recvbuf[BUFFER_LENGTH]{};
        int recvbuflen = BUFFER_LENGTH;
        int iResult = 0;

        do
        {
            memset(recvbuf, '\0', BUFFER_LENGTH);
            iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            if (iResult > 0)
            {
                _pass_Message(recvbuf, idx);
            
                continue;
            }
            else if (iResult == 0)
                printf("Connection closing...\n");
            else
            {
                printf("recv failed with error: %d\n", WSAGetLastError());
                break;
            }

        } while (iResult > 0);

        if (idx != -1) _logout(ClientSocket, idx);

        iResult = shutdown(ClientSocket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            printf("shutdown failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }
    }

    closesocket(ClientSocket);

    return 0;
}

int TCP_server::_login(SOCKET ClientSocket, int& idx)
{
    char data[BUFFER_LENGTH]{};
    int iResult = recv(ClientSocket, data, BUFFER_LENGTH, 0);
    if (iResult > 0)
    {
        char magic[] = "SauvtU6sxCx4fCnKq";
        if (strncmp(magic, data, (int)strlen(magic)) != 0) return 1;

        // add name & socket to array
        DWORD res = 0;
        res = WaitForSingleObject(wrSemaphore, INFINITE);
        if (res == WAIT_OBJECT_0)
        {
            if (free_indexes.empty()) return 1;
            else
            {
                idx = free_indexes.top();
                free_indexes.pop();
                users[idx] = ClientSocket;
            }

            if (!ReleaseSemaphore(wrSemaphore, 1, NULL))
            {
                printf("ReleaseSemaphore error: %d\n", GetLastError());
                return 1;
            }
        }
    }
    else if (iResult != 0)
    {
        printf("recv failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    return 0;
}

int TCP_server::_logout(SOCKET ClientSocket, int idx)
{
    // add name & socket to array
    DWORD res = 0;
    res = WaitForSingleObject(wrSemaphore, INFINITE);
    if (res == WAIT_OBJECT_0)
    {
        users[idx] = NULL;
        free_indexes.push(idx);

        if (!ReleaseSemaphore(wrSemaphore, 1, NULL))
        {
            printf("ReleaseSemaphore error: %d\n", GetLastError());
            return 1;
        }
    }

    return 0;
}

int TCP_server::_pass_Message(char* mess, int idx)
{
    DWORD res1 = 0, res2 = 0, res3 = 0;

    res1 = WaitForSingleObject(rsSemaphore, INFINITE);
    if (res1 == WAIT_OBJECT_0)
    {
        readers++;
        if (readers == 1) res2 = WaitForSingleObject(wrSemaphore, INFINITE);

        if (!ReleaseSemaphore(rsSemaphore, 1, NULL))
        {
            printf("ReleaseSemaphore error: %d\n", GetLastError());
            return 1;
        }
    }
    if (res2 == WAIT_OBJECT_0)
    {
        for (int i = 0; i < MAX_NUM_OF_USERS; i++)
        {
            //if (i == idx) continue;
            SOCKET RecvSocket = users[i];
            if (RecvSocket == NULL) continue;

            res3 = WaitForSingleObject(send_protect[i], INFINITE);
            if (res3 == WAIT_OBJECT_0)
            {
                //std::cout << "after: " << mess << std::endl;
                int iResult = send(RecvSocket, mess, BUFFER_LENGTH, 0);
                if (iResult == SOCKET_ERROR) {
                    printf("send failed with error: %d\n", WSAGetLastError());
                    closesocket(RecvSocket);
                    WSACleanup();
                    return 1;
                }

                if (!ReleaseSemaphore(send_protect[i], 1, NULL))
                {
                    printf("ReleaseSemaphore error: %d\n", GetLastError());
                    return 1;
                }
            }
        }

    }
    res1 = WaitForSingleObject(rsSemaphore, INFINITE);
    if (res1 == WAIT_OBJECT_0)
    {
        readers--;
        if (readers == 0)
        {
            if (!ReleaseSemaphore(wrSemaphore, 1, NULL))
            {
                printf("ReleaseSemaphore error: %d\n", GetLastError());
                return 1;
            }
        }

        if (!ReleaseSemaphore(rsSemaphore, 1, NULL))
        {
            printf("ReleaseSemaphore error: %d\n", GetLastError());
            return 1;
        }
    }

    return 0;
}

int TCP_server::_back_Info(const char* mess, SOCKET ClientSocket, int idx)
{
    int res = WaitForSingleObject(send_protect[idx], INFINITE);
    if (res == WAIT_OBJECT_0)
    {
        int iResult = send(ClientSocket, mess, (int)strlen(mess), 0);
        if (iResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }

        if (!ReleaseSemaphore(send_protect[idx], 1, NULL))
        {
            printf("ReleaseSemaphore error: %d\n", GetLastError());
            return 1;
        }
    }

    return 0;
}

TCP_server::TCP_server()
{
	_initialize_Variables();
}

TCP_server::~TCP_server()
{
    CloseHandle(wrSemaphore);
    CloseHandle(rsSemaphore);

    for (int i = 0; i < MAX_NUM_OF_USERS; i++) CloseHandle(send_protect[i]);
}

int TCP_server::run()
{
    if (_initialize_Winsock() != 0) return 1;
    if (_create_Socket() != 0)      return 2;
    if (_listen() != 0)             return 3;
    if (_main_Loop() != 0)          return 4;
    if (_shutdown() != 0)           return 5;
    
    return 0;
}
