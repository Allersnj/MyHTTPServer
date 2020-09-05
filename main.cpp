#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

/** the length of the receiving buffers. */
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
 * This struct holds the configurable options in one convenient spot.
 */
struct Config
{
	/** the host name of the server. */
	std::string host = "localhost";
	/** the port of the server. */
	std::string port = "8080";
	/** the root directory of files accessible by the server. */
	std::string web_directory = "www";
};

/**
 * This function contains the boilerplate Winsock code to create a socket that listens for clients.
 * @param options the options used to configure the socket.
 * @return the socket that listens for clients.
 */
SOCKET getListenSocket(const Config& options)
{
	struct addrinfo *result = NULL, hints;
	
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;       // IPv4
	hints.ai_socktype = SOCK_STREAM; // Streaming socket
	hints.ai_protocol = IPPROTO_TCP; // TCP/IP
	hints.ai_flags = AI_PASSIVE;     // Socket will be bound to an address.
	
	// Get the addresses of the server.
	int iResult = getaddrinfo(NULL, options.port.c_str(), &hints, &result);
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
	else if (ext == ".js")
	{
		return "application/javascript";
	}
	else if (ext == ".svg")
	{
		return "image/svg+xml";
	}
	else if (ext == ".css")
	{
		return "text/css;charset=UTF-8";
	}
	else if (ext == ".png")
	{
		return "image/png";
	}
	return "text/plain";
}

/** This function is what handles each client's commands.
 * @param ClientSocket the socket representing the connection to the client.
 * @param web_directory the string representing the root directory of accessible files.
 */
void serve(SOCKET ClientSocket, std::string web_directory)
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
	
	iResult = recv(ClientSocket, recvbuf.data(), DEFAULT_BUFLEN, 0);
	if (iResult > 0)
	{
		std::stringstream stream{recvbuf.data()};
		stream >> command >> resource >> version;
		std::cout << command << ' ' << resource << ' ' << version << '\n';
		if (std::find(commands.begin(), commands.end(), command) == commands.end())
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
		}	
		stream >> std::ws;
		bool fail = false;
		while (!stream.eof() && stream.peek() != '\r')
		{
			std::string temp;
			stream >> temp;
			std::cout << temp;
			if (!temp.ends_with(':'))
			{
				fail = true;
				break;
			}
			stream >> std::ws;
			std::getline(stream, temp, '\r');
			std::cout << ' ' << temp << '\n';
			stream.get(junk);
		}
		if (stream.fail() || fail)
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
		}
		else
		{
			stream >> std::ws;
			if (!stream.eof())
			{
				body = stream.str().substr(stream.tellg());
				std::cout << body << '\n';
			}
			
			if (command == "GET")
			{
				std::filesystem::path filepath = web_directory + resource;
				if (!std::filesystem::relative(filepath).parent_path().string().starts_with(web_directory))
				{
					resultStr = "HTTP/1.0 403 Forbidden\r\n";
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
					std::ifstream file;
					std::string pathString = filepath.string();
					std::string::size_type query_pos = pathString.find_first_of('?');
					if (query_pos != std::string::npos)
					{
						file.open(pathString.substr(0, query_pos));
					}
					else
					{
						file.open(pathString);
					}
					
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
			else
			{
				resultStr = "HTTP/1.0 400 Bad Request\r\n";
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
	
	
	iResult = shutdown(ClientSocket, SD_BOTH);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "shutdown failed with error: " << WSAGetLastError() << '\n';
		closesocket(ClientSocket);
		WSACleanup();
		return;
	}
	
	closesocket(ClientSocket);
}

/**
 * Loads the configuration from a file named "config" in the working directory.
 * @param options The options struct to populate.
 */
void loadConfig(Config& options)
{
	std::ifstream config("config");
	std::string option;
	std::string value;
	if (config)
	{
		while (config >> option >> value)
		{
			if (option == "port:")
			{
				options.port = value;
			}
			else if (option == "host:")
			{
				options.host = value;
			}
			else if (option == "web_directory:")
			{
				options.web_directory = value;
			}
			else
			{
				std::cout << "Unknown option " << option << "ignored.\n";
			}
		}
	}
	else
	{
		std::cout << "No config file found, using defaults...\n";
	}
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
	
	Config options;
	loadConfig(options);
	
	SOCKET ListenSocket = getListenSocket(options);
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
		std::thread t(serve, ClientSocket, options.web_directory);
		t.detach();
	}
	
	WSACleanup();
}
