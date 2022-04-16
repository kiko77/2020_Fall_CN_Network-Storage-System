#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include "opencv2/opencv.hpp"
#include <signal.h>

#define BUFF_SIZE 1024
using namespace cv;
using namespace std;

int imgSize[128];
Mat imgServer[128];
int videofinish;
int start, end, i;
void* child(void* ptr);

int main(int argc, char** argv) {

	//signal(SIGPIPE, SIG_IGN);
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);

	int localSocket, remoteSocket, port = atoi(argv[1]);
	int maxfd, recved;

	struct  sockaddr_in localAddr, remoteAddr;

 	int addrLen = sizeof(struct sockaddr_in);

	localSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (localSocket == -1) {
		printf("socket() call failed!!");
		return 0;
	}

	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = INADDR_ANY;
	localAddr.sin_port = htons(port);

	char receiveMessage[BUFF_SIZE] = {};
	char buf[BUFF_SIZE] = {};

	maxfd = getdtablesize();

	if (bind(localSocket, (struct sockaddr*) & localAddr, sizeof(localAddr)) < 0) {
		printf("Can't bind() socket");
		return 0;
	}

	listen(localSocket, 1024);
	int sent;
	fd_set master_set, working_set;
	FD_ZERO(&master_set);
	FD_ZERO(&working_set);
	FD_SET(localSocket, &master_set);
	char filename[BUFF_SIZE], path[BUFF_SIZE];
	std::cout << "Waiting for connections...\n" << "Server Port:" << port << std::endl;
	while (1) {

		memcpy(&working_set, &master_set, sizeof(master_set));
		select(maxfd, &working_set, NULL, NULL, NULL);
		for (i = 0; i < maxfd; i++) {
			if (FD_ISSET(i, &working_set)) {

				//	new connection
				if (i == localSocket) {
					remoteSocket = accept(localSocket, (struct sockaddr*) & remoteAddr, (socklen_t*)& addrLen);

					if (remoteSocket < 0) {
						printf("accept failed!");
						return 0;
					}
					std::cout << "Connection accepted" << std::endl;	// print in server
									
					FD_SET(remoteSocket, &master_set);

					struct stat st = { 0 };
					if (stat("/serverdir", &st) == -1) {	//mkdir
						mkdir("./serverdir", 0777);
					}
				}

				// cmd from connected server
				else {					
					bzero(receiveMessage, sizeof(char) * BUFF_SIZE);

					// receive cmd
					if ((recved = recv(i, receiveMessage, sizeof(char) * BUFF_SIZE, 0)) < 0) { // wrong recv
						cout << "recv failed, with received bytes = " << recved << endl;
						break;
					}
					else if (recved == 0) { // end connection
						cout << "<end>\n";
						close(i);
						FD_CLR(i, &master_set);
						continue;
					}

					printf("%d:%s\n", recved, receiveMessage);	// successful recv
				
				// ls			
					if (strcmp(receiveMessage, "ls") == 0) {
						// open directory
						DIR* dp;
						struct dirent* ep;
						dp = opendir("./serverdir");
						bzero(buf, sizeof(char) * BUFF_SIZE);
						if (dp != NULL) {
							int sz;
							while (ep = readdir(dp)) { // see every file in dir and send
								sz = strlen(buf);
								if (sz + strlen(ep->d_name) > 1024) {
									buf[sz] = '\0';
									sent = send(i, &sz, sizeof(int), 0);
									sent = send(i, buf, sz+1, 0);
									bzero(buf, sizeof(char)* BUFF_SIZE);
								}
								strncat(buf, ep->d_name, strlen(ep->d_name));
								buf[sz+strlen(ep->d_name)] = '\n';
							}
							sz = strlen(buf);
							buf[sz] = '\0';
							sent = send(i, &sz, sizeof(int), 0);
							if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set); continue; }
							sent = send(i, buf, sz + 1, 0);
							if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set); continue; }
							sz = -1;
							sent = send(i, &sz, sizeof(int), 0);
							if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set); continue;}
							bzero(buf, sizeof(char) * BUFF_SIZE);
							
							(void)closedir(dp);
						}
						else
							perror("Couldn't open the directory");
					}
				//put
					else if (strncmp(receiveMessage, "put", 3) == 0) {

						// get path 
						for (int j = 4; j < strlen(receiveMessage); j++) {
							filename[j - 4] = receiveMessage[j];
						}
						filename[strlen(receiveMessage) - 4] = '\0';
						strcpy(path, "./serverdir/");
						strcat(path, filename);
						
						// get file size 
						int sz;
						if ((recved = recv(i, (char*)&sz, sizeof(int), 0)) < 0) { // wrong recv
							cout << "recv failed, with received bytes = " << recved << endl;
							break;
						}
						else {
							// create file 
							int fd = open(path, O_WRONLY | O_CREAT);
							if (fd < 0) {
								printf("open %s error.\n", filename);
							}
							else {
								// get file 
								while (sz > 0) {
									if ((recved = recv(i, receiveMessage, sizeof(char) * BUFF_SIZE, MSG_WAITALL)) < 0) {
										cout << "recv failed, with received bytes = " << recved << endl;
										break;
									}
									else if (recved == 0) {
										cout << "<end>\n";
										break;
									}
									else {
										// receive file chunk
										if (sz < BUFF_SIZE) {	// last chunk
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
								close(fd);
							}
							printf("putfinish.\n");
						}
						


					}
				// put
					else if (strncmp(receiveMessage, "put", 3) == 0) {
						// get file path 
						for (int j = 4; j < strlen(receiveMessage); j++) {
							filename[j - 4] = receiveMessage[j];
						}
						filename[strlen(receiveMessage) - 4] = '\0';
						strcpy(path, "./serverdir/");
						strcat(path, filename);
						printf("filename = %s\n", filename);
						
						//get file size 
						int siz;
						if ((recved = recv(i, &siz, sizeof(int), 0)) < 0) { // wrong recv
							cout << "recv failed, with received bytes = " << recved << endl;
							break;
						}
						else {
							// create file 
							printf("sus! sz = %d\n", siz);
							int fd = open(path, O_WRONLY | O_CREAT);
							if ( fd < 0) {
								printf("open %s error.\n", filename);
							}
							else {
								// get file 
								while (siz > 0) {
									if ((recved = recv(i, receiveMessage, sizeof(char) * BUFF_SIZE, 0)) < 0) {
										cout << "recv failed, with received bytes = " << recved << endl;
										break;
									}
									else if (recved == 0) {
										cout << "<end>\n";
										close(i);
										FD_CLR(i, &master_set);
										break;
									}
									else {
										
										if (siz < BUFF_SIZE) {	// last chunk
											receiveMessage[siz] = '\0';
											write(fd, receiveMessage, siz);
											siz = 0;
										}
										else {	// other chunks
											write(fd, receiveMessage, 1024);
											siz -= 1024;
										}
									}
								}
								close(fd);
							}
							printf("putfinish.\n");
						}
					}
				// get
					else if (strncmp(receiveMessage, "get", 3) == 0) {
						// get file path
						for (int j = 4; j < strlen(receiveMessage); j++) {
							filename[j - 4] = receiveMessage[j];
						}
						filename[strlen(receiveMessage) - 4] = '\0';
						strcpy(path, "./serverdir/");
						strcat(path, filename);

						// open file
						int fd = open(path, O_RDONLY);
						int sz;
						if (fd < 0) {
							sz = -1; 
							sent = send(i, (char*)& sz, sizeof(int), 0);
							if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set); }
						}
						else {
							// send file size
							struct stat st;
							fstat(fd, &st);
							int sz = st.st_size;
							sent = send(i, (char*)& sz, sizeof(int), 0);
							if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set);  continue; }

							// send file 
							while (read(fd, buf, sizeof(char) * BUFF_SIZE)) {
								if (sz < 1024) {
									sent = send(i, buf, sizeof(char) * BUFF_SIZE, 0);
									break;
								}
								// osther chunks
								sent = send(i, buf, sizeof(char) * BUFF_SIZE, 0);
								sz -= 1024;
								if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set);  break; }
							}
							close(fd);
						}
					}
				// play
					else if (strncmp(receiveMessage, "play", 4) == 0) {
						// get file path 
						for (int j = 5; j < strlen(receiveMessage); j++) {
							filename[j - 5] = receiveMessage[j];
						}
						filename[strlen(receiveMessage) - 5] = '\0';
						strcpy(path, "./serverdir/");
						strcat(path, filename); 
						// check file exist
						if (access(path, F_OK) != 0) { 
							int notexist = -1;
							sent = send(i, (char*) &notexist, sizeof(int), 0);
							if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set); continue; }
						}
						else {
							start = 0;
							end = -1;
							videofinish = 0;
							pthread_t pid;
							pthread_create(&pid, NULL, child, path);
							int esc;
							while (1) {
								if (recv(i, &esc, sizeof(int), MSG_DONTWAIT) > 0) {
									if (esc == -1) { break; }
								}
								if (start != end && end != -1) {
									// send size 
									sent = send(i, (char*)& imgSize[start], sizeof(int), 0);
									if (sent < 0 || sent == 0) {
										videofinish = 1; cout << "<end>\n"; FD_CLR(i, &master_set); break;
									}
									// send data
									sent = send(i, imgServer[start].data, imgSize[start], 0);
									if (sent < 0 || sent == 0) { videofinish = 1; cout << "<end>\n"; FD_CLR(i, &master_set); break; }

									start = (start + 1) % 128;
									if (videofinish && start == end) {
										sent = send(i, (char*)& imgSize[start], sizeof(int), 0);
										if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set);  break; }
										break;
									}
								}

							}
							pthread_join(pid, NULL);
						}
					}
				// else
					else {
						strcpy(buf, "Command not found.\n");
						sent = send(i, buf, strlen(buf), 0);
						if (sent < 0) { cout << "<end>\n"; FD_CLR(i, &master_set); }
					}
				}
			}
		}

	}

	return 0;
}

void* child(void* ptr) {
	char* path = (char*)ptr;
	VideoCapture cap(path);

	// get the resolution of the video
	int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
	int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);


	// send width and height
	int sent;
	//printf("w=%d, h = %d\n", width, height);
	sent = send(i, (char*)& width, sizeof(int), 0);
	sent = send(i, (char*)& height, sizeof(int), 0);

	for (int k = 0; k < 128; k++) {
		//allocate container to load frames 
		imgServer[k] = Mat::zeros(height, width, CV_8UC3);
		// ensure the memory is continuous (for efficiency issue.)
		if (!imgServer[k].isContinuous()) {
			imgServer[k] = imgServer[k].clone();
		}
	}

	while (1) {
		if (videofinish == 1) { break; }
		if (end == -1 || (end + 1)%128 != start) {
			end = (end + 1) % 128;
				
			//get a frame from the video to the container on server.
			cap >> imgServer[end];

			// if finish
			if (imgServer[end].empty()) {
				imgSize[end] = -1;
				videofinish = 1;
				break;
			}

			// get the size of a frame in bytes 
			imgSize[end] = imgServer[end].total() * imgServer[end].elemSize();

		}
	}
	cap.release();
	pthread_exit(NULL);
}