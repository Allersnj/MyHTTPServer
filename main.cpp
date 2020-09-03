#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>

#define DEFAULT_PORT "8080"
#define DEFAULT_BUFLEN 512

/** @file main.cpp
 *  @brief The source for the main server.
 *
 *  This is where the magic happens. The main loop for the server and some helper functions are in here.
 */


/** 
 * This is the dictionary to hold definitions for access by clients.
 */
std::map<std::string, std::string> dictionary;

/** 
 * This mutex protects the dictionary from simultaneous access.
 */
std::mutex dictionary_mutex;

/**
 * This function contains the boilerplate Winsock code to create a socket that listens for clients.
 * @return the socket that listens for clients.
 */
SOCKET getListenSocket()
{
	struct addrinfo *result = NULL, hints;
	
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;       // IPv4
	hints.ai_socktype = SOCK_STREAM; // Streaming socket
	hints.ai_protocol = IPPROTO_TCP; // TCP/IP
	hints.ai_flags = AI_PASSIVE;     // Socket will be bound to an address.
	
	// Get the addresses of the server.
	int iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		std::cout << "getaddrinfo failed: " << iResult << '\n';
		WSACleanup();
		return INVALID_SOCKET;
	}
	
	SOCKET ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET)
	{
		std::cout << "Error at socket(): " << WSAGetLastError() << '\n';
		WSACleanup();
		return INVALID_SOCKET;
	}
	
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "bind failed with error: " << WSAGetLastError() << '\n';
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}
	
	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cout << "Listen failed with error: " << WSAGetLastError() << '\n';
		closesocket(ListenSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}
	
	return ListenSocket;
}


/** This function is what handles each client's commands.
 * @param ClientSocket the socket representing the connection to the client.
 */
void serve(SOCKET ClientSocket)
{
	char recvbuf[DEFAULT_BUFLEN];
	int iResult, iSendResult;
	std::string command;
	std::string resultStr;
	bool quit = false;
	
	do
	{
		iResult = recv(ClientSocket, recvbuf, DEFAULT_BUFLEN, 0);
		
		if (iResult > 0)
		{
			std::cout << "Bytes received: " << iResult << '\n';
			recvbuf[iResult] = 0; // Always null-terminate your strings.
			std::stringstream stream{recvbuf};
			// Make sure multiple commands from one recv are accounted for.
			while (stream >> command)
			{
				if (command == "GET")
				{
					stream >> command;
					if (dictionary.count(command))
					{
						const std::lock_guard<std::mutex> lock(dictionary_mutex);
						resultStr = "ANSWER " + dictionary[command] + "\r\n";
					}
					else
					{
						resultStr = "ERROR can't find " + command + "\r\n";
					}
					iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
					if (iSendResult == SOCKET_ERROR)
					{
						std::cout << "send failed: " << WSAGetLastError() << '\n';
						closesocket(ClientSocket);
						WSACleanup();
						return;
					}
					std::cout << "Bytes sent: " << iSendResult << '\n';
				}
				else if (command == "SET")
				{
					stream >> command;
					stream.get();
					stream.getline(recvbuf, DEFAULT_BUFLEN, '\n');
					const std::lock_guard<std::mutex> lock(dictionary_mutex);
					dictionary[command] = recvbuf;
				}
				else if (command == "CLEAR")
				{
					const std::lock_guard<std::mutex> lock(dictionary_mutex);
					dictionary.clear();
				}
				else if (command == "ALL")
				{
					const std::lock_guard<std::mutex> lock(dictionary_mutex);
					if (dictionary.size() == 0)
					{
						resultStr = "Dictionary is empty\r\n";
						iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
						if (iSendResult == SOCKET_ERROR)
						{
							std::cout << "send failed: " << WSAGetLastError() << '\n';
							closesocket(ClientSocket);
							WSACleanup();
							return;
						}
						std::cout << "Bytes sent: " << iSendResult << '\n';
					}
					else
					{
						for (const auto& [key, value] : dictionary)
						{
							resultStr = key + ": " + value + "\r\n";
							iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
							if (iSendResult == SOCKET_ERROR)
							{
								std::cout << "send failed: " << WSAGetLastError() << '\n';
								closesocket(ClientSocket);
								WSACleanup();
								return;
							}
							std::cout << "Bytes sent: " << iSendResult << '\n';
						}
					}
				}
				else if (command == "")
				{
					continue;
				}
				else if (command == "QUIT")
				{
					resultStr = "Goodbye!\r\n";
					iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
					if (iSendResult == SOCKET_ERROR)
					{
						std::cout << "send failed: " << WSAGetLastError() << '\n';
						closesocket(ClientSocket);
						WSACleanup();
						return;
					}
					iResult = shutdown(ClientSocket, SD_BOTH);
					if (iResult == SOCKET_ERROR)
					{
						std::cout << "shutdown failed with error: " << WSAGetLastError() << '\n';
						closesocket(ClientSocket);
						WSACleanup();
						return;
					}
					quit = true;
					break;
				}
				else
				{
					resultStr = "ERROR unknown command\r\n";
					iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
					if (iSendResult == SOCKET_ERROR)
					{
						std::cout << "send failed: " << WSAGetLastError() << '\n';
						closesocket(ClientSocket);
						WSACleanup();
						return;
					}
					std::cout << "Bytes sent: " << iSendResult << '\n';
				}
				command = "";
			}
		}
		else if (iResult == 0)
		{
			std::cout << "Connection closing...\n";
		}
		else
		{
			std::cout << "recv failed: " << WSAGetLastError() << '\n';
			closesocket(ClientSocket);
			WSACleanup();
			return;
		}
			
	} while (iResult > 0 && !quit);
	
	std::cout << "Connection closed.\n";
	
	closesocket(ClientSocket);
}


/** 
 * This is main, man. It sets up initial values for the dictionary, intializes and cleans up Winsock, and starts the loop to accept connections.
 */
int main()
{
	dictionary["red"] = "A color";
	dictionary["puppy"] = "A young dog";
	
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0)
	{
		std::cout << "WSAStartup failed: " << iResult << '\n';
		return 1;
	}
	
	SOCKET ListenSocket = getListenSocket();
	while (true)
	{
		SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET)
		{
			std::cout << "accept failed: " << WSAGetLastError() << '\n';
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
		std::thread t(serve, ClientSocket);
		t.detach();
	}
	
	WSACleanup();
}
