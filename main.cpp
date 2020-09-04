#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <winsock2.h>
#include <ws2tcpip.h>

#define DEFAULT_PORT "8080"
#define DEFAULT_BUFLEN 1024

/** @file main.cpp
 *  @brief The source for the main server.
 *
 *  This is where the magic happens. The main loop for the server and some helper functions are in here.
 */


/**
 * An array of known HTTP comands.
 */
const std::array<std::string, 8> commands{"GET", "PUT", "HEAD", "POST", "DELETE", "OPTIONS", "TRACE", "CONNECT"};

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

/**
 * Helper to get the content type of a response based on the extension of a file.
 * @param file the path to the file being sent.
 */
std::string getContentType(const std::filesystem::path& file)
{
	std::string ext = file.extension().string();
	if (ext == ".txt")
	{
		return "text/plain;charset=UTF-8";
	}
	else if (ext == ".html")
	{
		return "text/html;charset=UTF-8";
	}
	return "text/plain";
}

/** This function is what handles each client's commands.
 * @param ClientSocket the socket representing the connection to the client.
 */
void serve(SOCKET ClientSocket)
{
	std::array<char, DEFAULT_BUFLEN> recvbuf{0};
	int iResult, iSendResult;
	std::string command;
	std::string resource;
	std::string version;
	std::string resultStr;
	std::string body;
	bool quit = false;
	char junk;
	
	do
	{
		iResult = recv(ClientSocket, recvbuf.data(), DEFAULT_BUFLEN, 0);
		if (iResult > 0)
		{
			std::cout << "Bytes received: " << iResult << '\n';
			std::stringstream stream{recvbuf.data()};
			stream >> command >> resource >> version;
			std::cout << "Command: " << command << '\n';
			std::cout << "Resource: " << resource << '\n';
			std::cout << "Version: " << version << '\n';
			if (std::find(commands.begin(), commands.end(), command) == commands.end())
			{
				std::cout << "Unknown command: " << command << '\n';
			}	
			stream >> std::ws;
			while (!stream.eof() && stream.peek() != '\r')
			{
				std::array<char, DEFAULT_BUFLEN> temp;
				stream.getline(temp.data(), DEFAULT_BUFLEN, ':');
				stream >> std::ws;
				std::cout << temp.data();
				stream.getline(temp.data(), DEFAULT_BUFLEN, '\r');
				std::cout << ": " << temp.data() << '\n';
				stream.get(junk);
			}
			if (stream.fail())
			{
				resultStr = "HTTP/1.1 400 Bad Request\r\n";
				iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
				if (iSendResult == SOCKET_ERROR)
				{
					std::cout << "send failed: " << WSAGetLastError() << '\n';
					closesocket(ClientSocket);
					WSACleanup();
					return;
				}
				continue;
			}
			
			stream >> std::ws;
			
			if (command == "GET")
			{
				std::filesystem::path filepath = "www" + resource;
				std::ifstream file(filepath);
				if (file)
				{
					std::ostringstream fileStream;
					fileStream << file.rdbuf();
					std::string responseBody = fileStream.str();
					std::string type = getContentType(filepath);
					
					resultStr = "HTTP/1.0 200 OK\r\nContent-Length: " + std::to_string(responseBody.length()) + "\r\nServer: Custom C++ (Windows)\r\nContent-Type: " + type + "\r\n\r\n" + fileStream.str();
					iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
					if (iSendResult == SOCKET_ERROR)
					{
						std::cout << "send failed: " << WSAGetLastError() << '\n';
						closesocket(ClientSocket);
						WSACleanup();
						return;
					}
				}
				else
				{
					resultStr = "HTTP/1.0 404 File Not Found\r\n";
					iSendResult = send(ClientSocket, resultStr.c_str(), resultStr.length(), 0);
					if (iSendResult == SOCKET_ERROR)
					{
						std::cout << "send failed: " << WSAGetLastError() << '\n';
						closesocket(ClientSocket);
						WSACleanup();
						return;
					}
				}
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
	} while (iResult > 0);
	
	
	iResult = shutdown(ClientSocket, SD_BOTH);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "shutdown failed with error: " << WSAGetLastError() << '\n';
		closesocket(ClientSocket);
		WSACleanup();
		return;
	}
	
	std::cout << "Connection closed.\n";
	
	closesocket(ClientSocket);
}


/** 
 * This is main, man. It intializes and cleans up Winsock and starts the loop to accept connections.
 */
int main()
{	
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
