#include<iostream>
#include<unistd.h>
#include<dirent.h>
#include<sys/socket.h>
#include<string.h>
#include<stdio.h>
#include<fcntl.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<pthread.h>
#include<errno.h>
#include <netinet/tcp.h>
#include<string>
#include<fstream>

#define ACCEPT_QUEUE_LENGTH 20
#define RCV_BUFFER_SIZE 10240
#define SEND_BUFFER_SIZE 10240
#define PATH_BUFFER_SIZE 10240
#define ROW_WIDTH 80

using namespace std;

int help(int socket_fd, char * buffer, const char * path);
int get(int socket_fd, char * buffer, const char * src, const char * path);
int get_post(int socket_fd, char * buffer, const char * src, const char * path);
int put(int socket_fd, char * buffer, const char * dest, const char * path);
int put_post(int socket_fd, char * buffer, const char * dest, const char * path);
int pwd(int socket_fd, char * buffer, const char * path);
int dir(int socket_fd, char * buffer, const char * path);
int cd(int socket_fd, const char * dest, char * buffer, char * path);
int quit(int socket_fd);

int bad_cmd(int socket_fd, char * buffer, const char * path);
int send_reply(int socket_fd, const char * buffer, int size);
int get_relative_wd(const char * absolute, int size, char * cwd);
int init(int port);
void * accept_one_client(void * arg);
void * send_file(void * arg);
void * send_file_post(void * arg);
void * receive_file(void * arg);
void * receive_file_post(void * arg);

struct file_transmit_arg
{
	char * file_name;
	int socket_fd;
};

int main()
{
	cout << "Hello world!\n";
	int listen_socket_fd = init(9221);
	if (listen_socket_fd < 0)
		return 1;
	
	listen(listen_socket_fd, ACCEPT_QUEUE_LENGTH);
	
	while (true)
	{
		int control_socket_fd = accept(listen_socket_fd, NULL, NULL);
		if (control_socket_fd < 0)
		{
			cout << "Accept client failed! Socket error!\n";
			continue;
		}
		
		/*int control_socket_opt = 1;
		if (setsockopt(control_socket_fd, IPPROTO_TCP, TCP_NODELAY, &control_socket_opt, sizeof(control_socket_opt)) == -1)
		{
			cout << "Set socket opt failed!\n";
			continue;
		}*/
		
		pthread_t ntid;
		if (pthread_create(&ntid, NULL, accept_one_client, &control_socket_fd) != 0)
			cout << "Accept client failed! Create thread error!\n";
	}
	
	return 0;
}

int init(int port)
{
	int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	int control_socket_opt = 1;
	setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &control_socket_opt, sizeof(control_socket_opt));	
	if (listen_socket < 0)
	{
		cout << "Create socket failed!\n";
		return -1;
	}
	
	struct sockaddr_in control_sock_addr;
	memset(&control_sock_addr, 0, sizeof(control_sock_addr));
	control_sock_addr.sin_family = AF_INET;					//protocol family
	control_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);	//set control socket address as local address
	control_sock_addr.sin_port = htons(port);				//set control socket port
	socklen_t length = sizeof(control_sock_addr);
	if (bind(listen_socket, (struct sockaddr *) &control_sock_addr, length) < 0)
	{
		cout << "Bind socket failed!\n";
		return -1;
	}
	
	if (getsockname(listen_socket, (struct sockaddr *) &control_sock_addr, &length) < 0)
	{
		cout << "Get socket name failed!\n";
		return -1;
	}
	
	
	cout << "Create socket at port " << ntohs(control_sock_addr.sin_port) << " success!\n";
	return listen_socket;
}


void * accept_one_client(void * arg)
{
	int control_socket_fd = *(int *) arg;
	cout << "Accept one client at " << control_socket_fd << " success!\n";
	
	bool _quit = false;
	char rcv_buffer[RCV_BUFFER_SIZE];
	char send_buffer[SEND_BUFFER_SIZE];
	char cur_absolute_path[PATH_BUFFER_SIZE];
	memset(cur_absolute_path, 0, PATH_BUFFER_SIZE);
	memset(send_buffer, 0, SEND_BUFFER_SIZE);
	memset(rcv_buffer, 0, RCV_BUFFER_SIZE);
	if (getcwd(cur_absolute_path, PATH_BUFFER_SIZE) == NULL)
	{
		cout << "Get current work directory failed!\n";
		return NULL;
	}	
	
	//send help message	
	if (0 != help(control_socket_fd, send_buffer, cur_absolute_path))
		return NULL;
	
	while (!_quit)
	{
		memset(rcv_buffer, 0, sizeof(rcv_buffer));
		int rcv_bytes = read(control_socket_fd, rcv_buffer, sizeof(rcv_buffer));
		int errsv = errno;
		while (rcv_bytes <= 0 && errsv == EAGAIN && strlen(rcv_buffer) == 0)
		{
			rcv_bytes = read(control_socket_fd, rcv_buffer, sizeof(rcv_buffer));
			errsv = errno;
		}
		if (0 < rcv_bytes)
		{
			cout << "receive command : " << rcv_buffer << endl; 
			
			bool has_space = false;
			int cmd_len = 0;
			for (int i = 0; i < strlen(rcv_buffer); i ++)
			{
				if (rcv_buffer[i] == ' ' || rcv_buffer[i] == '\t')
				{
					if (!has_space)
					{
						rcv_buffer[cmd_len] = ' ';
						cmd_len ++;
						has_space = true;
					}
				}
				else
				{
					rcv_buffer[cmd_len] = rcv_buffer[i];
					cmd_len ++;
					has_space = false;
				}
			}
			rcv_buffer[cmd_len] = '\0';
			
			char * cmd = strtok(rcv_buffer, " ");
			if (cmd == NULL)
			{
				if (0 != bad_cmd(control_socket_fd, send_buffer, cur_absolute_path))
				{
					cout << "Send error message failed!\n";
					close(control_socket_fd);
					return NULL;
				}
			}
			
			memset(send_buffer, 0, SEND_BUFFER_SIZE);
			if (strcmp(cmd, "?") == 0)
			{
				if (0 != help(control_socket_fd, send_buffer, cur_absolute_path))
				{
					cout << "Send help message failed!\n";
					close(control_socket_fd);
					return NULL;
				}
			}
			else if (strcmp(cmd, "pwd") == 0)
			{
				if (0 != pwd(control_socket_fd, send_buffer, cur_absolute_path))
				{
					cout << "Send pwd message failed!\n";
					close(control_socket_fd);
					return NULL;
				}
			}
			else if (strcmp(cmd, "dir") == 0)
			{
				if (0 != dir(control_socket_fd, send_buffer, cur_absolute_path))
				{
					cout << "Send dir message failed!\n";
					close(control_socket_fd);
					return NULL;
				}
			}
			else if (strcmp(cmd, "cd") == 0)
			{
				char * dest = strtok(NULL, " ");
				if (dest == NULL)
				{
					if (0 != bad_cmd(control_socket_fd, send_buffer, cur_absolute_path))
					{
						cout << "Send error message failed!\n";
						close(control_socket_fd);
						return NULL;
					}
				}
				else
				{
					if (0 != cd(control_socket_fd, dest, send_buffer, cur_absolute_path))
					{
						cout << "Send cd message failed!\n";
						close(control_socket_fd);
						return NULL;
					}
				}
			}
			else if (strcmp(cmd, "quit") == 0)
			{
				quit(control_socket_fd);
				_quit = true;
			}
			else if (strcmp(cmd, "get") == 0)
			{
				char * type = strtok(NULL, " ");
				if (strcmp(type, "pasv") == 0)
				{
					char * src = strtok(NULL, " ");
					if (0 != get(control_socket_fd, send_buffer, src, cur_absolute_path))
					{
						cout << "Send get message failed!\n";
						close(control_socket_fd);
						return NULL;
					}
				}
				else if (strcmp(type, "post") == 0)
				{
					cout << "ok1\n";
					usleep(50 * 1000);
					char * src = strtok(NULL, " ");
					if (0 != get_post(control_socket_fd, send_buffer, src, cur_absolute_path))
					{
						cout << "Send get message failed!\n";
						close(control_socket_fd);
						return NULL;
					}
					cout << "ok2\n";
				}
				else
				{
					if (0 != bad_cmd(control_socket_fd, send_buffer, cur_absolute_path))
					{
						cout << "Send error message failed!\n";
						close(control_socket_fd);
						return NULL;
					}
				}
			}
			else if (strcmp(cmd, "put") == 0)
			{
				char * type = strtok(NULL, " ");
				if (strcmp(type, "pasv") == 0)
				{
					char * src = strtok(NULL, " ");
					char * dest = strtok(NULL, " ");
					
					if (0 != put(control_socket_fd, send_buffer, dest, cur_absolute_path))
					{
						cout << "Send get message failed!\n";
						close(control_socket_fd);
						return NULL;
					}
				}
				else if (strcmp(type, "post") == 0)
				{
					char * src = strtok(NULL, " ");
					char * dest = strtok(NULL, " ");
					usleep(50 * 1000);
					cout << "ok111\n";
					if (0 != put_post(control_socket_fd, send_buffer, dest, cur_absolute_path))
					{
						cout << "Send get message failed!\n";
						close(control_socket_fd);
						return NULL;
					}
					cout << "ok112\n";
				}
				else
				{
					if (0 != bad_cmd(control_socket_fd, send_buffer, cur_absolute_path))
					{
						cout << "Send error message failed!\n";
						close(control_socket_fd);
						return NULL;
					}
				}
			}
			else
			{
				if (0 != bad_cmd(control_socket_fd, send_buffer, cur_absolute_path))
				{
					cout << "Send error message failed!\n";
					close(control_socket_fd);
					return NULL;
				}
			}
		}
		else
		{
			cout << "Client at " << control_socket_fd << " lose...\n";
			close(control_socket_fd);
			return NULL;
		}
	}
	
	cout << "Client at " << control_socket_fd << " quit\n";
}

char helpMSG[] = "Usage:\n\tget <relative source path> <relative destination path> : get a file from server\n\tput <relative source path> <relative destination path> : put a file to server\n\tpwd : print the absolute path of current work directory on server\n\tdir : list files/directories in server's current work directory\n\tcd <path> : change current work directory of server\n\t? : print this list\n\tquit : quit this application";
int help(int socket_fd, char * buffer, const char * path)
{
	//send relative cwd
	if (0 != get_relative_wd(path, strlen(path), buffer))
		return -1;
	int cwd_len = strlen(buffer);
	//assume cwd_len < SEND_BUFFER_SIZE
	buffer[cwd_len] = '\n';
	strcat(buffer, helpMSG);
	return send_reply(socket_fd, buffer, strlen(buffer));
}

//assume reply message could be send in one TCP transmission
//this assumption is rational, because this max size could be 
//set by setsockopt
int send_reply(int socket_fd, const char * buffer, int size)
{
	if (buffer == NULL || size <= 0)
	{
		cout << "Bad message!\n";
		return -1;
	}
	int send_bytes = write(socket_fd, buffer, size);
	if (send_bytes <= 0)
	{
		//only try one time
		if (errno == EINTR)
			send_bytes = write(socket_fd, buffer, size);
		else
		{
			cout << "Send message error!\n";
			cout << "==================Message fail to send==================\n"; 
			cout << buffer << endl;
			cout << "==================Message fail to send==================\n"; 
			return -1;
		}
	}

	if (send_bytes != size)
		return 1;
	
	cout << "send : " << buffer << endl;
	return 0;
}

int get_relative_wd(const char * absolute, int size, char * cwd)
{
	if (absolute == NULL || size <= 0)
	{
		cout << "Bad absolute path!\n";
		return 1;
	}
	int pos = -1;
	for (pos = size - 1; 0 <= pos; pos --)
		if (absolute[pos] == '/')
			break;
	//maybe cwd is '/'
	if (pos < 0)
	{
		cout << "Get cwd failed!\n";
		return 1;
	}
	
	if (strcmp(absolute, "/") == 0)
	{
		cwd[0] = '/';
	}
	else
	{
		for (int i = pos + 1; i < size; i ++)
			cwd[i - pos - 1] = absolute[i];
	}
	return 0;
}

int pwd(int socket_fd, char * buffer, const char * path)
{
	//send relative cwd
	if (0 != get_relative_wd(path, strlen(path), buffer))
		return -1;
	int cwd_len = strlen(buffer);
	//assume cwd_len < SEND_BUFFER_SIZE
	buffer[cwd_len] = '\n';
	strcat(buffer, path);
	return send_reply(socket_fd, buffer, strlen(buffer));
}

int quit(int socket_fd)
{
	close(socket_fd);
	return 0;
}

int dir(int socket_fd, char * buffer, const char * path)
{
	//send relative cwd
	if (0 != get_relative_wd(path, strlen(path), buffer))
		return -1;
	int cwd_len = strlen(buffer);
	//assume cwd_len < SEND_BUFFER_SIZE
	buffer[cwd_len] = '\n';
	
	int total_len = cwd_len + 1;
	
	//open cwd
	DIR * cwd = opendir(path);
	struct dirent * file_ent;
	//count file/directory number
	int count = 0;
	while ((file_ent = readdir(cwd)) != NULL)
		count ++;
	closedir(cwd);
	
	char tmp_buf[PATH_BUFFER_SIZE];
	memset(tmp_buf, 0, PATH_BUFFER_SIZE);
	sprintf(tmp_buf, "items count: %d\n", count);
	strcat(buffer, tmp_buf);
	total_len += strlen(tmp_buf);
	if (0 < count)
	{
		cwd = opendir(path);
		for (int i = 1; i <= count; i ++)
		{
			file_ent = readdir(cwd);
			memset(tmp_buf, 0, PATH_BUFFER_SIZE);
			
			//test whether this item is a directory
			char tmp_path[PATH_BUFFER_SIZE];
			memset(tmp_path, 0, PATH_BUFFER_SIZE);
			sprintf(tmp_path, "%s/%s", path, file_ent->d_name);
			int file_fd = open(tmp_path, O_RDONLY, S_IREAD);
			struct stat file_stat;
			fstat(file_fd, &file_stat);
			struct tm * m_time = localtime(&(file_stat.st_mtime));
			char * m_time_s = asctime(m_time);
			m_time_s[strlen(m_time_s) - 1] = '\0';
			int f_size = int (file_stat.st_size);
			if (S_ISDIR(file_stat.st_mode))	
				sprintf(tmp_buf, "dir\t%s\t%d\t%s\n", m_time_s, f_size, file_ent->d_name);
			else
				sprintf(tmp_buf, "file\t%s\t%d\t%s\n", m_time_s, f_size, file_ent->d_name);
				
			if (total_len + strlen(tmp_buf) + 1 < SEND_BUFFER_SIZE)
			{
				strcat(buffer, tmp_buf);
				total_len += strlen(tmp_buf);
			}
			else
				break;
		}
		closedir(cwd);
	}
	
	return send_reply(socket_fd, buffer, strlen(buffer));
}

int cd(int socket_fd, const char * dest, char * buffer, char * path)
{
	const char * rcv_buffer = dest;
	
	int dir_len = strlen(rcv_buffer);
	
	char err_buf[128];
	memset(err_buf, 0, 128);
	
	if (rcv_buffer[0] == '/')
	{//cd to an absolute dir
		DIR * cwd = opendir(rcv_buffer);
		if (cwd == NULL)
		{
			int errsv = errno;
			switch (errsv)
			{
			case EACCES:
				strcat(err_buf, "Fail to open this directory, permission denied!");
				break;
			case ENOENT:
				strcat(err_buf, "Fail to open this directory, no such directory!");
				break;
			case ENOTDIR:
				strcat(err_buf, "Fail to open this directory, not a directory!");
				break;
			default:
				strcat(err_buf, "Fail to open this directory!");
			}
		}
		else
		{
			memset(path, 0, PATH_BUFFER_SIZE);
			strcpy(path, rcv_buffer);
			closedir(cwd);
		}
	}
	else
	{//cd to a relative dir
		int pos = 0;
		bool legal = true;
		while (pos < dir_len && legal)
		{
			char tmp_path[PATH_BUFFER_SIZE];
			memset(tmp_path, 0, PATH_BUFFER_SIZE);
			
			//parse one level
			for (int i = pos; i < dir_len; i ++)
			{
				if (rcv_buffer[i] == '/')
				{
					pos = i + 1;
					break;
				}
				else
					tmp_path[i - pos] = rcv_buffer[i];
				if (i == dir_len - 1)
					pos = i + 1;
			}
			
			if (strlen(tmp_path) == 0)
				continue;
			
			if (strcmp(tmp_path, ".") == 0)
				continue;
			else
			{
				if (strcmp(tmp_path, "..") == 0)
				{
					if (strcmp(path, "/") == 0)
						continue;
					int last_index = strlen(path) - 1;
					while (0 <= last_index && path[last_index] != '/')
						last_index --;
					if (last_index == 0)
						path[1] = '\0';
					else
						path[last_index] = '\0';
				}
				else
				{
					char tmp_dir[PATH_BUFFER_SIZE];
					memset(tmp_dir, 0, PATH_BUFFER_SIZE);
					sprintf(tmp_dir, "%s/%s", path, tmp_path);
					
					DIR * cwd = opendir(tmp_dir);
					if (cwd == NULL)
					{
						int errsv = errno;
						switch (errsv)
						{
						case EACCES:
							strcat(err_buf, "Fail to open this directory, permission denied!");
							break;
						case ENOENT:
							strcat(err_buf, "Fail to open this directory, no such directory!");
							break;
						case ENOTDIR:
							strcat(err_buf, "Fail to open this directory, not a directory!");
							break;
						default:
							strcat(err_buf, "Fail to open this directory!");
						}
						legal = false;
					}
					else
					{
						memset(path, 0, PATH_BUFFER_SIZE);
						strcpy(path, tmp_dir);
						closedir(cwd);
					}
				}
			}
		}
	}
	
	//send relative cwd
	if (0 != get_relative_wd(path, strlen(path), buffer))
		return -1;
	int cwd_len = strlen(buffer);
	//assume cwd_len < SEND_BUFFER_SIZE
	buffer[cwd_len] = '\n';
	strcat(buffer, err_buf);
	return send_reply(socket_fd, buffer, strlen(buffer));	
}

int bad_cmd(int socket_fd, char * buffer, const char * path)
{
	//send relative cwd
	if (0 != get_relative_wd(path, strlen(path), buffer))
		return -1;
	int cwd_len = strlen(buffer);
	//assume cwd_len < SEND_BUFFER_SIZE
	buffer[cwd_len] = '\n';
	strcat(buffer, "Bad command!\n");
	strcat(buffer, helpMSG);
	return send_reply(socket_fd, buffer, strlen(buffer));
}

//bool finish = false;
int get(int socket_fd, char * buffer, const char * src, const char * path)
{
	
	pthread_t ntid;
	char file_name[PATH_BUFFER_SIZE];
	memset(file_name, 0, PATH_BUFFER_SIZE);
	sprintf(file_name, "%s/%s", path, src);
	bool begin = true;
	if (pthread_create(&ntid, NULL, send_file, (void *) file_name) != 0)
	{
		cout << "Get file failed! Create thread error!\n";
		begin = false;
	}
		
	//send relative cwd
	if (0 != get_relative_wd(path, strlen(path), buffer))
		return -1;
	int cwd_len = strlen(buffer);
	//assume cwd_len < SEND_BUFFER_SIZE
	buffer[cwd_len] = '\n';
	if (begin)
		strcat(buffer, "Begin get file");
	else
		strcat(buffer, "Fail to get file");
	strcat(buffer, src);
	return send_reply(socket_fd, buffer, strlen(buffer));	
}

int get_post(int socket_fd, char * buffer, const char * src, const char * path)
{
	cout << "ok3\n";
	char rcv_buffer[RCV_BUFFER_SIZE];
	memset(rcv_buffer, 0, sizeof(rcv_buffer));
	int rcv_bytes = read(socket_fd, rcv_buffer, sizeof(rcv_buffer));
	int errsv = errno;
	cout << "rcv bytes : " << rcv_bytes << endl;
	while (rcv_bytes <= 0 && errsv == EAGAIN && strlen(rcv_buffer) == 0)
	{
		rcv_bytes = read(socket_fd, rcv_buffer, sizeof(rcv_buffer));
		errsv = errno;
		cout << "rcv bytes : " << rcv_bytes << endl;
	}
	if (0 < rcv_bytes)
	{
		struct sockaddr_in data_sock_addr = *((sockaddr_in *)rcv_buffer);
		int file_transmit_socket = socket(AF_INET, SOCK_STREAM, 0);
		
		struct sockaddr_in peer_name;
		socklen_t namelen = sizeof(peer_name);
		if (getpeername(socket_fd, (struct sockaddr *)&peer_name, &namelen) != 0)
		{
			cout << "Get peer name failed!\n";
			return -1;
		}
		
		data_sock_addr.sin_addr.s_addr = peer_name.sin_addr.s_addr;
		
		cout << ntohs(data_sock_addr.sin_port) << " " << inet_ntoa(*(struct in_addr *)&data_sock_addr.sin_addr.s_addr) << endl;
		
		if (file_transmit_socket < 0)
		{
			cout << "Create socket failed!\n";
			return -1;
		}
		
		struct timeval max_wait_time;
		max_wait_time.tv_sec = 0;
		max_wait_time.tv_usec = 30 * 1000;
		if (setsockopt(file_transmit_socket, SOL_SOCKET, SO_RCVTIMEO, &max_wait_time, sizeof(max_wait_time)) != 0)
		{
			cout << "Set socket opt failed!\n";
			return -1;
		}
		if (connect(file_transmit_socket, (struct sockaddr *) &data_sock_addr, sizeof(struct sockaddr)) < 0)
		{
			cout << "Connect to server failed!\n";
			return -1;
		}
		
		cout << "Connect to client at 9223 success\n";

		pthread_t ntid;
		char file_name[PATH_BUFFER_SIZE];
		memset(file_name, 0, PATH_BUFFER_SIZE);
		sprintf(file_name, "%s/%s", path, src);
		
		file_transmit_arg my_arg;
		my_arg.file_name = file_name;
		my_arg.socket_fd = file_transmit_socket;
		
		bool begin = true;
		if (pthread_create(&ntid, NULL, send_file_post, (void *) &my_arg) != 0)
		{
			cout << "Get file failed! Create thread error!\n";
			begin = false;
		}
			
		//send relative cwd
		if (0 != get_relative_wd(path, strlen(path), buffer))
			return -1;
		int cwd_len = strlen(buffer);
		//assume cwd_len < SEND_BUFFER_SIZE
		buffer[cwd_len] = '\n';
		if (begin)
			strcat(buffer, "Begin get file");
		else
			strcat(buffer, "Fail to get file");
		strcat(buffer, src);
		return send_reply(socket_fd, buffer, strlen(buffer));	
	}
	else
		return -1;
}

int put(int socket_fd, char * buffer, const char * dest, const char * path)
{
	char file_name[PATH_BUFFER_SIZE];
	memset(file_name, 0, PATH_BUFFER_SIZE);
	sprintf(file_name, "%s/%s", path, dest);
	ofstream fout;
	fout.open(file_name, ios_base::out|ios_base::binary);
	int errsv = errno;
	if (fout == NULL)
	{
		if (errsv == EACCES)
		{
			//send relative cwd
			if (0 != get_relative_wd(path, strlen(path), buffer))
				return -1;
			int cwd_len = strlen(buffer);
			//assume cwd_len < SEND_BUFFER_SIZE
			buffer[cwd_len] = '\n';
			strcat(buffer, "Error, permission denied!");
			return send_reply(socket_fd, buffer, strlen(buffer));	
		}
	}
	else
		fout.close();
	
	bool begin = true;
	pthread_t ntid;
	if (pthread_create(&ntid, NULL, receive_file, (void *) file_name) != 0)
	{
		cout << "Put file failed! Create thread error!\n";
		begin = false;
	}
	
	//send relative cwd
	if (0 != get_relative_wd(path, strlen(path), buffer))
		return -1;
	int cwd_len = strlen(buffer);
	//assume cwd_len < SEND_BUFFER_SIZE
	buffer[cwd_len] = '\n';
	if (begin)
		strcat(buffer, "Begin put file ");
	else
		strcat(buffer, "Fail to put file ");
	strcat(buffer, dest);
	return send_reply(socket_fd, buffer, strlen(buffer));
}

int put_post(int socket_fd, char * buffer, const char * dest, const char * path)
{
	cout << "ok113\n";
	char rcv_buffer[RCV_BUFFER_SIZE];
	memset(rcv_buffer, 0, sizeof(rcv_buffer));
	int rcv_bytes = read(socket_fd, rcv_buffer, sizeof(rcv_buffer));
	int errsv = errno;
	cout << "rcv bytes : " << rcv_bytes << endl;
	while (rcv_bytes <= 0 && errsv == EAGAIN && strlen(rcv_buffer) == 0)
	{
		rcv_bytes = read(socket_fd, rcv_buffer, sizeof(rcv_buffer));
		errsv = errno;
		cout << "rcv bytes : " << rcv_bytes << endl;
	}
	if (0 < rcv_bytes)
	{
		cout << "ok114\n";
		struct sockaddr_in data_sock_addr = *((sockaddr_in *)rcv_buffer);
		int file_transmit_socket = socket(AF_INET, SOCK_STREAM, 0);
		
		struct sockaddr_in peer_name;
		socklen_t namelen = sizeof(peer_name);
		if (getpeername(socket_fd, (struct sockaddr *)&peer_name, &namelen) != 0)
		{
			cout << "Get peer name failed!\n";
			return -1;
		}
		
		data_sock_addr.sin_addr.s_addr = peer_name.sin_addr.s_addr;
		
		cout << ntohs(data_sock_addr.sin_port) << " " << inet_ntoa(*(struct in_addr *)&data_sock_addr.sin_addr.s_addr) << endl;
		
		if (file_transmit_socket < 0)
		{
			cout << "Create socket failed!\n";
			return -1;
		}
		
		struct timeval max_wait_time;
		max_wait_time.tv_sec = 0;
		max_wait_time.tv_usec = 30 * 1000;
		if (setsockopt(file_transmit_socket, SOL_SOCKET, SO_RCVTIMEO, &max_wait_time, sizeof(max_wait_time)) != 0)
		{
			cout << "Set socket opt failed!\n";
			return -1;
		}
		if (connect(file_transmit_socket, (struct sockaddr *) &data_sock_addr, sizeof(struct sockaddr)) < 0)
		{
			cout << "Connect to server failed!\n";
			return -1;
		}
		
		cout << "Connect to client at 9222 success\n";
		
		char file_name[PATH_BUFFER_SIZE];
		memset(file_name, 0, PATH_BUFFER_SIZE);
		sprintf(file_name, "%s/%s", path, dest);
		ofstream fout;
		fout.open(file_name, ios_base::out|ios_base::binary);
		int errsv = errno;
		if (fout == NULL)
		{
			if (errsv == EACCES)
			{
				//send relative cwd
				if (0 != get_relative_wd(path, strlen(path), buffer))
					return -1;
				int cwd_len = strlen(buffer);
				//assume cwd_len < SEND_BUFFER_SIZE
				buffer[cwd_len] = '\n';
				strcat(buffer, "Error, permission denied!");
				return send_reply(socket_fd, buffer, strlen(buffer));	
			}
		}
		else
			fout.close();
		
		bool begin = true;
		pthread_t ntid;
		file_transmit_arg my_arg;
		my_arg.file_name = file_name;
		my_arg.socket_fd = file_transmit_socket;
		if (pthread_create(&ntid, NULL, receive_file_post, (void *) &my_arg) != 0)
		{
			cout << "Put file failed! Create thread error!\n";
			begin = false;
		}
		
		//send relative cwd
		if (0 != get_relative_wd(path, strlen(path), buffer))
			return -1;
		int cwd_len = strlen(buffer);
		//assume cwd_len < SEND_BUFFER_SIZE
		buffer[cwd_len] = '\n';
		if (begin)
			strcat(buffer, "Begin put file ");
		else
			strcat(buffer, "Fail to put file ");
		strcat(buffer, dest);
		return send_reply(socket_fd, buffer, strlen(buffer));
	}
	else
		return -1;
}

void * send_file(void * arg)
{
	const char * src = (const char *) arg;
	int socket_fd = init(9222);
	
	listen(socket_fd, 1);
	int file_transmit_socket = accept(socket_fd, NULL, NULL);
	//try two more times
	if (file_transmit_socket < 0)
		file_transmit_socket = accept(socket_fd, NULL, NULL);
	if (file_transmit_socket < 0)
		file_transmit_socket = accept(socket_fd, NULL, NULL);
		
	if (0 < file_transmit_socket)
	{
		cout << "Accept success\n";
		int src_file = open(src, O_RDONLY, S_IREAD);
		if (0 <= src_file)
		{
			char buffer[SEND_BUFFER_SIZE];
			memset(buffer, 0, SEND_BUFFER_SIZE);
			int bytes = read(src_file, buffer, SEND_BUFFER_SIZE);
			while (0 < bytes)
			{
				write(file_transmit_socket, buffer, bytes);
				cout << "Send " << bytes << " bytes\n";
				memset(buffer, 0, SEND_BUFFER_SIZE);
				bytes = read(src_file, buffer, SEND_BUFFER_SIZE);
			}
			cout << "Send finish\n";
			close(src_file);
		}
		else
			cout << "Open file fail\n";
		close(file_transmit_socket);
		close(socket_fd);
		cout << "Close socket at 9222\n";
	}
	else
		cout << "Fail! Give up!\n";
	return NULL;
}

void * send_file_post(void * arg)
{
	struct file_transmit_arg * myArg = (struct file_transmit_arg *) arg;
	
	const char * src = myArg->file_name;
	int file_transmit_socket = myArg->socket_fd;
	
	if (0 < file_transmit_socket)
	{
		cout << "Accept success\n";
		int src_file = open(src, O_RDONLY, S_IREAD);
		if (0 <= src_file)
		{
			char buffer[SEND_BUFFER_SIZE];
			memset(buffer, 0, SEND_BUFFER_SIZE);
			int bytes = read(src_file, buffer, SEND_BUFFER_SIZE);
			while (0 < bytes)
			{
				write(file_transmit_socket, buffer, bytes);
				cout << "Send " << bytes << " bytes\n";
				memset(buffer, 0, SEND_BUFFER_SIZE);
				bytes = read(src_file, buffer, SEND_BUFFER_SIZE);
			}
			cout << "Send finish\n";
			close(src_file);
		}
		else
			cout << "Open file fail\n";
		close(file_transmit_socket);
		cout << "Close socket at 9222\n";
	}
	else
		cout << "Fail! Give up!\n";
	return NULL;
}

void * receive_file_post(void * arg)
{
	struct file_transmit_arg * myArg = (struct file_transmit_arg *) arg;
	
	const char * dest = myArg->file_name;
	int file_transmit_socket = myArg->socket_fd;
	
	if (0 < file_transmit_socket)
	{
		ofstream fout;
		cout << "put post file name " << dest << endl;
		fout.open(dest, ios_base::out|ios_base::binary);
		int errsv = errno;
		//if (file_fd < 0)
		if (!fout)
		{
			cout << "Create file failed\n";
			cout << errsv << endl;
			return NULL;
		}
		char buffer[SEND_BUFFER_SIZE];
		memset(buffer, 0, SEND_BUFFER_SIZE);
		
		int bytes = read(file_transmit_socket, buffer, SEND_BUFFER_SIZE);
		
		errsv = errno;
		while (bytes <= 0 && errsv == EAGAIN && strlen(buffer) == 0)
		{
			bytes = read(file_transmit_socket, buffer, SEND_BUFFER_SIZE);
			errsv = errno;
			cout << "receive 2 " << bytes << " bytes\n";

		}
		cout << "receive 1 " << bytes << " bytes\n";
		bytes = strlen(buffer);
		while (0 < bytes)
		{
			//write(file_fd, buffer, bytes);
			fout << buffer;
			memset(buffer, 0, SEND_BUFFER_SIZE);
			bytes = read(file_transmit_socket, buffer, SEND_BUFFER_SIZE);
			errsv = errno;
			cout << "receive 2 " << bytes << " bytes\n";
			/*while (bytes <= 0 && errsv == EAGAIN && strlen(buffer) == 0)
			{
				bytes = read(file_transmit_socket, buffer, SEND_BUFFER_SIZE);
				errsv = errno;
				cout << "receive 3 " << bytes << " bytes\n";
				cout << errsv << " " << strlen(buffer) << endl;
			}
			bytes = strlen(buffer);
			cout << "receive 4 " << bytes << " bytes\n";*/
		}
		close(file_transmit_socket);
		cout << "Close socket at 9223\n";
		fout.close();
	}
	else
		cout << "Fail! Give up!\n";
	cout << "Receive " << dest << " success!\n";
	return NULL;
}

void * receive_file(void * arg)
{
	const char * dest = (const char *) arg;
	int socket_fd = init(9223);
	
	listen(socket_fd, 1);
	int file_transmit_socket = accept(socket_fd, NULL, NULL);
	//try two more times
	if (file_transmit_socket < 0)
		file_transmit_socket = accept(socket_fd, NULL, NULL);
	if (file_transmit_socket < 0)
		file_transmit_socket = accept(socket_fd, NULL, NULL);
		
	if (0 < file_transmit_socket)
	{
		ofstream fout;
		cout << "file name " << dest << endl;
		fout.open(dest, ios_base::out|ios_base::binary);
		int errsv = errno;
		//if (file_fd < 0)
		if (!fout)
		{
			cout << "Create file failed\n";
			cout << errsv << endl;
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
		close(socket_fd);
		cout << "Close socket at 9223\n";
		fout.close();
	}
	else
		cout << "Fail! Give up!\n";
	cout << "Receive " << dest << " success!\n";
	return NULL;
}
