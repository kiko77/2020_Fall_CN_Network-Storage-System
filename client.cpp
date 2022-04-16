#include <iostream>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include "opencv2/opencv.hpp"

#define BUFF_SIZE 1024

using namespace std;
using namespace cv;


void *child(void* ptr);

Mat imgClient[128];
int localSocket, start, end;
int videofinish;
int main(int argc , char *argv[])
{

    int recved, port = atoi(argv[1]);
    localSocket = socket(AF_INET , SOCK_STREAM , 0);

    if (localSocket == -1){
        printf("Fail to create a socket.\n");
        return 0;
    }
	
    struct sockaddr_in info;
    bzero(&info,sizeof(info));

    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr("127.0.0.1");
    info.sin_port = htons(port);


    int err = connect(localSocket,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error\n");
        return 0;
    }

	struct stat sv = { 0 };
	if (stat("/clientdir", &sv) == -1) {	//mkdir
		mkdir("./clientdir", 0777);
	}

    char receiveMessage[BUFF_SIZE] = {};
	char buf[BUFF_SIZE] = {};
	char cmd[BUFF_SIZE], filename[BUFF_SIZE], path[BUFF_SIZE];
	int sent;

    while(1){
		
		// get cmd from stdin
		bzero(buf, sizeof(char) * BUFF_SIZE);
		fgets(cmd, BUFF_SIZE, stdin);
		cmd[strlen(cmd)-1] = '\0';
		strcpy(buf, cmd);
		
	// ls
		if (strcmp(cmd, "ls") == 0) {
			// send cmd to server
			sent = send(localSocket, buf, strlen(buf), 0);
			bzero(receiveMessage, sizeof(char) * BUFF_SIZE);
			int sz;
			while (1) {	
				// get size of ls chunk
				if ((recved = recv(localSocket, &sz, sizeof(int), 0)) < 0) { // wrong recv
					cout << "recv failed, with received bytes = " << recved << endl;
					break;
				}
				else {
					if (sz < 0) { break; } // finish
					// receive ls chunk
					if ((recved = recv(localSocket, receiveMessage, sz+1, MSG_WAITALL)) < 0) {
						cout << "recv failed, with received bytes = " << recved << endl;
						break;
					}
					else if (recved == 0) {
						cout << "<end>\n";
						break;
					}
					else {
						receiveMessage[sz] = '\0';
						printf("%s", receiveMessage);
					}
				}
			}
		}


	// put
		else if (strncmp(cmd, "put", 3) == 0) {
			if (cmd[3] != ' ') { printf("Command format error.\n"); }
			else {
				// get file path 
				for (int j = 4; j < strlen(cmd); j++) {
					filename[j - 4] = cmd[j];
				}
				filename[strlen(cmd) - 4] = '\0';
				strcpy(path, "./clientdir/");
				strcat(path, filename);
				
				// open file 
				int fd = open(path, O_RDONLY);
				if (fd < 0) {	// check the file
					printf("The %s doesn't exist.\n", filename);
				}
				else {
					// send cmd
					sent = send(localSocket, buf, sizeof(char) * BUFF_SIZE, 0);
					
					// send file size 
					struct stat st;
					fstat(fd, &st);
					int sz = st.st_size;
					sent = send(localSocket, (char*)& sz, sizeof(int), 0);
					
					// send file 
					while (read(fd, buf, sizeof(char) * BUFF_SIZE)) {
						sent = send(localSocket, buf, 1024, 0);
					}
					close(fd);
				}
			}
		}
	// get 
		else if (strncmp(cmd, "get", 3) == 0) {
			if (cmd[3] != ' ') { printf("Command format error.\n"); }
			else {
				// send cmd
				sent = send(localSocket, buf, sizeof(char) * BUFF_SIZE, 0);

				// get filename 
				for (int j = 4; j < strlen(cmd); j++) {
					filename[j - 4] = cmd[j];
				}
				filename[strlen(cmd) - 4] = '\0';

				// get file size
				int sz;
				bzero(receiveMessage, sizeof(char)* BUFF_SIZE);
				if ((recved = recv(localSocket, (char*)&sz, sizeof(int), 0)) < 0) { // wrong recv
					cout << "recv failed, with received bytes = " << recved << endl;
					break;
				}
				else {
					if (sz < 0) { printf("The %s doesn't exist.\n", filename); }
					else {
						// get file path 
						strcpy(path, "./clientdir/");
						strcat(path, filename);

						// create file
						int fd = open(path, O_WRONLY | O_CREAT);
						if (fd < 0) {
							printf("open %s error.\n", filename);
						}
						else {
							//get file
							while (sz > 0) {
								if ((recved = recv(localSocket, receiveMessage, sizeof(char) * BUFF_SIZE,MSG_WAITALL)) < 0) {
									cout << "recv failed, with received bytes = " << recved << endl;
									break;
								}
								else if (recved == 0) {
									cout << "<end>\n";
									break;
								}
								else {
									if (sz < BUFF_SIZE) {	//last chunk
										receiveMessage[sz] = '\0';
										write(fd, receiveMessage, sz);
										sz = 0;
									}
									else {	// other chunks
										write(fd, receiveMessage, 1024);
										sz -= 1024;
									}
								}
							}
							printf("getfinish.\n");
							close(fd);
						}
						
						
					}
				}

			}
		}
	// play 
		else if (strncmp(cmd, "play", 4) == 0) {
			if (cmd[4] != ' ') { printf("Command format error.\n"); }
			else {
				// get filename
				for (int j = 5; j < strlen(buf); j++) {
					filename[j - 5] = buf[j];
				}
				filename[strlen(buf) - 5] = '\0';
				// check if it is .mpg
				int chk = 0, flen = strlen(filename);
				if (filename[flen - 4] != '.') { chk = 1; }
				if (filename[flen - 3] != 'm') { chk = 1; }
				if (filename[flen - 2] != 'p') { chk = 1; }
				if (filename[flen - 1] != 'g') { chk = 1; }
				if (chk) { printf("The %s is not a mpg file.\n", filename); }
				else {

					// send cmd
					sent = send(localSocket, buf, strlen(buf), 0);
					bzero(receiveMessage, sizeof(char) * BUFF_SIZE);

					// receive width and height
					int width=0, height=0;
					if ((recved = recv(localSocket, &width, sizeof(int), MSG_WAITALL)) < 0) { // wrong recv
						cout << "width recv failed, with received bytes = " << recved << endl;
						break;
					}

					if (width == -1) { // file doesn't exist
						printf("The %s doesn't exist.\n", filename); 

					}
					else {

						if ((recved = recv(localSocket, &height, sizeof(int), MSG_WAITALL)) < 0) { // wrong recv
							cout << "height recv failed, with received bytes = " << recved << endl;
							break;
						}

						for (int k = 0; k < 128; k++) {
							imgClient[k] = Mat::zeros(height, width, CV_8UC3);
							//allocate container to load frames 
							// ensure the memory is continuous (for efficiency issue.)
							if (!imgClient[k].isContinuous()) {
								imgClient[k] = imgClient[k].clone();
							}
						}

						start = 0;
						end = -1;
						pthread_t pid;
						pthread_create(&pid, NULL, child, NULL);

						videofinish = 0;
						while (1) {
							if (start != end && end != -1) {
								// show frame from brffer
								imshow("Video", imgClient[start]);
								start = (start + 1) % 128;
								if (videofinish && start == end) { imshow("Video", imgClient[start]);  break; }
								
								//Press ESC on keyboard to exit
								// notice: this part is necessary due to openCV's design.
								// waitKey means a delay to get the next frame.
								char c = (char)waitKey(33.3333);
								if (c == 27) { //send to server?
									int esc = -1;
									sent = send(localSocket, &esc, sizeof(int), 0);
									break;
								}
							}
						}

						pthread_join(pid, NULL);
						destroyAllWindows();
					}
				}
			}
		}
	// else
		else {
			printf("Command not found.\n");
		}
    }
    printf("close Socket\n");
    close(localSocket);
    return 0;
}

void *child(void *ptr) {
	while (1) {
		if (end == -1 || (end+1)%128 != start) {
			end = (end + 1) % 128;
			int recved;
			// get imgSize
			int imgSize;
			if ((recved = recv(localSocket, &imgSize, sizeof(int), MSG_WAITALL)) < 0) { // wrong recv
				cout << "imgSize recv failed, with received bytes = " << recved << endl;
				break;
			}

			// finish 
			if (imgSize == -1) { videofinish = 1; break; }
			//if (imgSize != 1555200) { printf("imgSize = %d\n", imgSize); }

			// allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
			uchar buffer[imgSize];

			if ((recved = recv(localSocket, buffer, sizeof(unsigned char) * imgSize, MSG_WAITALL)) < 0) { // wrong recv
				cout << "imgSize recv failed, with received bytes = " << recved << endl;
				break;
			}

			// copy a fream from buffer to the container on client
			uchar* iptr = imgClient[end].data;
			memcpy(iptr, buffer, imgSize);
			
		}
	}
	pthread_exit(NULL);
}