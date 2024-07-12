protoc.exe --proto_path=./../PacketLibrary --cpp_out=./../PacketLibrary ./../PacketLibrary/Protocol.proto
IF ERRORLEVEL 1 PAUSE