/*
CSci4061 F2018 Assignment 2
section: 7
date: 11/10/2018
names: Mukesh Pragaram Choudhary,  Abraham Narvaez,  Sean Miner
ids:   5420773                     5500608           4103814    
*/

# Project2_chat

---
Purpose:
---
The purpose of this program is to provide a multi-chat service within
the same system. The server process, launched first, handles routing of
messages between users, kicking users, and other server functions.
User client processes connect to the server process and can send messages
to all users or a specific user.

---
Contributions:
---
Mukesh: Contributed to Server and User commands processing. Extensive
refactoring and correcting. Contributed to error handling.

Abraham: Wrote original Server and Client implementations, put in extensive work
on user commands (kick in particular), general refactoring.

Sean: Refactored server to handle multiple users, contributed to command routing
and untangling pipes.

---
How to compile code:
---
Run "make clean" and then "make" commands in the command prompt.                  

---
How to use program from shell:
---
Use "./server" in one shell to create the server process.
Use "./client <name_of_the_client>" in another shell to
connect a user to the central server.

Server commands :
    "\list" :                       Prints list of active users
    "\kick <username>" :            Forces named user to exit chat
    "\exit" :                       Terminates user processes and server processes
    "<Any other text>"              Broadcasts message to all active users

User commands :
    "\list" :                       Prints list of active users
    "\exit" :                       Exits from chat
    ""\p2p <username> <message>"" : Sends a message exclusively to the named user
    "<Any other text>"              Broadcasts message to all active users

---
What exactly your program does:
---
Facilitates chat between multiple chat clients on one system. Uses pipes and
other forms of inter-process communication to achieve both that communication
and a degree of isolation between processes.

---
Assumptions:
---
- Currently, the program supports up to 10 users, dictated by the MAX_USER constant.
- The program is designed for use on Linux systems. It MAY work on other systems.
- When a user broadcasts a message, the message does not show up in admin, just as
in the solution executables that were provided. Following that example, p2p messages
also print with the sending user's user_id in the admin shell.
- Messages have a character limit of 256, as well as a limit to tokens (words) that
can be contained in each message. Users are advised to keep messages relatively short.

---
Error Handling strategies:
---
- Checked for errors in every system call.
- Useful messages print when aberrant behavior is detected.
