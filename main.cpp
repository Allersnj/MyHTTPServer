#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

SOCKET getListenSocket()
{
	struct addrinfo *result = NULL, hints;
	
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	
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

int main()
{
	
	std::map<std::string, std::string> dictionary;
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
	
	SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET)
	{
		std::cout << "accept failed: " << WSAGetLastError() << '\n';
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	
	char recvbuf[DEFAULT_BUFLEN];
	int iSendResult;
	std::string command;
	std::string resultStr;
	
	do
	{
		iResult = recv(ClientSocket, recvbuf, DEFAULT_BUFLEN, 0);
		if (iResult > 0)
		{
			std::cout << "Bytes received: " << iResult << '\n';
			recvbuf[iResult] = 0; // Always null-terminate your strings.
			std::stringstream stream{recvbuf};
			while (stream >> command)
			{
				if (command == "GET")
				{
					stream >> command;
					if (dictionary.count(command))
					{
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
						return 1;
					}
					std::cout << "Bytes sent: " << iSendResult << '\n';
				}
				else if (command == "SET")
				{
					stream >> command;
					stream.get();
					stream.getline(recvbuf, DEFAULT_BUFLEN, '\n');
					dictionary[command] = recvbuf;
				}
				else if (command == "CLEAR")
				{
					dictionary.clear();
				}
				else if (command == "ALL")
				{
					if (dictionary.size() == 0)
					{
						resultStr = "Dictionary is empty\r\n";
						iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
						if (iSendResult == SOCKET_ERROR)
						{
							std::cout << "send failed: " << WSAGetLastError() << '\n';
							closesocket(ClientSocket);
							WSACleanup();
							return 1;
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
								return 1;
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
						return 1;
					}
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
						return 1;
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
			return 1;
		}
			
	} while (iResult > 0);
	
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "shutdown failed with error: " << WSAGetLastError() << '\n';
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}
	
	closesocket(ClientSocket);
	WSACleanup();
}
