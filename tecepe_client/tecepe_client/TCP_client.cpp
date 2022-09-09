#include "TCP_client.h"


SOCKET ConnectSocket;
char sendbuf[BUFFER_LENGTH];
char recvbuf[BUFFER_LENGTH];
char username[12]{};
int line = 0;

HANDLE hStdout, sTerminalOutput, sLine, console;
COORD destCoord;

void TCP_client::_initialize_Variables()
{
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    sTerminalOutput = CreateSemaphore(NULL, 1, 1, NULL);
    if (sTerminalOutput == NULL)
    {
        printf("CreateSemaphore error: %d\n", GetLastError());
        exit(-1);
    }

    sLine= CreateSemaphore(NULL, 1, 1, NULL);
    if (sLine == NULL)
    {
        printf("CreateSemaphore error: %d\n", GetLastError());
        exit(-1);
    }
}

int TCP_client::_setup_Conection()
{
    WSADATA wsaData{};
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    char recvbuf[BUFFER_LENGTH]{};
    int iResult;
    int recvbuflen = BUFFER_LENGTH;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(ADDRESS_IP, PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    if (_greeting() != 0) return 1;
    Sleep(3000);
    _make_chat_design();

	return 0;
}

int TCP_client::_main_loop()
{
    HANDLE thread_sr[2]{};
    
    thread_sr[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_send_data, 0, 0, 0);
    if (thread_sr[0] == nullptr)
    {
        puts("Thread ERROR!");
        ExitProcess(1);
    }

    thread_sr[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_recv_data, 0, 0, 0);
    if (thread_sr[1] == nullptr)
    {
        puts("Thread ERROR!");
        ExitProcess(1);
    }

    WaitForMultipleObjects(2, thread_sr, TRUE, INFINITE);
    for (int i = 0; i < 2; i++) CloseHandle(thread_sr[i]);

    return 0;
}

int TCP_client::_send_data()
{
    int iResult = 0;
    int res = 0;
    char c = '\0';

    while (true)
    {
        if (_kbhit())
        {
            res = WaitForSingleObject(sTerminalOutput, INFINITE);
            if (res == WAIT_OBJECT_0)
            {
                destCoord.X = 0, destCoord.Y = TYPE_LINE;
                SetConsoleCursorPosition(hStdout, destCoord);
                printf("\33[2K\r");
                printf("type here: ");
                
                memcpy(sendbuf, username, (int)strlen(username));
                fseek(stdin, 0, SEEK_END);
                
                int it = (int)strlen(username);
                sendbuf[it++] = ' ';
                while ((it < BUFFER_LENGTH) && (c = getchar()) && (c != '\n')) sendbuf[it++] = c;
                sendbuf[BUFFER_LENGTH - 2] = '\n';
                sendbuf[BUFFER_LENGTH - 1] = '\0';
               

                if (!ReleaseSemaphore(sTerminalOutput, 1, NULL))
                {
                    printf("ReleaseSemaphore error: %d\n", GetLastError());
                    return 1;
                }
            }

            iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
            if (iResult == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(ConnectSocket);
                WSACleanup();
                return 1;
            }

            memset(sendbuf, '\0', BUFFER_LENGTH);

        }
    }
    return 0;
}

int TCP_client::_recv_data()
{
    int iResult = 0;
    int res = 0;

    while (true)
    {
        iResult = recv(ConnectSocket, recvbuf, BUFFER_LENGTH, 0);
        if (iResult > 0)
        {
            res = WaitForSingleObject(sTerminalOutput, INFINITE);
            if (res == WAIT_OBJECT_0)
            {
                int max_lines = MAX_LINES;
                if (line >= max_lines) _make_chat_design();

                destCoord.X = 0, destCoord.Y = line++;
                SetConsoleCursorPosition(hStdout, destCoord);
                printf("%s", recvbuf);

                if (!ReleaseSemaphore(sTerminalOutput, 1, NULL))
                {
                    printf("ReleaseSemaphore error: %d\n", GetLastError());
                    return 1;
                }
            }

        }
        else if (iResult == 0)
            printf("Connection closed\n");
        else
        {
            printf("recv failed with error: %d\n", WSAGetLastError());
            return 1;
        }
    }

    return 0;
}

int TCP_client::_shutdown()
{
    int iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}

int TCP_client::_get_login()
{
    printf("username: ");
    fgets(username, 10, stdin);

    int it = 0;
    while (it < 9 && username[it] != '\n') it++;
    username[it] = '>'; username[it + 1] = '\0';

    return 0;
}

int TCP_client::_greeting()
{
    char magic[] = "SauvtU6sxCx4fCnKq";
    int iResult = send(ConnectSocket, magic, (int)strlen(magic), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    iResult = recv(ConnectSocket, recvbuf, BUFFER_LENGTH, 0);
    if (iResult > 0) printf("%s", recvbuf);
    else if (iResult != 0) return 1;

    memset(recvbuf, '\0', BUFFER_LENGTH);

    return 0;
}

void TCP_client::_make_chat_design()
{
    system("cls");
    destCoord.X = 0, destCoord.Y = 0;
    SetConsoleCursorPosition(hStdout, destCoord);

    printf("--------------------------------------------------");

    destCoord.X = 0, destCoord.Y = 21;
    SetConsoleCursorPosition(hStdout, destCoord);
    
    printf("--------------------------------------------------\n");
    printf("type here: ");
    line = 1;
}

TCP_client::TCP_client()
{
    _initialize_Variables();
    ConnectSocket = INVALID_SOCKET;


    HWND console = GetConsoleWindow();
    RECT ConsoleRect;
    GetWindowRect(console, &ConsoleRect);
    SetWindowLong(console, GWL_STYLE, GetWindowLong(console, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);
    MoveWindow(console, ConsoleRect.left, ConsoleRect.top, 800, 600, TRUE);
}

TCP_client::~TCP_client()
{
}

int TCP_client::run()
{
    _get_login();
    _setup_Conection();
    _main_loop();
    _shutdown();

    return 0;
}
