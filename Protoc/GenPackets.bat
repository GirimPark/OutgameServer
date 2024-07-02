protoc.exe --proto_path=./../PacketLibrary --cpp_out=./../PacketLibrary ./../PacketLibrary/Echo.proto
IF ERRORLEVEL 1 PAUSE