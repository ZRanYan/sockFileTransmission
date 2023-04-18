#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <fcntl.h> 
#include <unistd.h>
#include <dirent.h>  
#include <sys/stat.h> 
#include <stdbool.h>
#include <unistd.h>
#include "file_md5.h"

#define SERVER_PORT    			   6969
#define LENGTH_OF_LISTEN_QUEUE     20
#define BUFFER_SIZE                1024
#define FILE_NAME_MAX_SIZE         512

#define LOCAL_FILE_PATH "/data1/ran/workfile/ran/sock_file/server"  //服务器保存的路径名

//#define DEBUG

typedef struct sock_data{
	char command; 
	unsigned int packet_num;
	char data_buff[BUFFER_SIZE];                                      
}Sock_data;                                                             //文件数据结构体
Sock_data server_data;				                                    //接收客户端发来的数据包
void scan_dirto_sock(Sock_data *socket_data)                            //扫描文件夹内的文件名到结构体中
{
	int m_mun=0;                                                        //文件名长度
	DIR *dp; 
	struct dirent *entry;
	char filepath[100][100];
	int dp_file_num=0;                                                  //扫描到的文件个数
	if((dp = opendir(LOCAL_FILE_PATH)) == NULL)                         //检查目录是否存在
    {
        puts("can't open dir.");
        return ;
    }
	while((entry = readdir(dp)) != NULL)
    {
		if(strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
        {
            continue;
        }
		strncpy(filepath[dp_file_num], entry->d_name,sizeof(entry->d_name));
		dp_file_num++;
	}
	int i=0;
	for(i=0;i<dp_file_num;i++)
	{
		 strncpy(&(socket_data->data_buff[m_mun]),filepath[i],strlen(filepath[i]));
		 m_mun+=strlen(filepath[i]);
		 strncpy(&(socket_data->data_buff[m_mun]),"\\",1);
		 m_mun++;
	}
	socket_data->data_buff[m_mun]='\0';                             //能够使用printf打印出来
	socket_data->packet_num=m_mun;
	closedir(dp);                                                  //关闭文件描述符
}
/*
功能：unsigned int 类型转化成十六进制数组
*/
void int_to_char(unsigned int num,unsigned char *data)  //将int类型传化成unsigned char类型
{
    unsigned int tmp=num;
    unsigned char i=0;
    for(i=0;i<4;i++){
        data[i]=(tmp>>(8*i));
    }
}
unsigned int char_to_int(unsigned char *data) //将unsigned char 转化成 unsigned int 类型
{
    unsigned int tmp=0;
    unsigned char i=0;
    for(i=0;i<4;i++)
    {
        tmp|=(data[i]<<(8*i));
    }
    return tmp;
}
/*
功能：扫描给定路径的文件内的文件名，根据flag判断是否打印，还是搜索
dir：欲扫描的文件夹路径
file_name:文件名字
flag：为1代表判断，为0代表只打印文件名
返回结果：
如果flag==0，返回0
如果falg==1，扫描到文件夹内有该文件，返回1，否则返回0
*/
int scan_present_file(char *dir,char *file_name,int flag)
{
    DIR *dp;
    struct dirent *entry;
    char name_tmp[FILE_NAME_MAX_SIZE+1]={0}; 
    int file_num=0;
    if(1==flag){
        memcpy(name_tmp,file_name,strlen(file_name)+1); 
    }
    if((dp=opendir(dir))==NULL){
        printf("can't open dir:%s.\n",dir);
        return 0;
    }
    while((entry=readdir(dp))!=NULL){
        if(strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
        {
                continue;				
        }
        if(1==flag){
            if(strcmp(entry->d_name,name_tmp)==0){
            closedir(dp);        //关闭文件夹描述符
            return 1;
            }
        }else{
            file_num++;
            printf("%d.%s\n",file_num,entry->d_name);
        }
    }
    closedir(dp);        //关闭文件夹描述符
    return 0;
}

void Send_data_to_client(int socket,Sock_data *data)                //给套接字发送数据
{
	if((send(socket,data,sizeof(Sock_data),MSG_DONTWAIT))==-1)
	{
		printf("send data failed,command: %c \n",data->command);
	}
        #ifdef DEBUG
        sleep(1);           //睡眠一秒
        #endif
    /*
    ssize_t tmp=write(socket,data,sizeof(Sock_data));
    #ifdef DEBUG
        sleep(1);           //睡眠一秒
        printf("send data result is :%ld\n",tmp);
    #endif
    */
}
/*
函数功能：将服务器扫描到的文件夹文件名传给连接客户端
*/
int Reply_file_name_formation(int socket)
{
	Sock_data m_data;
	bzero(&m_data,sizeof(m_data));
	m_data.command='r';
	scan_dirto_sock(&m_data);
	Send_data_to_client(socket,&m_data);                //回复扫描信息
}
/*
函数功能：获取文件的长度和需要传输的总包数
*/
void calculate_file_size(FILE *fp,unsigned int *file_size,unsigned int *file_packet_num)
{
    unsigned int *m_size=file_size;
    unsigned int *m_packet_num=file_packet_num;
    unsigned int last_file_data_length;
    fseek(fp,0,SEEK_END);
    *m_size=ftell(fp);
    fseek(fp,0,SEEK_SET);       //将文件指针移到文件头部
    *m_packet_num=((*m_size)/BUFFER_SIZE); //计算需要传输的总包数
    last_file_data_length=(*m_size)%BUFFER_SIZE;    //最后一包传输的数据
    if(0 == last_file_data_length)
    { 
    }else{
    (*m_packet_num)++; 
   }
}
void read_file_data_to_client(FILE *fp,unsigned int packet_start,unsigned int packet_all,int socket)
{
    Sock_data m_data_send;
    int  file_block_length=0;
    unsigned int  transmit_packet_num=packet_start;
    fseek(fp,BUFFER_SIZE*packet_start,SEEK_SET);           //将文件的指针移到客户端的断点处
    bzero(&m_data_send,sizeof(m_data_send));
    while((file_block_length=fread(m_data_send.data_buff,sizeof(char),\
                    BUFFER_SIZE,fp))>0){
         transmit_packet_num++;        //传输的包数自动加一
         m_data_send.command='D';      //添加传输数据的标识符
         m_data_send.packet_num=transmit_packet_num;
         if(transmit_packet_num==packet_all){
            m_data_send.command='E';      //代表最后一包数据
            m_data_send.packet_num=file_block_length;  //最后一包数据的记录传输数据长度，小于BUFFER_SIZE
         }
         #ifdef DEBUG
            printf("传输帧的command:%c,packet_num:%d(代表传输的第几包)\n",m_data_send.command,m_data_send.packet_num);
         #endif
         
         Send_data_to_client(socket,&m_data_send);
         bzero(&m_data_send,sizeof(m_data_send));     //清空文件包   
    }
    fclose(fp);
}

void Read_file_tmp_to_client(int socket,Sock_data data)         //提取出来文件的md5信息
{
    FILE *fp;
    Sock_data m_data_send;
    unsigned int  filesize;                    //读取文件的大小
    char file_md5[33]={0};          //获取文件唯一的md5校验
    bool file_flag;                 //为true表示传输文件的大小是BUFFER_SIZE的倍数
    if(0!=access(data.data_buff+32,R_OK|W_OK|F_OK))//数据包中的前32字节为md5校验数据
    {
        printf("文件不存在,或没有读取权限\n");
    }else
    {
        char client_file_md5[33];
        unsigned int file_packet_num;
        unsigned int last_file_data_length;
        unsigned int client_file_packnum=data.packet_num; //提取出来已经传输文件的包数
        memcpy(client_file_md5,data.data_buff,sizeof(client_file_md5)-1);
        client_file_md5[32]='\0';
        if(0==md5File((data.data_buff+32),file_md5))      //读取本地文件获取的md5校验值
         {
                file_md5[32]='\0';
         }
        fp=fopen(data.data_buff+32,"rb+");       //打开文件流
        calculate_file_size(fp,&filesize,&file_packet_num);
        #ifdef DEBUG
            printf("计算出文件%s的的大小%d,需要传输的包数:%d\n",data.data_buff+32,filesize,file_packet_num);
        #endif
        bzero(&m_data_send,sizeof(m_data_send));
        m_data_send.packet_num=filesize;
        if(0 == strcmp(file_md5,client_file_md5))//比较两个文件校验是否一样 
        {
            m_data_send.command='T';
            Send_data_to_client(socket,&m_data_send);  //回复确认信息，包含文件长度
            read_file_data_to_client(fp,client_file_packnum,file_packet_num,socket);
        }else{                                      //比较两个文件校验不一样
            m_data_send.command='N';
            memcpy(m_data_send.data_buff,file_md5,sizeof(file_md5));
            Send_data_to_client(socket,&m_data_send);  //回复确认信息，包含文件长度和新文件的md5信息
            read_file_data_to_client(fp,0,file_packet_num,socket);
        }
        
    }
}

void Read_file_to_client(int socket,Sock_data  data) //传递sock的文件描述符，和接收的数据包
{
    FILE *fp;
    Sock_data m_data_send;
    unsigned int filesize;                    //读取文件的大小
    unsigned int file_packet_num;            //传输文件需要封装的包数
    char file_md5[33]={0};          //计算文件唯一的md5校验
    if(0!=access(data.data_buff,R_OK|W_OK|F_OK))
    {
         printf("文件%s不存在，或没有读写权限\n",data.data_buff);
    }else
   {
        if(0 ==md5File(data.data_buff,file_md5))
        {
            #ifdef DEBUG
            printf("文件%s校验成功,校验值:%s\n",data.data_buff,file_md5);
            #endif
           
        }       
		fp=fopen(data.data_buff,"rb+");   

        if(NULL == fp)              //需要测试能不能正确打开文件
        {
            printf("file:%s open failed!\n",data.data_buff);
        }else{
            #ifdef DEBUG
                 printf("打开文件：%s\n",data.data_buff);
            #endif           
        calculate_file_size(fp,&filesize,&file_packet_num); //计算文件长度和需要传输的包数
        bzero(&m_data_send,sizeof(m_data_send));
        m_data_send.command='L';                //L代表传输文件长度标识符
        m_data_send.packet_num=filesize;        //将文件大小，字节为单位，封装到结构体中
        memcpy(m_data_send.data_buff,file_md5,sizeof(file_md5));     //将文件的md5信息封装到结构体中
        #ifdef DEBUG
        printf("文件:%s,大小:%d,包数:%d\n",data.data_buff,filesize,file_packet_num);
        #endif
        Send_data_to_client(socket, &m_data_send);   //发送给客户端，包含文件大小和md5信息
        read_file_data_to_client(fp,0,file_packet_num,socket);
        }
        #ifdef DEBUG
             printf("文件：%s传输完毕\n",data.data_buff);
        #endif
    }
 }
Sock_data Receive_data_from_client(int socket)              //从客户端读取一帧数据
{
	Sock_data m_data_receive;
	bzero(&m_data_receive,sizeof(m_data_receive));
	if(0 == recv(socket,&m_data_receive,sizeof(m_data_receive),0))
	{
		printf("recv from client error!\n");
        m_data_receive.command='e';                         //代表客户端删除文件时断开，接收到零
	}		
	return m_data_receive;
}
int recv_file_data_from_client(FILE *fp,unsigned int file_length,char *file_verify,int socket)
{
    int write_length;
    Sock_data m_data_receive;
    int file_fd=fileno(fp);
    bzero(&m_data_receive,sizeof(m_data_receive));
    while(1){
        m_data_receive=Receive_data_from_client(socket);
         #ifdef DEBUG
            printf("接收传输帧的command:%c,packet_num:%d(代表传输的第几包)\n",m_data_receive.command,m_data_receive.packet_num);
         #endif
        if('e'==m_data_receive.command){
            printf("客户端文件传输中断\n");
            fclose(fp);
            exit(0);
       }else if('D'==m_data_receive.command){
            int_to_char(m_data_receive.packet_num, file_verify);  //提取已经传输的数据包数
            write_length=fwrite(m_data_receive.data_buff,sizeof(char),BUFFER_SIZE,fp);  //写入文件
            fwrite(file_verify,sizeof(char),36,fp);                //将附加信息加进去文件末尾
            fseek(fp,-36,SEEK_CUR);
            if(write_length<BUFFER_SIZE){
                printf("file write failed!\n");
           } 
       }else if('E'==m_data_receive.command){
            write_length=fwrite(m_data_receive.data_buff,sizeof(char),m_data_receive.packet_num,fp);  //写入文件
            if((ftruncate(file_fd,file_length))==0){
            }
           fclose(fp);
           return 1;
       }
    }
    fclose(fp);
    return 0;
}

void Save_file_from_client(int socket,Sock_data  data)
{
    char file_name_tmp[FILE_NAME_MAX_SIZE+1]={0};    //末尾加上.tmp文件
    char file_name[FILE_NAME_MAX_SIZE]={0};
    Sock_data m_data_send;
    Sock_data m_data_receive;
    char file_verify[37]={0};                       //文件里面保存的验证信息：4字节包数+32字节md5
    unsigned int filesize;
    FILE *fp;
    memcpy(file_name_tmp,data.data_buff+32,strlen(data.data_buff)-31);         //添加文件末尾.tmp标识符
    memcpy(file_name,data.data_buff+32,strlen(data.data_buff)-31);         //添加文件末尾.tmp标识符
    memcpy(file_name_tmp+strlen(file_name_tmp),".tmp",5);
    filesize=data.packet_num;
    memcpy(file_verify+4,data.data_buff,32);
    bzero(&m_data_send,sizeof(m_data_send));
    bzero(&m_data_receive,sizeof(m_data_receive));              //提取出来需要传输的文件长度
    //printf("接收客户端上传来的文件名：%s---%s--%s \n",file_name_tmp,file_name,file_verify+4);
    if(scan_present_file(LOCAL_FILE_PATH, file_name_tmp,1)){    //搜索目录，看是否存在tmp文件
        char tmp_file_md5[33];
        unsigned char tmp_file_packet_num[4];       //读取已经保存tmp文件的已经传输的包数
        unsigned int  tmp_packet_num;
        fp=fopen(file_name_tmp,"rb+");
        fseek(fp,-36,SEEK_END);                    //移动文件指针到校验数据区
        fread(tmp_file_packet_num,sizeof(char),4,fp);
        fread(tmp_file_md5,sizeof(char),32,fp);
        tmp_file_md5[32]='\0';     
        file_verify[36]='\0';
        tmp_packet_num=char_to_int(tmp_file_packet_num);
        if(0==strcmp(file_verify+4,tmp_file_md5)){ //客户端和服务器存在的文件的MD5一致
           //  printf("两个文件校验一致\n");
             m_data_send.command='M';
             m_data_send.packet_num=tmp_packet_num;
             Send_data_to_client(socket,&m_data_send);
             fseek(fp,-36,SEEK_END);
             if(1==(recv_file_data_from_client(fp,filesize,file_verify,socket))){
                if(0==rename(file_name_tmp,file_name)){

                }
             }
             return;
        }else{                                      //md5文件不一致
           // printf("服务器端的tmp文件md5校验不正确，重新传输！\n");
            fseek(fp,0,SEEK_SET);                   //将文件指针移到文件头部
        }
    }else{
        //printf("服务器端文件不存，新建文件\n");
        fp=fopen(file_name_tmp,"w");
    }
    m_data_send.command='U';  //发送回复信息，表示服务器端没有tmp文件
    Send_data_to_client(socket,&m_data_send);
    if(1==(recv_file_data_from_client(fp,filesize,file_verify,socket))){
           if(0==rename(file_name_tmp,file_name)){
               // printf("更改名字成功\n");
           }
    }

}

int server_socket_init(void)
{
    struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	server_addr.sin_port = htons(SERVER_PORT);
	const int on = 1;
	int server_socket=socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		printf("Create Socket Failed!\n");
		exit(1);
	}
	if ( setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,(char *)&on,sizeof(on)) )   //能多次连接
	{
        printf("setsockopt reuseaddr failed\n");
	}    

	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)))
	{
		printf("Server Bind Port: %d Failed!\n", SERVER_PORT);
		exit(1);
	}
	if(listen(server_socket,LENGTH_OF_LISTEN_QUEUE))
	{
		printf("Server Listen Failed!\n");
		exit(1);
	}
    return server_socket;
}
int main()
{
	pid_t pid;
	int m_server_socket=server_socket_init();
	while(1){
		struct sockaddr_in client_addr;
		socklen_t length=sizeof(client_addr);
		int new_server_socket=accept(m_server_socket,(struct sockaddr*)&client_addr,&length);
		if((pid=fork())==0){	//创建子进程
			close(m_server_socket);
			if(new_server_socket<0)
			{
				printf("Server Accept Failed!\n");
				break;
			}
		while(1){	                    //一直循环等待接收信息
			if(0 == (recv(new_server_socket,&server_data,sizeof(server_data),0)))
			{
				printf("接收出错,客户端退出\n");
				close(new_server_socket);
				exit(0);
			}
			switch(server_data.command){      //判断command类型
				case 's':{
					Reply_file_name_formation(new_server_socket); //回复扫描到的信息
				}break;
                case 'd':{               //提取出来需要传输的文件名，客户端不存在的文件
                    Read_file_to_client(new_server_socket,server_data);
                }break;   
                case 't':{
                    Read_file_tmp_to_client(new_server_socket,server_data); //判断客户端文件的md5校验，并移动文件指针
                }break;
                case 'u':{              //从客户端提取出来文件名，开始接收文件
                    Save_file_from_client(new_server_socket,server_data); 
                }break;
				case 'q':{
					printf("结束连接,客户端退出\n");
					close(new_server_socket);
					exit(0);
				}break;
				default:{
                    printf("传输错误，强制退出！\n");
					close(new_server_socket);
					exit(0);
				}break;
			}//end switch
		  }
		  close(new_server_socket);
		  exit(0);
		}
		  close(new_server_socket);   
        while(waitpid(-1,NULL,WNOHANG)>0)    //等待子进程结束，杀掉僵尸进程
            continue;
	 }
	return 0;
}

