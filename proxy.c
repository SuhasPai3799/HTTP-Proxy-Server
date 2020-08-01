#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <ctype.h>
#include <arpa/inet.h>
#include <netdb.h>


#include "proxy_parse.h"


// This function constructs the request to be sent to the server from the parsed request received.
char * form_request(struct ParsedRequest *req)
{
	ParsedHeader_set(req,"Host",req->host);
	ParsedHeader_set(req, "Connection", "close");
	int tot = ParsedHeader_headersLen(req);
	char *res = (char*)malloc(tot+1);
	ParsedRequest_unparse_headers(req,res,tot);
	

	char * final_msg = (char*)malloc(1024);
	strcpy(final_msg,"");
	strcpy(final_msg,req->method);
	strcat(final_msg," ");
	strcat(final_msg,req->path);
	strcat(final_msg," ");
	strcat(final_msg,req->version);
	strcat(final_msg,"\r\n");
	strcat(final_msg,res);
	
	return final_msg;
}

//This function converts the received request into a sendable format and sends it to the server. Then it forwards the response received to the client.
void handler(int client_socket)
{
	char request_msg[1024] = {0};
	char * cur_msg = (char*)malloc(1024);
	while(strstr(request_msg, "\r\n\r\n") == NULL)
	{
		read( client_socket , cur_msg, 1024); 
		strcat(request_msg,cur_msg);
	}
	char* index = strstr(request_msg, "\r\n\r\n"); 
	if(index == NULL)
	{
		strcat(request_msg,"\r\n\r\n");
	}
	struct ParsedRequest *req;
	req = ParsedRequest_create();
	int len = strlen(request_msg);
	
	if(len == 0)
	{
		char *response = (char*)malloc(100);
		strcat(response,"Bad request");
		send(client_socket, response, strlen(response), 0 ); 
		return;
	}
	char * bad_req = (char*)malloc(100);
	strcpy(bad_req,"Error");
    if (ParsedRequest_parse(req, request_msg, len) < 0) {
       printf("parse failed - %s \n ", request_msg);
       return ;
    }
  //hostname stores the destination url.
    char * host_name = (char*)malloc(1024);
    strcpy(host_name,req->host);
    if(req->port == NULL)
    {
    	req->port = (char*)malloc(5);
    	strcpy(req->port,"80");
    }
    char *final_request = (char*)malloc(1024);
    strcpy(final_request,form_request(req));
    char *IPbuffer; 
    struct hostent *host_entry; 
    host_entry = gethostbyname(host_name); 
    IPbuffer = inet_ntoa(*((struct in_addr*) 
                           host_entry->h_addr_list[0])); 
   

    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return ;
    }
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_addr.s_addr = inet_addr(IPbuffer);
    serv_addr.sin_port = htons(atoi(req->port)); 
  
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) !=0) 
    { 
        printf("\nConnection Failed \n"); 
        return ;
    } 
    send(sock , final_request, strlen(final_request) , 0 ); 
    char msg_from_server[1024];
    long int tot_bytes = 0;
    int i=0;
    char* msg=(char*)malloc(400000);
    while(1)
    {	
    	long int  cur_bytes = read( sock , msg_from_server,1);
    	tot_bytes += cur_bytes;
    	msg[i]=msg_from_server[0];
    	if(cur_bytes == 0)
    		break;
    	i+=1;

    }
    strcat(msg,"\0");
    send(client_socket, msg, strlen(msg) , 0 ); 
  	close(client_socket);
    return ;
   
}

int main(int argc, char * argv[]) {
  if(argc == 1)
  {
  	printf("Please enter a port number\n");
  	return 0;
  }
  /**************** Proxy server set-up begins *********************/

  
  int port_no = atoi(argv[1]);
  int sock_fd;
  int opt = 1;
  struct sockaddr_in server_sock; // Proxy acting as a server_sock
  int addrlen = sizeof(server_sock);

  sock_fd = socket(AF_INET, SOCK_STREAM,0);
  server_sock.sin_family = AF_INET;
  server_sock.sin_addr.s_addr = INADDR_ANY;
  server_sock.sin_port = htons(port_no);
  if(sock_fd<0)
  {
  	printf("Could not create the socket , please try again\n");
  	return 0;
  }
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))) 
  { 
    perror("setsockopt"); 
    exit(EXIT_FAILURE); 
  } 
  if( bind(sock_fd,(struct sockaddr* )&server_sock , sizeof(server_sock))<0)
  {
  	printf("Binding error. Exiting\n");
  	return 0;
  }

  /***************** Proxy server set up done **********************/
  


  listen(sock_fd,100);

  int new_socket ;
  while(1)
  {
  	if ((new_socket = accept(sock_fd, (struct sockaddr *)&server_sock,  
                       (socklen_t*)&addrlen))<0) 
    { 
        perror("accept"); 
        exit(EXIT_FAILURE); 
    }
    // A new process is forked when a request is received.
    int pid = fork();
    if(pid==0)
    {
	    
	    
	    handler(new_socket);
	    close(new_socket);
	    break;
	}
	else
	{
		close(new_socket);
	}
  }
  return 0;
}