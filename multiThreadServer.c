/*
 * multiThreadServer.c -- a multithreaded server
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <regex>
#include <chrono>
#include <thread>



// Struct to store each user's credentials
struct User {
    char user_id[20];
    char password[20];
};

// Array of users
struct User users[] = {
    {"root", "root05"},
    {"john", "john06"},
    {"david", "david07"},
    {"mary", "mary08"}
};

fd_set master;   // master file descriptor list
int listener;    // listening socket descriptor
int fdmax;       // track the biggest file descriptor

int logged_in_sockets[FD_SETSIZE] = {0}; // Track whether a socket is logged in
char logged_in_users[FD_SETSIZE][20] = {""}; // Track which user is logged in per socket

using namespace std;
#define ADDRESS_BOOK_FILE "address_book.txt"
#define PORT 3798  // port we're listening on
#define MAX_LINE 256
#define MAX_RECORDS 20

struct AddressBookEntry {
    int record_id;              
    string first_name;
    string last_name;
    string phone_number;

    // Constructor to initialize an AddressBookEntry with record_id
    AddressBookEntry(int record_id, const string& first_name, const string& last_name, const string& phone_number)
        : record_id(record_id), first_name(first_name), last_name(last_name), phone_number(phone_number) {}

    // Default constructor
    AddressBookEntry() : record_id(0), first_name(""), last_name(""), phone_number("") {}
};


vector<AddressBookEntry> address_book;
pthread_mutex_t book_mutex = PTHREAD_MUTEX_INITIALIZER;
// we will initialize the current record id starting from 1000 to match the example provided
int current_record_id = 1000;


// Function to save address book to file
void save_address_book_to_file() {
    ofstream outfile(ADDRESS_BOOK_FILE);
    if (outfile.is_open()) {
        for (const auto &entry : address_book) {
            outfile << entry.record_id << " " << entry.first_name << " " << entry.last_name << " " << entry.phone_number << endl;
        }
        outfile.close();
    } else {
        cout << "Error: Unable to open file for saving." << endl;
    }
}

// Function to load address book from file on server startup
void load_address_book_from_file() {
    ifstream infile(ADDRESS_BOOK_FILE);
    string line;
    address_book.clear(); // Clear any existing data in memory
    current_record_id = 1000; // Reset the record ID starting point

    if (infile.is_open()) {
        while (getline(infile, line)) {
            istringstream iss(line);
            int record_id;
            string first_name, last_name, phone_number;
            if (iss >> record_id >> first_name >> last_name >> phone_number) {
                address_book.push_back(AddressBookEntry(record_id, first_name, last_name, phone_number));
                if (record_id >= current_record_id) {
                    current_record_id = record_id;
                }
            }
        }
        infile.close();
    } else {
        cout << "Error: Unable to open file for loading." << endl;
    }
}
// function that validates the USA phone number format
bool is_valid_phone_number(const string& phone) {
    // Check for the format xxx-xxx-xxxx
    regex phone_format("\\d{3}-\\d{3}-\\d{4}");
    return regex_match(phone, phone_format);
}

// function that validates name lengths up to 8 characters
bool is_valid_name(const string& name) {
    return name.length() >= 1 && name.length() <= 8;
}

// Modify validate_login to return specific codes for 'root' or regular users
int validate_login(const char *user_id, const char *password) {
    int num_users = sizeof(users) / sizeof(users[0]);
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].user_id, user_id) == 0 && strcmp(users[i].password, password) == 0) {
            return (strcmp(user_id, "root") == 0) ? 2 : 1; // 2 for root, 1 for regular
        }
    }
    return 0;  // Login failed
}

// Helper function to check if a user is already logged in
bool is_user_already_logged_in(const char *user_id) {
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (strcmp(logged_in_users[i], user_id) == 0) {
            return true;
        }
    }
    return false;
}

//  handle_login_command that handles single-client login per user credentials
void handle_login_command(const char *command, int client_socket) {
    
    char user_id[20], password[20];
    sscanf(command, "LOGIN %s %s", user_id, password);

    char response[MAX_LINE];
    int login_result = validate_login(user_id, password);

    if (login_result > 0) {
        if (is_user_already_logged_in(user_id)) {
            snprintf(response, sizeof(response), "411 User already logged in elsewhere\n");
        } else {
            snprintf(response, sizeof(response), "200 OK\n");
            logged_in_sockets[client_socket] = login_result; // 1 for user, 2 for root
            strcpy(logged_in_users[client_socket], user_id); // Store user_id for reference
        }
    } else {
        snprintf(response, sizeof(response), "410 Wrong UserID or Password\n");
    }
    send(client_socket, response, strlen(response), 0);
    cout << "LOGIN command received from client " << client_socket << " - " << command << endl;
}

//function that handles the add command
void handle_add_command(string command, int client_socket) {
    // Parse the command
    stringstream ss(command);
    string cmd, first_name, last_name, phone_number;
    ss >> cmd >> first_name >> last_name >> phone_number;
    pthread_mutex_lock(&book_mutex);

    // Check if all required fields are provided (First Name, Last Name, Phone Number)

    if (first_name.empty() || last_name.empty() || phone_number.empty()) {
        string error_message = "301 Message format error: Incomplete command. Please enter first name, last name, and phone number.\n";
        send(client_socket, error_message.c_str(), error_message.length(), 0);
        pthread_mutex_unlock(&book_mutex);
        return;
    }

    // Validate First Name and Last Name (up to 8 characters) 
    if (!is_valid_name(first_name) || !is_valid_name(last_name)) {
        string error_message = "301 Message format error: Name too long\n";
        send(client_socket, error_message.c_str(), error_message.length(), 0);
        pthread_mutex_unlock(&book_mutex);
        return;
    }
    // Validate Phone Number format (xxx-xxx-xxxx)
    if (!is_valid_phone_number(phone_number)) {
        string error_message = "301 Message format error: Invalid phone number format\n";
        send(client_socket, error_message.c_str(), error_message.length(), 0);
        pthread_mutex_unlock(&book_mutex);
        return;
    }

    // Validate address book size doesn't exceed 20 records
    if (address_book.size() >= MAX_RECORDS) {
        string response = "Address book full\n";
        send(client_socket, response.c_str(), response.length(), 0);
        pthread_mutex_unlock(&book_mutex);
        return;
    }

    // Create a new address book entry
    AddressBookEntry new_entry;
    new_entry.record_id = ++current_record_id;
    new_entry.first_name = first_name;
    new_entry.last_name = last_name;
    new_entry.phone_number = phone_number;

    // Add entry to address book
    address_book.push_back(new_entry);

    // Save the updated address book to the file
    save_address_book_to_file();

    // Respond to the client
    string response = "200 OK\nThe new Record ID is " + to_string(new_entry.record_id) + "\n";
    send(client_socket, response.c_str(), response.length(), 0);
    printf("Received ADD command from client %d: %s\n", client_socket, command.c_str());
    pthread_mutex_unlock(&book_mutex);

}

// function that handles delete command
void handle_delete_command(string command, int client_socket) {
    stringstream ss(command);
    string cmd;
    int record_id;
    ss >> cmd >> record_id;
    pthread_mutex_lock(&book_mutex);

    
    // Find the record with the given record ID
    auto it = find_if(address_book.begin(), address_book.end(),
                      [record_id](const AddressBookEntry& entry) {
                          return entry.record_id == record_id;
                      });
    string response;
    if (it != address_book.end()) {
              cout << "Deleting record with ID: " << record_id << endl;

        // Record found, delete it
        address_book.erase(it);

         // Save the updated address book to the file
        save_address_book_to_file();

        response = "200 OK\n";
    } else {
              cout << "Record with ID " << record_id << " not found." << endl;

        // Record not found
        response = "403 The Record ID does not exist\n";
    }
    send(client_socket, response.c_str(), response.length(), 0);
    cout << "DELETE command received for Record ID: " << record_id << "from client "<< client_socket << endl;
    pthread_mutex_unlock(&book_mutex);

}

// function that handles list command
void handle_list_command(int client_socket) {
    cout << "LIST command received from client " << client_socket << endl;
    string response = "200 OK\nThe list of records in the book:\n";
    pthread_mutex_lock(&book_mutex);
    for (const auto& entry : address_book) {
        response += to_string(entry.record_id) + "\t" + entry.first_name + "\t" + entry.last_name + "\t" + entry.phone_number + "\n";
    }

    if (address_book.empty()) {
        response += "No records found.\n";
    }

    send(client_socket, response.c_str(), response.length(), 0);
    pthread_mutex_unlock(&book_mutex);

}


//function that handles shutdown command
void handle_shutdown_command(int client_socket) {
    cout << "SHUTDOWN command received from client " << client_socket << endl;
    if (strcmp(logged_in_users[client_socket], "root") == 0) {
        // Notify the client that the shutdown is successful
        const char *response = "200 OK\n";
        send(client_socket, response, strlen(response), 0);

        // Pause briefly to ensure root client receives confirmation before the broadcast
        this_thread::sleep_for(chrono::milliseconds(100));

         // Notify all clients about the server shutdown
        const char *shutdown_notice = "210 the server is about to shutdown .....\n";
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &master) && i != listener) {
                send(i, shutdown_notice, strlen(shutdown_notice), 0);
            }
        }

        // Close all client connections after a short delay
        this_thread::sleep_for(chrono::seconds(2)); // Allow clients time to read message
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &master) && i != listener) {
                close(i);  // Close client socket
            }
        }

        // Exit the server
        close(listener); // Close the listening socket
        exit(0); // Terminate the server
    } else {
        // User is not root; send error message
        const char *response = "402 User not allowed to execute this command\n";
        send(client_socket, response, strlen(response), 0);
    }
}

// Function to handle logout command
void handle_logout_command(int client_socket) {
    cout << "LOGOUT command received from client " << client_socket << endl;
    if (logged_in_sockets[client_socket] > 0) {
        // Clear the login state for the client
        logged_in_sockets[client_socket] = 0;           // Reset login status
        logged_in_users[client_socket][0] = '\0';       // Clear logged-in user ID

        // Send confirmation message to client
        const char *response = "200 OK\n";
        send(client_socket, response, strlen(response), 0);
    } else {
        // If client is not logged in, notify that logout isn't applicable
        const char *response = "403 You are not logged in.\n";
        send(client_socket, response, strlen(response), 0);
    }
}

//function that handles quit command
void handle_quit_command(int client_socket) {
    cout << "QUIT command received from client " << client_socket << endl;
    // Check if the client is logged in
    if (logged_in_sockets[client_socket] > 0) {
        // Perform a logout if the client is currently logged in
        handle_logout_command(client_socket);
    }
    string response = "200 OK\n";
    send(client_socket, response.c_str(), response.length(), 0);
    close(client_socket); // Close the client socket
}

void handle_who_command(int client_socket) {
    cout << "WHO command received from client " << client_socket << endl;
    string response = "200 OK\nThe list of active users:\n";
    bool any_user_found = false;

    // Iterate over all possible socket descriptors
    for (int i = 0; i <= fdmax; i++) {
        if (logged_in_sockets[i] > 0) { // Check if the socket is logged in
            any_user_found = true;
            
            // Retrieve the user ID for this socket
            string user_id = logged_in_users[i];
            
            // Retrieve the IP address associated with this socket
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            getpeername(i, (struct sockaddr*)&addr, &addr_len);
            string ip_address = inet_ntoa(addr.sin_addr);

            // Append user information to the response
            response += user_id + " " + ip_address + "\n";
        }
    }

    // If no users are found, let the client know
    if (!any_user_found) {
        response += "No active users.\n";
    }

    // Send the response to the client
    send(client_socket, response.c_str(), response.length(), 0);
}

// Function to handle the LOOK command
void handle_look_command(int client_socket, const string& command) {
    cout << "LOOK command received: " << command << " - from client " << client_socket << endl;
    // Parse the command to determine search type and query term
    stringstream ss(command);
    string cmd;
    int search_type;
    string search_term;
    ss >> cmd >> search_type >> search_term;

    pthread_mutex_lock(&book_mutex); // Ensure thread-safe access to address book
    vector<AddressBookEntry> results;

    // Search based on the search_type provided
    for (const auto& entry : address_book) {

        if (search_type < 1 || search_type > 3) {
        string response = "301 Message format error: Search type must be 1 (first name), 2 (last name), or 3 (phone number)\n";
        send(client_socket, response.c_str(), response.length(), 0);
        pthread_mutex_unlock(&book_mutex);
        return;
    }
        if ((search_type == 1 && entry.first_name == search_term) ||
            (search_type == 2 && entry.last_name == search_term) ||
            (search_type == 3 && entry.phone_number == search_term)) {
            results.push_back(entry);
        }
    }

    string response;
    if (!results.empty()) {
        response = "200 OK\nFound " + to_string(results.size()) + " match(es)\n";
        for (const auto& entry : results) {
            response += to_string(entry.record_id) + " " + entry.first_name + " " + entry.last_name + " " + entry.phone_number + "\n";
        }
    } else {
        response = "404 Your search did not match any records\n";
    }

    send(client_socket, response.c_str(), response.length(), 0);
    pthread_mutex_unlock(&book_mutex); // Release lock after search
}

void handle_update_command(string command, int client_socket) {
    cout << "UPDATE command received: " << command << " - from client " << client_socket << endl;
    // Check if the client is logged in
    if (logged_in_sockets[client_socket] == 0) {
        // Send response that login is required
        string response = "403 You must be logged in to use this command\n";
        send(client_socket, response.c_str(), response.length(), 0);
        return;
    }
    stringstream ss(command);
    string cmd;
    int record_id, field_to_update;
    string new_value;

    // Parse the command
    ss >> cmd >> record_id >> field_to_update;
    // The new value is everything after the field_to_update
    getline(ss, new_value);
    new_value = new_value.substr(1); // Remove the leading space

    pthread_mutex_lock(&book_mutex);

    // Find the record with the given record ID
    auto it = find_if(address_book.begin(), address_book.end(),
                      [record_id](const AddressBookEntry& entry) {
                          return entry.record_id == record_id;
                      });

    string response;
    if (it != address_book.end()) {
        // Record found, update the corresponding field
        switch (field_to_update) {
            case 1: // Update first name
                if (is_valid_name(new_value)) {
                    it->first_name = new_value;
                } else {
                    response = "301 Message format error: Name must be 1-8 characters\n";
                    send(client_socket, response.c_str(), response.length(), 0);
                    pthread_mutex_unlock(&book_mutex);
                    return;
                }
                break;

            case 2: // Update last name
                if (is_valid_name(new_value)) {
                    it->last_name = new_value;
                } else {
                    response = "301 Message format error: Name must be 1-8 characters\n";
                    send(client_socket, response.c_str(), response.length(), 0);
                    pthread_mutex_unlock(&book_mutex);
                    return;
                }
                break;

            case 3: // Update phone number
                if (is_valid_phone_number(new_value)) {
                    it->phone_number = new_value;
                } else {
                    response = "301 Message format error: Invalid phone number format\n";
                    send(client_socket, response.c_str(), response.length(), 0);
                    pthread_mutex_unlock(&book_mutex);
                    return;
                }
                break;

            default:
                response = "301 Message format error: Invalid update type\n";
                send(client_socket, response.c_str(), response.length(), 0);
                pthread_mutex_unlock(&book_mutex);
                return;
        }

        // Save the updated address book to the file
        save_address_book_to_file();

        // Respond to the client
        response = "200 OK\n";
        response += "Record " + to_string(record_id) + " updated\n";
        response += to_string(it->record_id) + " " + it->first_name + " " + it->last_name + " " + it->phone_number + "\n";
        send(client_socket, response.c_str(), response.length(), 0);

    } else {
        // Record not found
        response = "403 The Record ID does not exist.\n";
        send(client_socket, response.c_str(), response.length(), 0);
    }

    pthread_mutex_unlock(&book_mutex);
}


// the child thread
void *ChildThread(void *newfd) {
    char buf[MAX_LINE];
    int nbytes;
    int i, j;
    int childSocket = (long) newfd;

    while(1) {
        // handle data from a client
        if ((nbytes = recv(childSocket, buf, sizeof(buf), 0)) <= 0) {
            // got error or connection closed by client
            if (nbytes == 0) {
                // connection closed
                cout << "multiThreadServer: socket " << childSocket <<" hung up" << endl;
            } else {
                perror("recv");
            }
            // Clear the login state of the disconnected client
            logged_in_sockets[childSocket] = 0;           // Reset login status
            logged_in_users[childSocket][0] = '\0';       // Clear logged-in user ID
            close(childSocket); // bye!
            FD_CLR(childSocket, &master); // remove from master set
            pthread_exit(0);
        } else {

             // Convert buffer to string and parse command
            buf[nbytes] = '\0';
            char *command = buf;

            if (strncmp(command, "LOGIN", 5) == 0) {
                // Check if already logged in
                if (logged_in_sockets[childSocket] > 0) {
                    char response[] = "403 You are already logged in. Please quit before logging in with another account.\n";
                    send(childSocket, response, strlen(response), 0);
                } else {
                    handle_login_command(command, childSocket);
                }
            } else if (strncmp(command, "LOGOUT", 6) == 0) {
                handle_logout_command(childSocket);
            } else if (strncmp(command, "ADD", 3) == 0) {
                if (logged_in_sockets[childSocket] > 0) {
                    handle_add_command(command, childSocket);
                } else {
                    char response[] = "401 You are not currently logged in, login first\n";
                    send(childSocket, response, strlen(response), 0);
                }
            } else if (strncmp(command, "DELETE", 6) == 0) {
                if (logged_in_sockets[childSocket] > 0) {
                    handle_delete_command(command, childSocket);
                } else {
                    char response[] = "401 You are not currently logged in, login first\n";
                    send(childSocket, response, strlen(response), 0);
                }
            } else if (strncmp(command, "UPDATE", 6) == 0) {
                if (logged_in_sockets[childSocket] > 0) {
                    handle_update_command(command, childSocket);
                } else {
                    char response[] = "401 You are not currently logged in, login first\n";
                    send(childSocket, response, strlen(response), 0);
                }
            }  else if (strncmp(command, "LIST", 4) == 0) {
                handle_list_command(childSocket);
            } else if (strncmp(command, "WHO", 3) == 0) {
                handle_who_command(childSocket);
            } else if (strncmp(command, "LOOK", 4) == 0) {
                handle_look_command(childSocket, command);
            } else if (strncmp(command, "QUIT", 4) == 0) {
                handle_quit_command(childSocket);
                break;
            } else if (strncmp(command, "SHUTDOWN", 8) == 0) {
                // Check if the user is logged in 
                if (logged_in_sockets[childSocket] > 0) {  // Ensure user is logged in
                    handle_shutdown_command(childSocket);
                } else {
                    char response[] = "401 You are not currently logged in, login first\n";
                    send(childSocket, response, strlen(response), 0);
                }
            } else {
                char response[] = "300 invalid command\n";
                send(childSocket, response, strlen(response), 0);
            }
        }
    }
    return nullptr;
}


int main(void)
{
    load_address_book_from_file();

    struct sockaddr_in myaddr;     // server address
    struct sockaddr_in remoteaddr; // client address
    int newfd;        // newly accept()ed socket descriptor
    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;

    pthread_t cThread;

    FD_ZERO(&master);    // clear the master and temp sets

    // get the listener
    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // lose the pesky "address already in use" error message
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    // bind
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(PORT);
    memset(&(myaddr.sin_zero), '\0', 8);
    if (bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(1);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    addrlen = sizeof(remoteaddr);

    // main loop
    for(;;) {
        // handle new connections
        if ((newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen)) == -1) {
            perror("accept");
	        exit(1);
        } else {
            FD_SET(newfd, &master); // add to master set
            cout << "multiThreadServer: new connection from "
		 		 << inet_ntoa(remoteaddr.sin_addr)
                 << " socket " << newfd << endl;

            if (newfd > fdmax) {    // keep track of the maximum
                fdmax = newfd;
            }

	    if (pthread_create(&cThread, NULL, ChildThread, (void *)(intptr_t)newfd) <0) {
                perror("pthread_create");
                exit(1);
            }
        }

    }
    return 0;
}

