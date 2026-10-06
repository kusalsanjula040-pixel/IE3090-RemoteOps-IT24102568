
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


# LISTPROC Command
Purpose

The LISTPROC command allows the Controller to request a list of currently running processes from the RemoteOps Agent.

The Agent executes the Linux ps command and sends the process information back to the Controller over the existing TCP connection.

Command
LISTPROC
Authentication Requirement

The Controller must authenticate before using LISTPROC.

Example:

AUTH OPS-2568

Response:

OK AUTHENTICATED SID:8652

Then:

LISTPROC
Agent Processing

The Agent handles the command using:

handle_listproc(client_fd);

Inside this function, popen() is used to execute the Linux process listing command:

ps -eo pid,user,comm --sort=pid

The command provides:

PID — Process ID
USER — Process owner
COMMAND — Process name

The output is read line by line using fgets().

Each line is sent to the Controller using the TCP send() function.

Example Response
OK PROCS SID:8652
    PID USER     COMMAND
      1 root     systemd
      2 root     kthreadd
      3 root     pool_workqueue_release
...
END PROCS

The exact process list depends on the processes currently running on the CentOS system.

End Marker

The Agent sends:

END PROCS

after the complete process list has been transmitted.

This allows the Controller to identify the end of the LISTPROC response.




##EXEC Command

The RemoteOps Agent supports the EXEC command for executing a predefined set of Linux system commands.

Purpose

The EXEC command allows the Controller to request basic system information from the Linux Agent.

For security reasons, arbitrary Linux commands are not allowed. The Agent uses a whitelist to allow only predefined commands.

Supported Commands

RemoteOps Command	Linux Command	Purpose
EXEC DATE	date	Displays the current system date and time
EXEC UPTIME	uptime	Displays system uptime and load information
EXEC DISKFREE	df -h	Displays disk space usage
EXEC HOSTNAME	hostname	Displays the Agent machine hostname
EXEC WHOAMI	whoami	Displays the current Linux user

EXEC Processing

When the Controller sends an EXEC command:

The Agent receives the command.
The Agent checks whether the requested command is in the whitelist.
If the command is allowed, the corresponding fixed Linux command is executed.
The command output is collected by the Agent.
The result is sent back to the Controller.
If the command is not allowed, the Agent rejects the request.

Command Security

The Agent does not allow arbitrary commands.

For example:

RemoteOps> EXEC ls
Agent: ERR 002 COMMAND_NOT_ALLOWED

The following type of command is also rejected:

RemoteOps> EXEC rm -rf /
Agent: ERR 002 COMMAND_NOT_ALLOWED

This prevents the Controller from directly executing unauthorized Linux commands on the Agent machine.



## PUT and GET – File Transfer

The Controller and Agent can transfer files in either direction via RemoteOps. Commands to transfer files must be successful.

### PUT – Upload File

Uploads a file to the Agent from the Controller.

text
PUT <filename>


Example:

text
RemoteOps> PUT test.txt
Agent: OK READY SID:8652
Agent: OK FILE_RECEIVED SID:8652
PUT completed successfully.


The files uploaded are kept in:

text
./agentfiles/568/


### GET – Download File

Downloads a file from the Agent to the Controller.

text
GET <filename>


Example:

text
RemoteOps> GET test.txt
Agent: OK FILE_READY SID:8652 SIZE:30
Agent: OK FILE_SENT SID:8652
GET completed successfully.


Files downloaded will be saved as:

text
received_<filename>


Example:

text
received_test.txt


### File Transfer Features

Reliable file transfer using TCP.
* Supports text and binary files
Before transferring file data, it also transfers file size.
Implements exact-byte send/receive loops
Supports files up to 64-bit sizes
Path traversal is prevented with filename validation
The name of the resource to be modified or retrieved must be authenticated before PUT/GET.
Successful transfers are indicated by final acknowledgements.

### Testing

Create a test file:

bash
echo "Hello RemoteOps" > test.txt


Authenticate and upload:

text
AUTH OPS-2568
PUT test.txt


Download the file:

text
GET test.txt


Verify the files:

bash
ls -l agentfiles/568/
ls -l received_test.txt
cmp test.txt received_test.txt
```

When `cmp` does not return anything, the file uploaded and downloaded are the same.


 
###UDP Monitoring

Using a separate UDP channel for system monitoring, RemoteOps offers real-time monitoring of a system. Control commands are sent via the TCP connection, and monitoring information is continually sent from the Agent to the Controller via the UDP connection.

### MONITOR START

The `MONITOR START` command starts real-time monitoring on the RemoteOps Agent.

Command:

text
MONITOR START


Example:
RemoteOps> MONITOR START
Agent: OK MONITOR_STARTED SID:8652


Once monitoring has started, the Agent periodically sends out UDP monitoring packets to the Controller.

The monitoring information comprises:

* CPU Load
* Memory Usage
* System Uptime
* Session ID (SID)

Example UDP output:


UDP Monitor [127.0.0.1:40457] -> MONITOR CPU=0.04 MEM=37% UPTIME=00:11:07 SID:8652


Packets are sent periodically at a rate of 0.5 seconds.

 MONITOR STOP

The MONITOR STOP command will terminate the currently running monitoring process.



The Agent terminates monitoring and no more UDP monitoring packets are sent.

 Duplicate Monitoring Request

The Agent returns an error if monitoring is already running and the Controller sends the `MONITOR START` again.


Agent: ERR 011 MONITOR_ALREADY_RUNNING SID:8652


UDP Monitoring Architecture


Controller
    |
    | TCP
    | MONITOR START / STOP
    |
    v
RemoteOps Agent
    |
    | UDP
    | CPU / Memory / Uptime
    |
    v
Controller UDP Receiver


 UDP Port

The UDP monitoring service is based on:


UDP Port: 9410


Since TCP and UDP are separate transport protocols, the same port number is used for both.

 Monitoring Thread

Monitoring is performed in a separate POSIX thread by the Agent. This enables Agent to process TCP commands as data is being transmitted via UDP.

The monitoring thread:

1. Creates a UDP socket.
2. Gets the Controller IP address.
3. Gathers CPU, memory and uptime data.
4. Generates the monitoring message.
Sends the message via UDP.
6. Waits for 2 seconds.
7. Repeats until monitoring is stopped.

 Controller UDP Receiver

The Controller starts a UDP receiver after establishing the TCP connection with the Agent.

The receiver is listening on:


UDP Port: 9410


It is not blocking the normal RemoteOps command prompt, it will receive and display monitoring packets.

Error Handling

These are the responses that are accepted:
OK MONITOR_STARTED
OK MONITOR_STOPPED
ERR 011 MONITOR_ALREADY_RUNNING
ERR 012 MONITOR_FAILED
