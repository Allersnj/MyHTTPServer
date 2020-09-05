CXXFLAGS = -isystem "C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0" -std=c++2a

LDFLAGS = -L"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.18362.0\um\x64"

libraries = -lWS2_32 

.PHONY : ALL

ALL : server.exe client.exe

server.exe : main.o
	$(CXX) $(LDFLAGS) -o server.exe main.o $(libraries)

client.exe : client.o
	$(CXX) $(LDFLAGS) -o client.exe client.o $(libraries)

main.o : main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp

client.o : client.cpp
	$(CXX) $(CXXFLAGS) -c client.cpp

