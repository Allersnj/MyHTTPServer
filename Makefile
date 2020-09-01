objects = main.o

CXXFLAGS = -isystem "C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0"

LDFLAGS = -L"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.18362.0\um\x64"

libraries = -lWS2_32 

server.exe : $(objects)
	$(CXX) $(LDFLAGS) -o server.exe $(objects) $(libraries)

main.o : main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp
