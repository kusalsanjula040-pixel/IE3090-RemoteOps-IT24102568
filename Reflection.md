RemoteOps: Remote System Monitoring and Management Tool 

Structured Reflection 

I have incorporated ChatGPT as an AI support during the implementation of my assignment 
RemoteOps in the following phases: I used it primarily to learn about the concepts of socket 
programming in C, fix implementation issues, explain some of the Linux commands and API's, 
and to better understand my needs for TCP and UDP communication. I also used it to get tips on 
some of the features I've implemented such as authentication, SYSINFO, LISTPROC, EXEC, 
PUT, GET, or UDP monitoring. The AI wasn't a copy-paste of an entire solution, it was a 
learning and debugging tool. 

AI was able to explain the enigmatic concepts in the net programming world in a more 
comprehensible way. It provided me with a small insight into the Agent/Controller concept, the 
meaning of socket(), bind(), listen(), accept(), connect(), send(), recv() and how they can be used 
to support multiple clients with threads. It was also helpful for troubleshooting file transfer issues 
since TCP offers a byte stream and a recv() operation does not always return all the data 
requested. AI was able to teach me about how to handle the number of bytes that I expect to 
move from one place to another when transferring files. 

I also discovered, however, that AI suggestions weren't always applicable. Some of the ideas 
were more abstract and needed to be altered to fit the particular protocol for RemoteOps offered 
in the assignment. The personalised requirements were particularly important. For my 
implementation I had to ensure that my own registration-number-based port, SID, authentication 
token, source filenames, log filename and storage directory were used. I also needed to make 
sure that EXEC functionality would only let the five commands outlined in the assignment. 
I have tested the ideas that AI has thrown my way in my own CentOS environment, and modified 
and rejected them when they were not applicable to the task or when they were different from the 
code that I already have. I made sure that the solution was implemented correctly, as I ran into 
compiler errors and output to the terminal, and also tested using actual client/server testing to be 
sure. 

From this assignment I learned some of the important thing about TCP socket programming in a 
real time application between client and server. Learned about Connection Handling, 
Concurrency w/ Threads, Authentication, Protocol Framing, File Transfer, Process Monitoring 
and UDP Comm. I also found that proper error handling and testing of real communications is 
essential for a successful network programming. 
I believe that the use of AI support with learning and troubleshooting some of the more 
challenging aspects of my assignment, was helpful but testing and modifying the final 
implementation, as well as understanding what it would look like, was my job. This experience 
also assisted me in preparing for the Lab Assessment and Viva, where I have to explain and 
tweak my own implementation without the use of AI.
