#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#define DEFAULT_PORT "8080"
#define DEFAULT_BUFLEN 1024

/** @file client.cpp
 *  @brief An automated client for testing the server.
 *
 *  This client serves as a tester for the server. It has some utility functions for this purpose. Main runs the tests.
 */
 
 
 /**
  * @brief Sends a command to the server.
  * @param ConnectSocket The socket representing the connection to the server.
  * @param command The command to send.
  * Basically a test that doesn't expect to receive anything back.
  */
int send_command(SOCKET ConnectSocket, std::string_view command)
{
	char recvbuf[DEFAULT_BUFLEN];
	std::string testResponse;
	int iResult = send(ConnectSocket, command.data(), command.length(), 0);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "send failed with error: " << WSAGetLastError() << '\n';
		return iResult;
	}
	return iResult;
}
 
 /**
  * @brief Tests a command against the server.
  * @param ConnectSocket The socket representing the connection to the server.
  * @param command The command to test.
  * @param response The expected response from the server.
  */
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

/**
 * @brief An overload to test multiple commands.
 * @param ConnectSocket The socket representing the connection to the server.
 * @param command The command to test.
 * @param responses The expected responses from the server.
 */
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

/**
 * @brief It's main, man. Sets up and cleans up Winsock, and tests the commands.
 */
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
	
	iResult = send_command(ConnectSocket, "GET / HTTP/1.1\r\n");
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