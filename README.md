IE3090 — Network Programming
RemoteOps: Remote System Monitoring and Management Tool over TCP/IP
Student: Kusal Sanjula
Registration Number: IT24102568
Module: IE3090 — Network Programming
Year: 3rd Year, Semester 1
Assignment: RemoteOps — Part 1 Take-Home Implementation

1. Project Overview

The RemoteOps is a system monitoring and management program implemented in C that makes use of the standard BSD sockets API for remote systems.
This system comprises of two programs:
The program running on the managed Linux machine.The server program running on the managed Linux machine.
Controller — Client program for an Administrator to communicate with the Agent.
The main control communications over IP are done via TCP/IP. If monitoring has been enabled, a secondary UDP channel is used to periodically monitor the system.
The implementation supports:
Allowing multiple connections to a Controller simultaneously.Supporting simultaneous Controller connections.
•	Authentication
•	System information monitoring
•	Process listing
It restricts the ability to execute commands remotely.It limits the capability to execute commands remotely.
•	File upload
•	File download
•	UDP-based periodic monitoring
•	Graceful disconnection
•	Logging
•	Personalised protocol responses

2. Personalisation Details
The implementation is personalised according to the registration number:
Registration Number: IT24102568
Requirement	Calculation / Value
Registration Number	IT24102568
Numeric part	24102568
First four digits	2410
Agent TCP Port	7000 + 2410 = 9410
Last three digits	568
The agent_568.c file is an agent source file.
This file contains the source code for the controller.controller_568.c – contains the source code for the controller.
Makefile	Makefile_568
Last four digits	2568
Reversed last 1 digit	8525
Session ID (SID)	8652
Authentication Token	OPS-2568
Log File	remoteops_568.log
Storage Directory	./agentfiles/IT24102568/
Submission ZIP	IE3090_IT24102568.zip
Every TCP response message that the Agent produces contains the personalised SID tag:
SID:8652


3. Architecture
RemoteOps uses a client/server architecture.
                    TCP/IP
        +----------------------------+
        |                            |
        |       Controller           |
        |      (TCP Client)          |
        |                            |
        +-------------+--------------+
                      |
                      | TCP Commands
                      |
                      v
        +-------------+--------------+
        |                            |
        |          Agent             |
        |       (TCP Server)         |
        |                            |
        |  Port: 9410                |
        |                            |
        +-------------+--------------+
                      |
       +--------------+--------------+
       |              |              |
       v              v              v
   SYSINFO        LISTPROC        EXEC
       |
       +----------------------------+
       |
       v
   File Storage
   ./agentfiles/IT24102568/

       ^
       |
       | UDP Monitoring
       |
       +----------------------------+
                Controller
The Agent listens for incoming TCP connections on port 9410.
Each accepted Controller connection is handled independently using a POSIX thread (pthread). This allows the Agent to serve multiple Controllers concurrently.

4. Concurrency Model
The Agent runs concurrently in a thread-per-client fashion.
On connecting to a new Controller:
The Agent accepts the TCP connection.
2.	A client context is created.
4.	The newly created thread is detached from the parent thread.
The new thread works on its own with respect to the client.
5.	Main Agent thread remains waiting for new connection(s).
This design enables the Agent to handle several concurrent Controller connections, without blocking the primary listening loop.
The project uses:
pthread_create()
and is produced with:
-pthread
The Agent should be able to support a minimum of five Controller connections for testing purposes.

5. Technologies Used
Technology	Purpose
C	Main implementation language
BSD Sockets API	TCP and UDP communication
TCP/IP	Control channel
UDP	Periodic monitoring
POSIX Threads	Concurrent client handling
Linux /proc	System information
Linux process utilities	Process listing
GCC	Compilation
CentOS/Linux	Development and testing environment
Git/GitHub	Version control and process evidence
6. Project Files
RemoteOps/
│
├── agent_568.c
├── controller_568.c
├── Makefile_568
├── README.md
├── remoteops_568.log
│
└── agentfiles/
    └── IT24102568/
        └── <uploaded files>
Agent
agent_568.c
The Agent acts as the TCP server and provides the remote monitoring and management functionality.
Controller
controller_568.c
The Controller acts as the TCP client used to send commands to the Agent.
Makefile
Makefile_568
The Makefile provides the build configuration for the project.


7. Requirements
The following software will be required:
•	Linux/CentOS environment
•	GCC compiler
•	POSIX/BSD socket support
•	pthread support
Check GCC:
gcc --version
See what is currently in the directory:
pwd

8. Compilation
The project can be compiled using the personalised Makefile.
Check that all files needed are in place:
ls
Expected files include:
agent_568.c
controller_568.c
Makefile_568
README.md
Compile using:
make -f Makefile_568
Alternatively, the Agent can be compiled directly using:
gcc -Wall -Wextra -pthread agent_568.c -o agent_568
The Controller can be compiled using:
The "controller_568.c" program is compiled using the command line above.The above command line is used to compile the "controller_568.c" program.

9. Running the Agent
To start the Agent use:
./agent_568
The Agent listens on the following:
TCP Port: 9410
The Agent should show a startup message, stating that it is now ready to accept connections from Controller.

10. Running the Controller
Launch a new terminal either on the same machine or a machine that can communicate with the Agent.
Run:
./controller_568
The Controller communicates with Agent through TCP.
The Agent address may be:
127.0.0.1
and the port is:
9410

11. Authentication
Any other RemoteOps command must be preceded by the authentication.
The personalised authentication token is:
OPS-2568
The Controller sends:
AUTH OPS-2568
If your response is successful, it will be in the format required as follows:
OK AUTHENTICATED SID:8652
If it is not correct, it should be rejected:
AUTH WRONG-TOKEN
Expected response:
ERR 001 AUTH_FAILED SID:8652
On successful authentication, the Controller can perform the other RemoteOps commands.

12. SYSINFO
The SYSINFO command displays the information of the system.
Command:
SYSINFO
The response contains information related to:
•	CPU load
•	Memory usage
•	System uptime
Example format:
OK SYSINFO ...
SID:8652
Values vary according to the condition of the Linux system.
Implementation gets system information from the Linux environment, such as /proc information if available.

13. LISTPROC
The LISTPROC command displays a list of processes that are running.
Command:
LISTPROC
The process data is collected by the Agent and sent back to the Controller.
Example:
LISTPROC
Responses should start with:
OK PROCS
Process list is a list of processes as a snapshot at the time the command is issued.
Restricted Command Execution. (14)
Arbitrary shell commands are not allowed in RemoteOps.
The following commands are allowed:
DATE
UPTIME
DISKFREE
HOSTNAME
WHOAMI
Examples:
EXEC DATE
EXEC UPTIME
EXEC DISKFREE
EXEC HOSTNAME
EXEC WHOAMI
The Agent pre-checks the requested command with a hard-coded whitelist before running it.
For example:
EXEC ls
LS must be rejected as it is not in the whitelisted list.
Expected error:
ERR 002 COMMAND_NOT_ALLOWED SID:8652
This limitation stops arbitrary shell commands.

14. EXEC — Restricted Command Execution
RemoteOps does not allow arbitrary shell commands.
Only the following commands are permitted:
DATE
UPTIME
DISKFREE
HOSTNAME
WHOAMI
Examples:
EXEC DATE
EXEC UPTIME
EXEC DISKFREE
EXEC HOSTNAME
EXEC WHOAMI
The Agent checks the requested command against the fixed whitelist before execution.
For example:
EXEC ls
must be rejected because ls is not part of the permitted whitelist.
Expected error:
ERR 002 COMMAND_NOT_ALLOWED SID:8652
This restriction prevents arbitrary shell command execution.


15. PUT — File Upload
The PUT command can be used to transfer a file from the Controller to the Agent.
Uploaded files are saved:
./agentfiles/IT24102568/
Example:
PUT test.txt
The Agent is given the file data and it is saved in the personalised storage directory.
Successful transfer response:
OK FILE_RECEIVED SID:8652
The following can be used to check the stored file:
ls -lah ./agentfiles/IT24102568/

16. GET — File Download
The GET command can be used to retrieve a previously uploaded file.
Example:
GET test.txt
If the file is present, the Agent transmits the content of the file to the Controller.
Successful response:
OK FILE_SEND SID:8652
When the file requested does not exist:
ERR 005 FILE_NOT_FOUND SID:8652
The downloaded file can be compared to the original file to ensure that there are no differences in the content.
For example:
md5sum test.txt
md5sum downloaded_test.txt
If the file is the same, then the hashes should be the same.

17. UDP Monitoring
UDP is used as an alternative channel for periodic monitoring for RemoteOps.
The Controller can start monitoring using:
MONITOR START
The Agent responds:
OK MONITOR_STARTED SID:8652
The Agent then periodically sends system-statistics datagrams to the Controller.
The monitoring information is comparable to the SYSINFO information.
Monitoring can be stopped using:
MONITOR STOP
The Agent responds:
OK MONITOR_STOPPED SID:8652
This implementation's monitoring interval is:
INSERT YOUR ACTUAL INTERVAL (e.g. 5 seconds)
The personalised SID is also included in the UDP monitoring datagrams:
SID:8652

18. QUIT
The session can be terminated by the Controller through:
QUIT
The Agent responds:
OK BYE SID:8652
The Agent terminates any monitoring stream that is currently running for a session before closing the connection.
The TCP connection is then closed properly.



19. Error Handling
The Agent deals with improper requests with error responses.
Authentication Failure
AUTH WRONG
Response:
ERR 001 AUTH_FAILED SID:8652
Disallowed Command
EXEC ls
Response:
ERR 002 COMMAND_NOT_ALLOWED SID:8652
File Too Large
When a file is implemented that is greater than the configured file size:
ERR 004 FILE_TOO_LARGE SID:8652
File Not Found
GET missing.txt
Response:
ERR 005 FILE_NOT_FOUND SID:8652
The Agent is intended to catch invalid commands and clients that have disconnected and not kill the whole server process.

20. Logging
In RemoteOps, a personalised log file is maintained:
remoteops_568.log
According to the log, significant activities are noted such as:
•	Client connections
•	Client disconnections
•	Authentication attempts
•	Commands
•	File uploads
•	File downloads
•	Monitoring operations
•	Relevant errors
The log can be checked by:
cat remoteops_568.log
or:
tail -n 20 remoteops_568.log

21. The System's Announce
The Agent is listening on TCP PORT:
9410
Listening socket can be verified with:
ss -tlnp | grep 9410
An active Agent should be in the listening state and have port number 9410.

Make sure to verify the Storage Directory.Verify Storage Directory #23.
The customised storage path is:
./agentfiles/IT24102568/
Check it using:
ls -lah ./agentfiles/IT24102568/
Files uploaded should be found within this directory.

22. File Integrity Verification
In the case of PUT/GET testing, the file downloaded can be compared to the original using:
md5sum test.txt
md5sum downloaded_test.txt
When both MD5 are the same, the files are the same.
For example:
<original hash>  test.txt
<same hash>      downloaded_test.txt
This test is intended to verify that the file has been transferred without errors.

23. Multiple Client Testing
The Agent implements multiple simultaneous connections with Controller using the POSIX threads.
Several Controller instances can be launched in different terminals for testing:
./controller_568
The Agent establishes a TCP session with each Controller.
For every connection accepted by the Agent, a new thread will be opened.
The design thus enables multiple Controllers to communicate with the same Agent simultaneously.

24. Security Considerations
It contains some simple security measures that are expected as part of the assignment.
Authentication
The Agent needs the personalised authentication token for processing protected commands.
OPS-2568
Restricted EXEC
Arbitrary shell commands are not allowed.
Only:
DATE
UPTIME
DISKFREE
HOSTNAME
WHOAMI
are permitted.
Personalised Storage
The uploaded files are kept in:
./agentfiles/IT24102568/
Session Identification
All responses to the TCP include:
SID:8652
This enables associations of responses with the personalised RemoteOps session.


25. TCP and UDP Responsibilities
Explain the responsibilities of TCP and UDP.Explain responsibilities of TCP and UDP.
With RemoteOps, control communication is differentiated from monitoring communication.
TCP
TCP is used for:
•	Authentication
•	SYSINFO
•	LISTPROC
•	EXEC
•	PUT
•	GET
•	MONITOR START
•	MONITOR STOP
•	QUIT
These operations are guaranteed to be reliably and orderly delivered by TCP.
UDP
UDP is used for:
Monitor system periodically via datagrams
UDP is appropriate for periodic monitoring since updates to a monitor do not have to be sent on a persistent reliable stream for each update.

26. Protocol Framing
The RemoteOps TCP protocol is line oriented.
The normal command and response are both followed by:
\n
If a transfer is done, its text command is followed by the raw data of the file to be transferred.
The implementation should therefore take care of the different aspects of:
Text protocol data
and:
Raw file bytes
Using the required number of bytes to transfer a file, not the number of send()/recv() calls.

27. GitHub Repository
The source code and development history is available in Git.
Repository:
https://github.com/kusalsanjula040-pixel/IE3090-RemoteOps-IT24102568
It includes a copy of the source code, Makefile, README and development history for the personalised version.
Use of the Git history as evidence of the assignment process.

28. Development Process
The project was developed gradually.
Key phases of development were:
A basic connection between a TCP Agent and a Controller.A simple TCP Agent/Controller connection.
2.	Multi-client handling
3.	Authentication
4.	SYSINFO
5.	LISTPROC
6.	Restricted EXEC
7.	PUT file upload
8.	GET file download
9.	UDP monitoring
11.	Accessing web services.12.	Navigating the web.
11.	Testing and debugging
Documentation and report writing
These stages were documented using meaningful Git commits during the course of the development.

29. AI Assistance
During Part 1, AI tools were used as specified by the AI Collaboration requirements in the assignment's CLEAR Level 3 requirements.
Using ChatGPT for:
The student should be able to comprehend the C socket programming.
This course covers the explanation of Linux networking commands.
•	Debugging implementation issues
Know how to interpret TCP file transfer.Interpretation of the TCP file transfer.
•	Understanding UDP monitoring
•	Reviewing documentation structure
•	Preparing report/documentation guidance
All AI outputs were reviewed, edited and tested before using.
A separate AI Prompt Log is available to capture the actual AI interactions during development.
In addition, a structured reflection is submitted with an assignment.
30. Design Diary
The development process is recorded in a separate design diary which includes:
•	Initial architecture decisions
•	Concurrency model selection
•	Protocol implementation decisions
•	Problems encountered
•	Debugging activities
•	File-transfer testing
•	UDP monitoring implementation
•	Final testing and improvements
31. Limitations
This is a current implementation for the purpose of the IE3090 RemoteOps assignment.
Future improvements to be considered:
The TCP control channel is encrypted with TLS.
•	Persistent system-monitoring history
•	Transfer throughput measurement
•	File compression
•	More advanced authentication
•	Improved monitoring visualisation
•	More detailed access control
These are not necessary for the minimum required implementation.

32. Submission Information
GitHub Repository:
https://github.com/kusalsanjula040-pixel/IE3090-RemoteOps-IT24102568
Submission Archive:
IE3090_IT24102568.zip
Implementation Report:
Implementation_Report_IT24102568.pdf
AI Prompt Log:
AI_Prompt_Log_IT24102568.pdf
Reflection:
Reflection_IT24102568.pdf
33. Student Declaration
I certify that this is an implement from my assignment and that the implementation I have submitted has been tested and reviewed. All AI tools used in Part 1 have been noted in the AI Prompt Log submitted and discussed critically as per the assignment specification.
Student: Kusal Sanjula
Registration Number: IT24102568
You will be taught to program applications for networks with the module IE3090.
Date: 7 October 2026
~
