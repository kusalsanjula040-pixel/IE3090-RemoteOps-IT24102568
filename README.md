
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

#SYSINFO Command

The RemoteOps Agent supports the SYSINFO command after successful authentication.

Command
SYSINFO
Response Format
OK SYSINFO CPU:<load> MEMORY:<percentage>% UPTIME:<HH:MM:SS> SID:<SID>
Example
RemoteOps> AUTH OPS-2568
Agent: OK AUTHENTICATED SID:8652

RemoteOps> SYSINFO
Agent: OK SYSINFO CPU:0.15 MEMORY:38% UPTIME:05:21:34 SID:8652
System Information

The Agent retrieves:

CPU load
Memory usage percentage
System uptime
Session ID
Linux Sources

The implementation uses the Linux /proc filesystem:

/proc/loadavg
/proc/meminfo
/proc/uptime

/proc/loadavg is used to obtain CPU load information.

/proc/meminfo is used to calculate memory usage.

/proc/uptime is used to calculate system uptime.

Authentication Requirement

SYSINFO cannot be accessed before authentication.
