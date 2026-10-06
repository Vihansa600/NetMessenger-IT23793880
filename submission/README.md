NETMESSENGER - IE3010 NETWORK PROGRAMMING 
 
Student Registration Number: 
IT23793880 
 
 
PERSONALISATION 
 
Full Registration Number: 
IT23793880 
 
Numeric Part: 
23793880 
 
Last Four Digits: 
3880 
 
Server/Client Port: 
6000 + 3880 = 9880 
 
Node ID: 
Digits 3-6 of 23793880 = 7938 
 
NID: 
NID:7938 
 
 
SOURCE FILES 
 
Server: 
server_3880.c 
 
Client: 
client_3880.c 
 
Makefile: 
Makefile_3880 
 
 
LOG FILE 
 
netmsg_IT23793880.log 
 
 
FILE STORAGE 
 
./storage/IT23793880/<sender_username>/<filename> 
 
 
SUBMISSION ARCHIVE 
 
IE3010_IT23793880.zip 
 
 
IMPLEMENTED FEATURES 
 
1. Multiple simultaneous TCP clients 
2. Client registration 
3. User listing 
4. Broadcast messaging 
5. Private messaging 
6. Room creation/joining 
7. Leaving rooms 
8. Room listing 
9. Room messaging 
10. Graceful client disconnection 
11. File sharing 
12. Personalised file storage 
13. Server logging 
14. NID-tagged server responses 
 
 
SUPPORTED COMMANDS 
 
REGISTER <username> 
LIST 
BCAST <message> 
PMSG <username> <message> 
JOIN <room> 
LEAVE <room> 
ROOMS 
RMSG <room> <message> 
SENDFILE <target> <filename> 
QUIT 
 
 
FILE TRANSFER EXAMPLE 
 
SENDFILE receiver test.txt 
 
The file is stored using: 
 
./storage/IT23793880/sender/test.txt 
 
 
TESTING 
 
The system was tested with multiple TCP clients. 
 
The following features were tested: 
REGISTER 
LIST 
BCAST 
PMSG 
JOIN 
ROOMS 
RMSG 
LEAVE 
SENDFILE 
 
A test file named test.txt was successfully transferred and stored under the personalised storage directory. 
 
 
BUILD 
 
Compile the complete project using: 
 
make -f Makefile_3880 
 
Clean compiled programs using: 
 
make -f Makefile_3880 clean 
 
 
RUN SERVER 
 
./server_3880 
 
 
RUN CLIENT 
 
./client_3880 
 
 
PORT 
 
9880 
 
 
NID 
 
7938
