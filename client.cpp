#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

int test_command(SOCKET ConnectSocket, std::string_view command, std::string_view response)
{
	char recvbuf[DEFAULT_BUFLEN];
	std::string testResponse;
	int iResult = send(ConnectSocket, command.data(), command.length(), 0);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "send failed with error: " << WSAGetLastError() << '\n';
		return iResult;
	}
	iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
	if (iResult < 0)
	{
		std::cout << "recv failed with error: " << WSAGetLastError() << '\n';
		return iResult;
	}
	recvbuf[iResult] = 0;
	
	testResponse = recvbuf;
	assert(testResponse == response);
	return iResult;
}

int test_command(SOCKET ConnectSocket, std::string_view command, const std::vector<std::string_view>& responses)
{
	char recvbuf[DEFAULT_BUFLEN];
	std::string testResponse;
	int iResult = send(ConnectSocket, command.data(), command.length(), 0);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "send failed with error: " << WSAGetLastError() << '\n';
		return iResult;
	}
	for (auto response : responses)
	{
		iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
		if (iResult < 0)
		{
			std::cout << "recv failed with error: " << WSAGetLastError() << '\n';
			return iResult;
		}
		recvbuf[iResult] = 0;
	
		testResponse = recvbuf;
		assert(testResponse == response);
	}
	
	return iResult;
}

int main()
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	int iResult;
	char recvbuf[DEFAULT_BUFLEN];
	
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		std::cout << "WSAStartup failed with error: " << iResult << '\n';
		return 1;
	}
	
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		std::cout << "getaddrinfo failed with error: " << iResult << '\n';
		WSACleanup();
		return 1;
	}
	
	for (ptr=result; ptr != NULL; ptr = ptr->ai_next)
	{
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET)
		{
			std::cout << "socket failed with error: " << WSAGetLastError() << '\n';
			WSACleanup();
			return 1;
		}
		
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR)
		{
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}
	
	freeaddrinfo(result);
	
	if (ConnectSocket == INVALID_SOCKET)
	{
		std::cout << "Unable to connect to server!\n";
		WSACleanup();
		return 1;
	}
	
	// Run tests here
	
	iResult = test_command(ConnectSocket, "GET red\n", "ANSWER A color\r\n");
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	iResult = test_command(ConnectSocket, "ALL\n", {"puppy: A young dog\r\n", "red: A color\r\n"});
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	iResult = send(ConnectSocket, "CLEAR\n", 6, 0);
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	iResult = test_command(ConnectSocket, "ALL\n", "Dictionary is empty\r\n");
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	iResult = send(ConnectSocket, "SET blue Another color\n", 23, 0);
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	iResult = test_command(ConnectSocket, "GET blue\n", "ANSWER Another color\r\n");
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	iResult = test_command(ConnectSocket, "blah\n", "ERROR unknown command\r\n");
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	iResult = test_command(ConnectSocket, "QUIT\n", "Goodbye!\r\n");
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	std::cout << "All tests passed.\n";
	
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "shutdown failed with error: " << WSAGetLastError() << '\n';
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	do
	{
		iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
		if (iResult > 0)
		{
			std::cout << "Bytes received: " << iResult << '\n';
		}
		else if (iResult == 0)
		{
			std::cout << "Connection closed\n";
		}
		else
		{
			std::cout << "recv failed with error: " << WSAGetLastError() << '\n';
		}
	} while (iResult > 0);
	
	closesocket(ConnectSocket);
	WSACleanup();
}