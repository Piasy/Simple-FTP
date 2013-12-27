#include<iostream>
#include<iomanip>
#include<stdio.h>
#include<sys/fcntl.h>
#include<netinet/in.h>
#include<netdb.h>

#include<errno.h>
#include<cstdlib>
#include<cerrno>

#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include<string>
#include<fstream>

#define DEFAULT_SERVER_PORT 9221
#define RCV_BUFFER_SIZE 10240
#define SEND_BUFFER_SIZE 10240
#define PATH_BUFFER_SIZE 10240
#define BLUE "\033[0;34m"
#define YELLOW "\033[1;33m"
#define GREEN "\033[0;32m"
#define L_BLUE "\033[1;34m"

using namespace std;

int init(int argc, char * argv[]);

char * receive_reply(int socket_fd);
void print_cwd(char * cwd, bool root);
void print_reply(char * reply);

void * send_file(void * arg);
void * receive_file(void * arg);
void * receive_file_post(void * arg);
void * send_file_post(void * arg);

struct file_transmit_arg
{
	char * file_name;
	struct sockaddr_in server_addr;
};

struct file_transmit_post_arg
{
	char * file_name;
	int socket_fd;
};

struct sockaddr_in server_address;
bool finish = false;
char cwd[PATH_BUFFER_SIZE];

int main(int argc, char * argv[])
{
	cout << GREEN;
	int control_socket_fd = init(argc, argv);
	
	cout << control_socket_fd << endl;
	if (control_socket_fd < 0)
	{
		//close(control_socket_fd);
		return 1;
	}
	
	cout << "Connect to server success!\n";
	
	
	
	//usleep(1000 * 1000);
	char * reply = receive_reply(control_socket_fd);
	if (reply == NULL)
	{
		close(control_socket_fd);
		return 1;
	}
	print_reply(reply);
	
	bool quit = false;
	char command[SEND_BUFFER_SIZE];
	while (true)
	{
		memset(command, 0, sizeof(command));
		string input;
		getline(cin, input);
		bool has_space = false;
		int cmd_len = 0;
		for (int i = 0; i < input.size(); i ++)
		{
			if (cmd_len == SEND_BUFFER_SIZE && i < input.size())
			{
				cout << "Command is too long!\n";
				return 1;
			}
			if (input[i] == ' ' || input[i] == '\t')
			{
				if (!has_space)
				{
					command[cmd_len] = ' ';
					cmd_len ++;
					has_space = true;
				}
			}
			else
			{
				command[cmd_len] = input[i];
				cmd_len ++;
				has_space = false;
			}
		}
		
		char * cmd = strtok(command, " ");
		if (cmd == NULL)
		{
			//cout << "Bad input!\n";
			print_cwd(cwd, false);
			continue;
		}
		
		if (strcmp(cmd, "quit") == 0)
			break;
		
		//usleep(1000 * 1000);
		if (strcmp(cmd, "dir") == 0)
		{
			write(control_socket_fd, input.c_str(), input.size());
			reply = receive_reply(control_socket_fd);
			if (reply == NULL)
			{
				close(control_socket_fd);
				return 1;
			}
			print_reply(reply);
		}
		else if (strcmp(cmd, "cd") == 0)
		{
			write(control_socket_fd, input.c_str(), input.size());
			reply = receive_reply(control_socket_fd);
			if (reply == NULL)
			{
				close(control_socket_fd);
				return 1;
			}
			
			print_reply(reply);
		}
		else if (strcmp(cmd, "get") == 0)
		{
			char * type = strtok(NULL, " ");
			//cout << "type = " << type << endl;
			if (strcmp(type, "pasv") == 0)
			{
				char * src = strtok(NULL, " ");
				char * dest = strtok(NULL, " ");
				if (src == NULL || dest == NULL)
				{
					cout << "Bad command!\n";
					print_cwd(cwd, false);
					continue;
				}
				else
				{
					int dest_file = open(dest, O_RDONLY, S_IREAD);
					if (0 <= dest_file)
					{
						cout << "Error! There is one file or directory with the same name.\n";
						print_cwd(cwd, false);
						continue;
					}
					else
					{
						memset(cwd, 0, PATH_BUFFER_SIZE);
						write(control_socket_fd, input.c_str(), input.size());
						
						reply = receive_reply(control_socket_fd);
						if (reply == NULL)
						{
							close(control_socket_fd);
							return 1;
						}
						else
						{
							int reply_len = strlen(reply);
						
							//get cwd
							for (int i = 0; i < reply_len; i ++)
							{
								if (reply[i] == '\n')
									break;
								else
									cwd[i] = reply[i];
							}
							//get message
							int cwd_len = strlen(cwd);
							for (int i = cwd_len + 1; i < reply_len; i ++)
								reply[i - cwd_len - 1] = reply[i];
							reply[reply_len - cwd_len - 1] = '\0';
						
							if (strstr(reply, "Begin") != NULL)
							{
								struct file_transmit_arg arg;
								arg.file_name = new char[strlen(dest) + 3];
								strcpy(arg.file_name, dest);
								//cout << "dest " << dest << endl;
								memcpy(&arg.server_addr, &server_address, sizeof(server_address));
								arg.server_addr.sin_port = htons(9222);
							
								pthread_t ntid;
								if (pthread_create(&ntid, NULL, receive_file, (void *) &arg) != 0)
								{
									cout << "Get file failed! Create thread error!\n";
								}
							}
						}

						if (strlen(reply) != 0)
							cout << reply << endl;
						print_cwd(cwd, false);
					}
				}
			}
			else if (strcmp(type, "post") == 0)
			{
				char * src = strtok(NULL, " ");
				char * dest = strtok(NULL, " ");
				if (src == NULL || dest == NULL)
				{
					cout << "Bad command!\n";
					print_cwd(cwd, false);
					continue;
				}
				else
				{
					int dest_file = open(dest, O_RDONLY, S_IREAD);
					if (0 <= dest_file)
					{
						cout << "Error! There is one file or directory with the same name.\n";
						print_cwd(cwd, false);
						continue;
					}
					else
					{
						memset(cwd, 0, PATH_BUFFER_SIZE);
						write(control_socket_fd, input.c_str(), input.size());
						
						pthread_t ntid;
						file_transmit_post_arg arg;
						arg.file_name = dest;
						arg.socket_fd = control_socket_fd;
						if (pthread_create(&ntid, NULL, receive_file_post, (void *) &arg) != 0)
						{
							cout << "Get file failed! Create thread error!\n";
						}
												
						reply = receive_reply(control_socket_fd);
						if (reply == NULL)
						{
							close(control_socket_fd);
							return 1;
						}
						else
						{
							int reply_len = strlen(reply);
						
							//get cwd
							for (int i = 0; i < reply_len; i ++)
							{
								if (reply[i] == '\n')
									break;
								else
									cwd[i] = reply[i];
							}
							//get message
							int cwd_len = strlen(cwd);
							for (int i = cwd_len + 1; i < reply_len; i ++)
								reply[i - cwd_len - 1] = reply[i];
							reply[reply_len - cwd_len - 1] = '\0';
						
							cout << "ok1\n";
						}

						if (strlen(reply) != 0)
							cout << reply << endl;
						print_cwd(cwd, false);
					}
				}
			}
			else
			{
				//cout << "in 3\n";
				cout << "Bad command! You should specify the transport mode, post or pasv.\n";
				print_cwd(cwd, false);
				continue;
			}
		}
		else if (strcmp(cmd, "put") == 0)
		{
			char * type = strtok(NULL, " ");
			if (strcmp(type, "pasv") == 0)
			{
				char * src = strtok(NULL, " ");
				char * dest = strtok(NULL, " ");
				if (src == NULL || dest == NULL)
				{
					cout << "Bad command!\n";
					print_cwd(cwd, false);
					continue;
				}
				else
				{
					memset(cwd, 0, PATH_BUFFER_SIZE);
					write(control_socket_fd, input.c_str(), input.size());
						
					reply = receive_reply(control_socket_fd);
					if (reply == NULL)
					{
						close(control_socket_fd);
						return 1;
					}
					else
					{
						int reply_len = strlen(reply);
						
						//get cwd
						for (int i = 0; i < reply_len; i ++)
						{
							if (reply[i] == '\n')
								break;
							else
								cwd[i] = reply[i];
						}
						//get message
						int cwd_len = strlen(cwd);
						for (int i = cwd_len + 1; i < reply_len; i ++)
							reply[i - cwd_len - 1] = reply[i];
						reply[reply_len - cwd_len - 1] = '\0';
						
						if (strstr(reply, "Begin") != NULL)
						{
							struct file_transmit_arg arg;
							arg.file_name = new char[strlen(src) + 3];
							strcpy(arg.file_name, src);
							//cout << "dest " << dest << endl;
							memcpy(&arg.server_addr, &server_address, sizeof(server_address));
							arg.server_addr.sin_port = htons(9223);
							
							usleep(50 * 1000);
							pthread_t ntid;
							if (pthread_create(&ntid, NULL, send_file, (void *) &arg) != 0)
							{
								cout << "Get file failed! Create thread error!\n";
								return 1;
							}
						}
					}
					if (strlen(reply) != 0)
						cout << reply << endl;
					print_cwd(cwd, false);
				}
			}
			else if (strcmp(type, "post") == 0)
			{
				char * src = strtok(NULL, " ");
				char * dest = strtok(NULL, " ");
				if (src == NULL || dest == NULL)
				{
					cout << "Bad command!\n";
					print_cwd(cwd, false);
					continue;
				}
				else
				{
					memset(cwd, 0, PATH_BUFFER_SIZE);
					write(control_socket_fd, input.c_str(), input.size());
						
						
					pthread_t ntid;
					file_transmit_post_arg arg;
					arg.file_name = src;
					arg.socket_fd = control_socket_fd;
					if (pthread_create(&ntid, NULL, send_file_post, (void *) &arg) != 0)
					{
						cout << "Get file failed! Create thread error!\n";
						return 1;
					}
						
					reply = receive_reply(control_socket_fd);
					if (reply == NULL)
					{
						close(control_socket_fd);
						return 1;
					}
					else
					{
						int reply_len = strlen(reply);
						
						//get cwd
						for (int i = 0; i < reply_len; i ++)
						{
							if (reply[i] == '\n')
								break;
							else
								cwd[i] = reply[i];
						}
						//get message
						int cwd_len = strlen(cwd);
						for (int i = cwd_len + 1; i < reply_len; i ++)
							reply[i - cwd_len - 1] = reply[i];
						reply[reply_len - cwd_len - 1] = '\0';
						
					}
					if (strlen(reply) != 0)
						cout << reply << endl;
					print_cwd(cwd, false);
				}
			}
			else
			{
				cout << "Bad command! You should specify the transport mode, post or pasv.\n";
				print_cwd(cwd, false);
				continue;
			}

		}
		else
		{
			memset(cwd, 0, PATH_BUFFER_SIZE);
			write(control_socket_fd, input.c_str(), input.size());
			reply = receive_reply(control_socket_fd);
			if (reply == NULL)
			{
				close(control_socket_fd);
				return 1;
			}
			else
			{
				int reply_len = strlen(reply);
				//get cwd
				for (int i = 0; i < reply_len; i ++)
				{
					if (reply[i] == '\n')
						break;
					else
						cwd[i] = reply[i];
				}
				//get message
				int cwd_len = strlen(cwd);
				for (int i = cwd_len + 1; i < reply_len; i ++)
					reply[i - cwd_len - 1] = reply[i];
				reply[reply_len - cwd_len - 1] = '\0';
			}
			
			cout << reply << endl;
			print_cwd(cwd, false);
		}
	}
	
	close(control_socket_fd);
	return 0;
}

int init(int argc, char * argv[])
{
	if (argc != 3)
	{
		cout << "Usage: Client <server address> <server port>\n";
		return -1;
	}
	
	//cout << "ip " << argv[1] << ", port " << argv[2] << endl;
	
	int control_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (control_socket_fd < 0)
	{
		cout << "Create socket failed!\n";
		return -1;
	}
	
	/*int control_socket_opt = 1;
	if (setsockopt(control_socket_fd, IPPROTO_TCP, TCP_NODELAY, &control_socket_opt, sizeof(control_socket_opt)) == -1)
	{
		cout << "Set socket opt failed!\n";
		return -1;
	}*/
	
	struct timeval max_wait_time;
	max_wait_time.tv_sec = 0;
	max_wait_time.tv_usec = 30 * 1000;
	if (setsockopt(control_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &max_wait_time, sizeof(max_wait_time)) != 0)
	{
		cout << "Set socket opt failed!\n";
		return -1;
	}
	
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	
	char server_ip[32];
	int input_ip_len = (strlen(argv[1]) < 32) ? strlen(argv[1]) : 31;
	strncpy(server_ip, argv[1], input_ip_len);
	server_ip[input_ip_len] = 0;
	//cout << "server ip " << server_ip << endl;
	if (inet_addr(server_ip) != INADDR_NONE)
		server_addr.sin_addr.s_addr = inet_addr(server_ip);
	else
	{
		struct hostent * server_host_name = gethostbyname(server_ip);
		if (server_host_name != 0)
			memcpy(&server_addr.sin_addr, &server_host_name->h_addr, sizeof(server_addr.sin_addr));
		else
		{
			cout << "Illegal server address!\n";
			return -1;
		}
	}
	
	int server_port = DEFAULT_SERVER_PORT;
	if (atol(argv[2]) != 0)
		server_port = atol(argv[2]);
	server_addr.sin_port = htons(server_port);
	
	memcpy(&server_address, &server_addr, sizeof(server_address));
	
	if (connect(control_socket_fd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) < 0)
	{
		cout << "Connect to server failed!\n";
		return -1;
	}
	return control_socket_fd;
}

char rcv_buffer[RCV_BUFFER_SIZE];
//assume all data could be receive at one time
char * receive_reply(int socket_fd)
{
	memset(rcv_buffer, 0, sizeof(rcv_buffer));
	int byte_rcv = read(socket_fd, rcv_buffer, sizeof(rcv_buffer));
	int errsv = errno;
	while (byte_rcv <= 0 && errsv == EAGAIN && strlen(rcv_buffer) == 0)
	{
		byte_rcv = read(socket_fd, rcv_buffer, sizeof(rcv_buffer));
		errsv = errno;
	}
	if (strlen(rcv_buffer) <= 0)
	{
		cout << "Receive message failed!\n";
		cout << "errno : " << errsv << endl;
		return NULL;
	}
	return rcv_buffer;
}

void print_cwd(char * cwd, bool root)
{
	cout << BLUE << cwd << GREEN;
	if (root)
		cout << "$ ";
	else
		cout << "# ";
}

void * send_file(void * arg)
{
	cout << endl;
	struct file_transmit_arg * myArg = (struct file_transmit_arg *) arg;
	
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0)
	{
		cout << "Create socket failed!\n";
		return NULL;
	}
	
	struct timeval max_wait_time;
	max_wait_time.tv_sec = 0;
	max_wait_time.tv_usec = 30 * 1000;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &max_wait_time, sizeof(max_wait_time)) != 0)
	{
		cout << "Set socket opt failed!\n";
		return NULL;
	}
	if (connect(socket_fd, (struct sockaddr *) &myArg->server_addr, sizeof(struct sockaddr)) < 0)
	{
		cout << "Connect to server failed!\n";
		return NULL;
	}
	
	cout << "Connect to server at 9223 success\n";
	
	int src_file = open(myArg->file_name, O_RDONLY, S_IREAD);
	if (0 <= src_file)
	{
		//cout << "open file ok\n";
		char buffer[RCV_BUFFER_SIZE];
		memset(buffer, 0, RCV_BUFFER_SIZE);
		int bytes = read(src_file, buffer, RCV_BUFFER_SIZE);
		while (0 < bytes)
		{
			write(socket_fd, buffer, bytes);
			cout << "Send " << bytes << " bytes\n";
			memset(buffer, 0, RCV_BUFFER_SIZE);
			bytes = read(src_file, buffer, RCV_BUFFER_SIZE);
		}
		cout << "Send finish\n";
		close(src_file);
	}
	else
		cout << "Open file fail\n";
	close(socket_fd);
	cout << "Send " << myArg->file_name << " success!\n";
	//print_cwd(cwd, false);
	finish = true;
	return NULL;
}

void * receive_file(void * arg)
{
	cout << endl;
	struct file_transmit_arg * myArg = (struct file_transmit_arg *) arg;
	
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0)
	{
		cout << "Create socket failed!\n";
		return NULL;
	}
	
	struct timeval max_wait_time;
	max_wait_time.tv_sec = 0;
	max_wait_time.tv_usec = 30 * 1000;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &max_wait_time, sizeof(max_wait_time)) != 0)
	{
		cout << "Set socket opt failed!\n";
		return NULL;
	}
	if (connect(socket_fd, (struct sockaddr *) &myArg->server_addr, sizeof(struct sockaddr)) < 0)
	{
		cout << "Connect to server failed!\n";
		return NULL;
	}
	
	cout << "Connect to server at 9222 success\n";
	
	//int file_fd = open(myArg->file_name, O_CREAT, 0644);
	ofstream fout;
	cout << "file name " << myArg->file_name << endl;
	fout.open(myArg->file_name, ios_base::out|ios_base::binary);
	int errsv = errno;
	//if (file_fd < 0)
	if (!fout)
	{
		cout << "Create file failed\n";
		cout << errsv << endl;
		return NULL;
	}
	char buffer[RCV_BUFFER_SIZE];
	memset(buffer, 0, RCV_BUFFER_SIZE);
	int bytes = read(socket_fd, buffer, RCV_BUFFER_SIZE);
	cout << "receive " << bytes << " bytes\n";
	errsv = errno;
	while (0 < bytes)
	{
		//write(file_fd, buffer, bytes);
		fout << buffer;
		memset(buffer, 0, RCV_BUFFER_SIZE);
		bytes = read(socket_fd, buffer, RCV_BUFFER_SIZE);
		cout << "receive " << bytes << " bytes\n";
	}
	close(socket_fd);
	//close(file_fd);
	fout.close();
	cout << "Receive " << myArg->file_name << " success!\n";
	//print_cwd(cwd, false);
	//cout << "";
	finish = true;
	return NULL;
}

void * receive_file_post(void * arg)
{
	cout << endl;
	cout << "ok3\n";
	struct sockaddr_in data_sock_addr;
	memset(&data_sock_addr, 0, sizeof(data_sock_addr));
	data_sock_addr.sin_family = AF_INET;					//protocol family
	data_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);	//set control socket address as local address
	data_sock_addr.sin_port = htons(9223);				//set control socket port
	socklen_t length = sizeof(data_sock_addr);
	
	cout << "ok4\n";
	
	int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	int control_socket_opt = 1;
	setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &control_socket_opt, sizeof(control_socket_opt));	
	if (listen_socket < 0)
	{
		cout << "Create socket failed!\n";
		return NULL;
	}
	cout << "ok5\n";
	
	if (bind(listen_socket, (struct sockaddr *) &data_sock_addr, length) < 0)
	{
		cout << "Bind socket failed!\n";
		return NULL;
	}
	
	cout << "ok6\n";
	
	if (getsockname(listen_socket, (struct sockaddr *) &data_sock_addr, &length) < 0)
	{
		cout << "Get socket name failed!\n";
		return NULL;
	}
	
	cout << "ok7\n";
	
	file_transmit_post_arg myArg = *((file_transmit_post_arg *) arg);
	int control_socket_fd = myArg.socket_fd;
	char * dest = new char [strlen(myArg.file_name)];
	strcpy(dest, myArg.file_name);
	
	cout << "ok8\n";
	cout << dest << " " << control_socket_fd << endl;
	
	
	cout << "addr : " << data_sock_addr.sin_addr.s_addr << endl;
	
	write(control_socket_fd, &data_sock_addr, sizeof(data_sock_addr));
	
	cout << "Create socket at 9223 success!\n";
	
	listen(listen_socket, 1);
	
	int file_transmit_socket = accept(listen_socket, NULL, NULL);
	//try two more times
	if (file_transmit_socket < 0)
		file_transmit_socket = accept(listen_socket, NULL, NULL);
	if (file_transmit_socket < 0)
		file_transmit_socket = accept(listen_socket, NULL, NULL);
		
	if (0 < file_transmit_socket)
	{
		ofstream fout;
		cout << "file name : " << dest << "**" << endl;
		fout.open(dest, ios_base::out|ios_base::binary);
		int errsv = errno;
		if (!fout)
		{
			cout << "Create file failed\n";
			cout << "error num : " << errsv << endl;
			return NULL;
		}
		char buffer[SEND_BUFFER_SIZE];
		memset(buffer, 0, SEND_BUFFER_SIZE);
		
		int bytes = read(file_transmit_socket, buffer, SEND_BUFFER_SIZE);
		
		errsv = errno;
		cout << errsv << endl;
		while (bytes <= 0 && errsv == EAGAIN && strlen(buffer) == 0)
		{
			bytes = read(file_transmit_socket, buffer, SEND_BUFFER_SIZE);
			errsv = errno;
		}
		cout << "receive " << bytes << " bytes\n";
		bytes = strlen(buffer);
		while (0 < bytes)
		{
			//write(file_fd, buffer, bytes);
			fout << buffer;
			memset(buffer, 0, SEND_BUFFER_SIZE);
			bytes = read(file_transmit_socket, buffer, SEND_BUFFER_SIZE);
			errsv = errno;
			while (bytes <= 0 && errsv == EAGAIN && strlen(buffer) == 0)
			{
				bytes = read(file_transmit_socket, buffer, SEND_BUFFER_SIZE);
				errsv = errno;
			}
			bytes = strlen(buffer);
			cout << "receive " << bytes << " bytes\n";
		}
		close(file_transmit_socket);
		cout << "Close socket at 9223\n";
		fout.close();
	}
	else
		cout << "Fail! Give up!\n";
	cout << "Receive " << dest << " success!\n";
	//print_cwd(cwd, false);
	finish = true;
	return NULL;
}

void * send_file_post(void * arg)
{
	cout << endl;
	struct sockaddr_in data_sock_addr;
	memset(&data_sock_addr, 0, sizeof(data_sock_addr));
	data_sock_addr.sin_family = AF_INET;					//protocol family
	data_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);	//set control socket address as local address
	data_sock_addr.sin_port = htons(9222);				//set control socket port
	socklen_t length = sizeof(data_sock_addr);
	
	int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	int control_socket_opt = 1;
	setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &control_socket_opt, sizeof(control_socket_opt));	
	if (listen_socket < 0)
	{
		cout << "Create socket failed!\n";
		return NULL;
	}
	
	if (bind(listen_socket, (struct sockaddr *) &data_sock_addr, length) < 0)
	{
		cout << "Bind socket failed!\n";
		return NULL;
	}
	
	if (getsockname(listen_socket, (struct sockaddr *) &data_sock_addr, &length) < 0)
	{
		cout << "Get socket name failed!\n";
		return NULL;
	}
	
	file_transmit_post_arg myArg = *((file_transmit_post_arg *) arg);
	int control_socket_fd = myArg.socket_fd;
	
	write(control_socket_fd, &data_sock_addr, sizeof(data_sock_addr));
	
	cout << "Create socket at 9222 success!\n";
	
	listen(listen_socket, 1);
	
	int file_transmit_socket = accept(listen_socket, NULL, NULL);
	//try two more times
	if (file_transmit_socket < 0)
		file_transmit_socket = accept(listen_socket, NULL, NULL);
	if (file_transmit_socket < 0)
		file_transmit_socket = accept(listen_socket, NULL, NULL);
		
	if (0 < file_transmit_socket)
	{
		int src_file = open(myArg.file_name, O_RDONLY, S_IREAD);
		if (0 <= src_file)
		{
			//cout << "open file ok\n";
			char buffer[RCV_BUFFER_SIZE];
			memset(buffer, 0, RCV_BUFFER_SIZE);
			int bytes = read(src_file, buffer, RCV_BUFFER_SIZE);
			while (0 < bytes)
			{
				write(file_transmit_socket, buffer, bytes);
				cout << "Send " << bytes << " bytes\n";
				memset(buffer, 0, RCV_BUFFER_SIZE);
				bytes = read(src_file, buffer, RCV_BUFFER_SIZE);
			}
			cout << "Send finish\n";
			close(src_file);
			close(file_transmit_socket);
		}
		else
			cout << "Open file fail\n";
	}
	else
		cout << "Fail! Give up!\n";
	cout << "Receive " << myArg.file_name << " success!\n";
	//print_cwd(cwd, false);
	finish = true;
	return NULL;
}

void print_reply(char * reply)
{
	//assume that the socket communication is reliable,
	//we receive message as the order they sent
	//! but it read two packets in one time
	memset(cwd, 0, PATH_BUFFER_SIZE);
	
	int reply_len = strlen(reply);
	//get cwd
	for (int i = 0; i < reply_len; i ++)
	{
		if (reply[i] == '\n')
			break;
		else
			cwd[i] = reply[i];
	}
	//get message
	int cwd_len = strlen(cwd);
	for (int i = cwd_len + 1; i < reply_len; i ++)
		reply[i - cwd_len - 1] = reply[i];
	reply[reply_len - cwd_len - 1] = '\0';
	
	if (strlen(reply) != 0)
		cout << reply << endl;
	print_cwd(cwd, false);
}
