protoc.exe --proto_path=./../PacketLibrary --proto_path=./../ImportLibrary/Include --cpp_out=./../PacketLibrary ./../PacketLibrary/Protocol.proto
IF ERRORLEVEL 1 PAUSE