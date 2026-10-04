
RemoteOps - IE3090 Network Programming



Registration Number: IT24102568



Agent Port: 9410

Session ID: 8652

Authentication Token: OPS-2568



Project:

Remote system monitoring and management tool

using TCP/IP and UDP.


System Architecture
              Controller
             (Client)
                 |
                 |
              TCP/IP
              Port 9410
                 |
                 v
               Agent
              (Server)
                 |
        +--------+--------+
        |                 |
   Authentication    Command Handler
        |
   OPS-2568
        |
     SID 8652


CP Communication Flow

The Agent follows the standard TCP server flow:

socket()
   |
bind()
   |
listen()
   |
accept()
   |
pthread_create()
   |
handle_client()
   |
recv()
   |
process command
   |
send()

The Controller follows:

socket()
   |
connect()
   |
send()
   |
recv()
   |
close()


Multi-Client Support

The Agent uses POSIX threads (pthread) to handle multiple Controllers.

                 Agent
                   |
          accept connection
                   |
       +-----------+-----------+
       |           |           |
       v           v           v
   Thread 1    Thread 2    Thread 3
   Client 1    Client 2    Client 3

Each connected Controller is handled by a separate thread.


#Authentication




Authentication is required before executing RemoteOps commands.

The Controller sends:

AUTH OPS-2568

The Agent checks the authentication token.

Successful Authentication

Controller:

AUTH OPS-2568

Agent:

OK AUTHENTICATED SID:8652

The session is then marked as authenticated.

# IE3090-RemoteOps-IT24102568

