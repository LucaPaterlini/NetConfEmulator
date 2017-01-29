/* gcc -o project project.c -pthread */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <fcntl.h>


#define MAX_LANS 12
#define MAX_BRIDGES 5
#define MAXFD 120

#define CONF_FILE "conf1"
#define TIME_SLICE 10


#define SIZEBUF 10000
#define STARTBRIDGE 12000
#define STARTLAN 10000

#define VAL_SPLITTER " "
#define RECORD_SPLITTER "|"

#define SOCKET_ERROR   ((int)-1)


int nbridges,nlans;
int Matrix_of_Links[MAX_BRIDGES][MAX_LANS];

void print_fw_table(int fw[][2],int idt){
    int i;char tmp[SIZEBUF],rs[SIZEBUF];
    sprintf(rs,"\nFORWARD TABLE BRIDGE %d\n",idt);
    for(i=0;i<nlans;i++){
        sprintf(tmp,"lan dest: %d next hop:%d hops:%d \n",i,fw[i][0],fw[i][1]);
        strcat(rs,tmp);
    }
    printf("%s",rs);
}

void print_send(char buffer[],int idt){
    char tmp[SIZEBUF];
    sprintf(tmp,"\nMESSAGE SENDED BY BRDIGE %d\n",idt);
    strcat(tmp,buffer);
    printf("%s\n",tmp);
}

void print_receive(char buffer[],int idt){
    char tmp[SIZEBUF];
    sprintf(tmp,"\nMESSAGE RECEIVED BY BRIDGE%d\n",idt);
    strcat(tmp,buffer);
    printf("%s\n",tmp);
}

void loading_matrix(char a[]){
	int i,j,h,e;
    FILE *fd;
    char buff[255],*token;

	fd=fopen(a,"r");
    fgets(buff, 255, (FILE*)fd);
	sscanf(buff,"%d %d",&nbridges,&nlans);

	for(i=0;i<MAX_BRIDGES;i++)
        for(j=0;j<MAX_LANS;j++) 
            Matrix_of_Links[i][j]=0;

	for(i=0;i<nbridges;i++){
			fgets(buff, 255, (FILE*)fd);
			token = strtok(buff, VAL_SPLITTER);

			while( token != NULL ){
				h=atoi(token);
				Matrix_of_Links[i][h]=1;
     	 	    token = strtok(NULL, VAL_SPLITTER);
     	 	}
	}
    fclose(fd);
}

int extract(char *msg,int matrix[][2],int * source){

    int i=0,j;
    char *save_ptr1,*save_ptr2;
    char *token,*tokenvar;

    token = strtok_r(msg, RECORD_SPLITTER,&save_ptr1);
    *source=atoi(token);
    token = strtok_r(NULL, RECORD_SPLITTER,&save_ptr1);

    while( token != NULL ){
        j=0;
        tokenvar=strtok_r(token, VAL_SPLITTER,&save_ptr2);
        while(tokenvar!=NULL){
            matrix[i][j]=atoi(tokenvar);
            tokenvar = strtok_r(NULL, VAL_SPLITTER,&save_ptr2);j+=1;
        }
        token = strtok_r(NULL, RECORD_SPLITTER,&save_ptr1);i+=1;
    }
    return(i);
}

void compress(int idt,char *msg,int matrix[][2]){
    sprintf(msg,"%d|",idt);
    int i;char t[100];
    for(i=0;i<MAX_BRIDGES;i++){
        if (matrix[i][1]>=0){
            sprintf(t,"%d %d|",i,matrix[i][1]);
            strcat(msg,t);
        }
    }
}

void update_table(int idt,int F_T[][2],int source,int n,int U_T[][2]){
    int oldval,newval,dest,next_hop;
    while (n>0){
        n-=1;
        dest=U_T[n][0];
        oldval=F_T[dest][1];
        newval=U_T[n][1]+2;
        next_hop=F_T[dest][0];
        if((oldval==-1)||((oldval!=1)&&(idt>source))){
            F_T[dest][1]=newval;
            F_T[dest][0]=source;
        }
    }
}

void update_function(int idt,int FT[][2], char msg[]){
    int i,b_source,M_u[MAX_BRIDGES][2];
    int nrecord=extract(msg,M_u,&b_source);
    update_table(idt,FT,b_source,nrecord,M_u);
}

int new_port(unsigned short int local_port_number){

    struct sockaddr_in Local;
    int socketfd,ris,OptVal;
    socketfd = socket(AF_INET,SOCK_DGRAM,0);
    if (socketfd == SOCKET_ERROR) {
        printf ("socket() failed, Err: %d \"%s\"\n", errno,strerror(errno));
        exit(1);
    }

    OptVal = 1;
    ris = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (char *)&OptVal, sizeof(OptVal));
    if (ris == SOCKET_ERROR)  {
        printf ("setsockopt() SO_REUSEADDR failed, Err: %d \"%s\"\n", errno,strerror(errno));
        exit(1);
    }

    memset(&Local, 0,sizeof(Local));
    Local.sin_family =AF_INET;
    Local.sin_addr.s_addr=inet_addr("127.0.0.1");
    Local.sin_port=htons(local_port_number);
    ris=bind(socketfd,(struct sockaddr*)&Local,sizeof(Local));
    if (ris == SOCKET_ERROR)  {
        printf ("bind() 1 failed, Err: %d \"%s\"\n",errno,strerror(errno));
        exit(1);
    }
    printf("new_local_p=%d\n",local_port_number);
    return (socketfd);

}

int multi_sender(int socketfd,char* msg,int source_port,int number_ports,int ports[],int diff){ 
  struct sockaddr_in To;
  int  OptVal, addr_size;
  int ris,i;
  for(i=0;i<number_ports;i++){
    if ((ports[i]!=source_port)||(diff==STARTLAN)){
        To.sin_family  =  AF_INET;
        To.sin_addr.s_addr  = inet_addr("127.0.0.1");
        To.sin_port    =  htons(ports[i]+diff);
        addr_size = sizeof(struct sockaddr_in);
        ris = sendto(socketfd, msg, strlen(msg)+1 , 0, (struct sockaddr*)&To, addr_size);
       }
    }
  return(0);
}

void first_message_initialize(char buffer[],int idt){
    int i;char tmp[SIZEBUF];
    sprintf(buffer,"%d",idt);
    for(i=0;i<nlans;i++){
        if(1==Matrix_of_Links[idt][i]){
            sprintf(tmp,"|%d -1",i);strcat(buffer,tmp);
        }
    }
}


int bridge(int idt,int number_ports,int ports[]){

    int i,FT[MAX_BRIDGES][2],send;
    for(i=0;i<MAX_BRIDGES;i++){ FT[i][0]=-1; FT[i][1]=-1; }

    char buffer[SIZEBUF];
    fd_set rfds,wfds;
    struct timeval tv;
    struct sockaddr_in From;
    int msglen,h[MAXFD],retval,Fromlen;

    int VICmax=-1;
    for(i=0;i<number_ports;i++){
      h[i]=new_port(STARTBRIDGE+ports[i]);
        if(h[i]>VICmax) VICmax=h[i];
    }


    first_message_initialize(buffer,idt);
    update_function(idt,FT,buffer);
    tv.tv_sec = TIME_SLICE; 
    int source_port=idt*MAX_LANS;
    while(1){

        FD_ZERO(&rfds);

        for(i=0;i<number_ports;i++)FD_SET(h[i], &rfds);
        retval = select(VICmax+1, &rfds, NULL, NULL, &tv);
        for(i=0;i<number_ports;i++){
                if (FD_ISSET(h[i], &rfds)){ 
                    memset(&From, 0, sizeof(From));
                    Fromlen=sizeof(struct sockaddr_in);
                    msglen = recvfrom (h[i], buffer, (int)SIZEBUF, 0, (struct sockaddr*)&From, &Fromlen);
                    if (msglen>0){
                        print_receive(buffer,idt);
                        update_function(idt,FT,buffer);
                        source_port=ntohs(From.sin_port)-STARTLAN;
                    }
            }
        }

        print_fw_table(FT,idt);
        compress(idt,buffer,FT);
        print_send(buffer,idt);
        
        multi_sender(h[0],buffer,source_port,number_ports,ports,STARTLAN);
        sleep(TIME_SLICE);
    }
}

int lan(int number_ports,int ports[]){

	char buffer[SIZEBUF];
	fd_set rfds,wfds;
    struct timeval tv;
    struct sockaddr_in From;
    int i,msglen,h[MAXFD],retval,Fromlen;

    int VICmax=-1;
    for(i=0;i<number_ports;i++){
    	h[i]=new_port(STARTLAN+ports[i]);
        if(h[i]>VICmax) VICmax=h[i];
    }

    while(1){

        tv.tv_sec = 1;   
        FD_ZERO(&rfds);

        for(i=0;i<number_ports;i++)FD_SET(h[i], &rfds);

        retval = select(VICmax+1, &rfds, NULL, NULL, &tv);

        for(i=0;i<number_ports;i++){
                if (FD_ISSET(h[i], &rfds)){ 

                    memset(&From, 0, sizeof(From));
                    Fromlen=sizeof(struct sockaddr_in);
                    msglen = recvfrom (h[i], buffer, (int)SIZEBUF, 0, (struct sockaddr*)&From, &Fromlen);
                    
                    if (msglen>0){
                    	int source_port=ntohs(From.sin_port)-STARTBRIDGE;
                    	multi_sender(h[i],buffer,source_port,number_ports,ports,STARTBRIDGE);
                    }
            }
        }
    }
}

void *Bridges_Fun(void *threadid){
  int i,c=0;
  char rs[1000],tmp[1000];
  int ports[MAX_BRIDGES];
  sprintf(rs,"BRIDGES ID=%ld Lan_ports= ",(long int)threadid);
  for(i=0;i<MAX_BRIDGES;i++){
    if (Matrix_of_Links[(long int)threadid][i]==1){
      ports[c]=MAX_LANS*(long int)threadid+i;c+=1;
      sprintf(tmp,"%d ",ports[c-1]);strcat(rs,tmp);}
  }

  sprintf(tmp,"con %d porte\n",c);strcat(rs,tmp);
  printf("%s",rs);
  bridge((long int)threadid,c,ports);

  pthread_exit(NULL);
}

void *Lans_Fun(void *threadid){

	int i,c=0;
    char rs[1000],tmp[1000];
	int ports[MAX_LANS];
	sprintf(rs,"LAN ID=%ld Briges_ports= ",(long int)threadid);
	for(i=0;i<MAX_LANS;i++){
		if (Matrix_of_Links[i][(long int)threadid]==1){
			ports[c]=i*MAX_LANS+(long int)threadid;c+=1;
			sprintf(tmp,"%d ",ports[c-1]);strcat(rs,tmp);}
	}

	sprintf(tmp,"con %d porte\n",c);strcat(rs,tmp);
	printf("%s",rs);
    lan(c,ports);

	pthread_exit(NULL);
}

int main(){
	
	loading_matrix(CONF_FILE);
	pthread_t threads_lans[MAX_LANS];
	pthread_t threads_bridges[MAX_BRIDGES];
	long int rc,t;
	int i,j;

	for (t=0;t<nlans;t++){
		rc=pthread_create(&threads_lans[t],NULL,Lans_Fun,(void *)t);
		if(rc){
			printf("ERROR; return code from pthread_create() is %ld\n",rc);
			exit(-1);
		}
	}

    for (t=0;t<nbridges;t++){
        rc=pthread_create(&threads_bridges[t],NULL,Bridges_Fun,(void *)t);
        if(rc){
            printf("ERROR; return code from pthread_create() is %ld\n",rc);
            exit(-1);
        }
    }

	pthread_exit(NULL);
	return 0;
}

