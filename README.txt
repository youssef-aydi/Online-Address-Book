This project implements a simple client-server application to manage an address book. Users can manipulate records using the commands ADD, DELETE, LIST, SHUTDOWN, QUIT, UPDATE, LOGIN, LOGOUT, WHO, LOOK and UPDATE.

The project is stable and free of bugs or problems. It includes a data file, address_book.txt, which is initially empty and is used to store all address book entries persistently.

Instructions for Compiling and Running the Program

1.Compiling the Program:

Navigate to the project directory.
Run the following command to compile the project: make

2.Running the Server and Client:

Open three terminals: one for the server and two for the clients.
Server Terminal: ./multiThreadServer
Client 1 Terminal: ./sclient 127.0.0.1
Client 2 Terminal: ./sclient 141.215.69.184 
Note: The server must be started first before any client connection is attempted.

Command Details

LOGIN Command

Use the Login command to authenticate with the server.
Important Rules:
Only registered users can log in. Unregistered or unauthorized users will receive an error message. (root/john/david/mary)
Once logged in, the client can execute commands to manage the address book.

LOGOUT Command

Use the LOGOUT command to disconnect from the current session while keeping the server running.
Important Rules:
You have to be connected first to execute logout command.

ADD Command

Use the ADD command to add a new entry to the address book.
Important Rules:
You should Login first. If a user has not logged in, the server will return a "401 You are not
currently logged in, login first" message
First name and last name should not exceed 8 characters.
Phone number must follow the format xxx-xxx-xxxx.
If any field is missing or invalid, the server will return an error message, and the record will not be added.
Record ID:
Record IDs start from 1000. The first record will have ID 1001 and increment by 1 for each new record.
The maximum number of records is 20. Once the limit is reached, no new entries can be added.

DELETE Command

Use the DELETE command followed by a record ID to delete an existing entry from the address book.
Important Rules:
You should Login first. If a user has not logged in, the server will return a "401 You are not
currently logged in, login first" message
If you don't specify an ID or if the ID doesn't exist, an error message will be returned.

LIST Command
Use the LIST command to display all the current records in the address book.
Important Rules:
If there are no records in the address book, the server will inform you that no records were found.

SHUTDOWN Command

Use the SHUTDOWN command to shut down the server. The server will stop accepting new connections and close all existing connections after shutting down.
Important Rules:
You should Login first. If a user has not logged in, the server will return a "401 You are not
currently logged in, login first" message
Only the root user can execute this command.
The server then terminates all connections and stops running.


QUIT Command

Use the QUIT command to disconnect a client from the server without affecting the server's operation or other clients.
Important Rules:
Quit Command also logout the user.

WHO Command

Use the WHO command to list all active users including the UserID and the users IP adresses.
Important Rules: 
if none of the users is authenticated. Server will return "No active users."

LOOK Command

Use the LOOK command to look up a name in the book and display the full name along with its phone number record.
Important Rules:
if the record doesn't exist that you are trying to look up for it. the server will return and error message "404 Your search did not match any records".

UPDATE Command

Use the UPDATE command to update an existing record in the book and display the updated record. 
Important Rules:
You must login first. If a user has not logged in, the server will return a "401 You are not
currently logged in, login first" message
If you are trying to update a record that doesn't exit the server will return error message "403 The Record ID does not exist".

Important Notes

Field Validation: The ADD command will only execute successfully if all fields (first name, last name, phone number) are valid according to the specified rules. Otherwise, you'll receive an error message.

Command Restrictions: You must use one of the valid commands: ADD, DELETE, LIST, SHUTDOWN, QUIT, UPDATE, LOGIN, LOGOUT, WHO, LOOK or UPDATE. Any invalid command will result in an error message.

Persistent Storage: The project includes a file, address_book.txt, which is initially empty. This file is used to store all address book entries. When the server starts, it loads existing records from the file. Changes made while the server is running (adding or deleting records) are saved back to the file for persistence.

You can only login using one of the registered users

UserID		Password
root		root05
john		john06
David		david07
mary		mary08


Sample output for the LOGIN command
c: LOGIN root root05
s: 200 OK

Sample output for the LOGOUT command
c: LOGOUT
s: 200 OK

Sample output for the WHO command
c: WHO
s: 200 OK
The list of the active users:
mary 141.215.69.184
root 127.0.0.1

Sample output for the LOOK command
c:LOOK 1 youssef
s:200 OK
Found 2 match(es)
1005 youssef aydi 312-323-2378
1008 youssef ben 312-323-3232

Sample output for the UPDATE command
c:UPDATE 1008 2 ID
s:200 OK
Record 1008 updated
1008 youssef ID 312-323-3232

Sample output for the ADD command
c: ADD youssef aydi 313-312-2321
s : Server response 200 OK
The new record ID is 1001 

Sample output for LIST command
c: LIST
s : Server response 200 OK
The list of records in the book: 
1001 youssef aydi 313-312-2321

Sample output for DELETE Command
c: DELETE 1001
s : Server response 200 OK

Sample output for QUIT
c: QUIT
s : Server response 200 OK

Sample output for SHUTDOWN
c: SHUTDOWN
s: 200 OK
210 the server is about to shutdown .....
Connection closed by server.


