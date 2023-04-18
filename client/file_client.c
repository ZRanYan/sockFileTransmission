#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>    
#include <unistd.h>
#include <dirent.h> 
#include <sys/stat.h>  
#include <stdbool.h>
#include <mqueue.h>
#include <pthread.h>
#include "file_md5.h"
#include <signal.h>


#define SERVER_PORT                   6969      //设置服务器开放的端口
#define BUFFER_SIZE                   1024      //一次数据包传输的文件数据大小
#define FILE_NAME_MAX_SIZE            512       

#define LOCAL_FILE_PATH		    "/data1/ran/workfile/ran/sock_file/client"  //本地需要扫描的文件夹
#define QUEUE_NAME              "/my_queue"       //定义消息队列的名字 

//#define DEBUG

pthread_t m_thread;                         //定义一个线程
static volatile bool Is_pause=false;        //是否暂停的标志位
int g_client_socket;                         //全局变量，等待连接到服务器后初始化
typedef struct sock_data{
	char command; 
	unsigned int packet_num;
	char data_buff[BUFFER_SIZE];     //文件数据
}Sock_data;

void show_welcom(void)				//提供给用户选择界面
{
	printf("输入一下命令进行选择：\n");
	printf("1.扫描本地文件\n");
	printf("2.扫描服务器端文件\n");
	printf("3.开始文件传输\n");
	printf("4.结束传输\n");
}

void Send_data_to_server(int socket,Sock_data *data)     //将自定义的结构体信息发送给服务器
{
	if((send(socket,data,sizeof(Sock_data),0))==-1)
	{
		printf("send data failed,command: %c \n",data->command);//出错信息
	}

    #ifdef DEBUG
        sleep(1);
    #endif
}
Sock_data Receive_data_from_server(int socket)      
{
	Sock_data m_data_receive;
	bzero(&m_data_receive,sizeof(m_data_receive));
	if(0 == recv(socket,&m_data_receive,sizeof(m_data_receive),0))
	{
		printf("recv error!\n");
        m_data_receive.command='e';               //e表示连接中断，服务器那边断开
	}		
	return m_data_receive;
}
/*
函数功能：解析服务器回传的文件名，并打印出来
*/
void show_file_name(char *data,unsigned int length)     
{
	unsigned int i=0,j=0,k=0;
	char m_buff[20];   		//暂存文件名
	printf("server file names:\n");
	for(i=0;i<length;i++){
		if((*(data+i))=='\\'){
			k++;
			m_buff[j]='\0';
			j=0;
			printf("%d. %s \n",k,m_buff);
			continue;
		}
		m_buff[j++]=*(data+i);
	}
}

/*
函数功能：向服务器发送指令获取服务器端文件列表信息
*/
void scan_server_file(int socket)
{
	Sock_data m_data;
	bzero(&m_data,sizeof(m_data));
	m_data.command='s';
	Send_data_to_server(socket,&m_data);
	bzero(&m_data,sizeof(m_data));
	m_data=Receive_data_from_server(socket);
	if('r' == m_data.command)        //对接收的信息多个判断
	{
		show_file_name(m_data.data_buff,m_data.packet_num);    //将文件名打印出来
	}
}
/*
函数功能：向服务器发送断开连接信息，主动断开
*/
void active_disconnect(int socket)
{
	Sock_data m_data;
	bzero(&m_data,sizeof(m_data));
	m_data.command='q';	
	Send_data_to_server(socket,&m_data);
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
/*
功能：将十六进制数组转换成整数
*/
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
/*
函数功能：接收从服务器传过来的文件数据，写入文件流
*/
int recv_file_data_from_server(FILE *fp,unsigned int file_length,char *file_verify,int socket)
{
    int write_length;
    Sock_data m_data_receive;
    int file_fd=fileno(fp);
    bzero(&m_data_receive,sizeof(m_data_receive));
    while(1){

            while(Is_pause){
            sleep(1);  
            printf("暂停文件下载传输，等待恢复\n");
            continue;
            }
        m_data_receive=Receive_data_from_server(socket);

        #ifdef DEBUG
            printf("接收传输帧的command:%c,packetnum:%d \n",m_data_receive.command,m_data_receive.packet_num);
        #endif
        if('e'==m_data_receive.command){
            printf("文件传输中断\n");
            break;
       }else if('D'==m_data_receive.command){
            int_to_char(m_data_receive.packet_num, file_verify);  //提取已经传输的数据包数
            write_length=fwrite(m_data_receive.data_buff,sizeof(char),BUFFER_SIZE,fp);  //写入文件
            fwrite(file_verify,sizeof(unsigned char),36,fp);                //将附加信息加进去文件末尾
            fseek(fp,-36,SEEK_CUR);
            if(write_length<BUFFER_SIZE){
                printf("file write failed!\n");
           } 
       }else if('E'==m_data_receive.command){
            write_length=fwrite(m_data_receive.data_buff,sizeof(char),m_data_receive.packet_num,fp);  //写入文件
            if((ftruncate(file_fd,file_length))==0){
              //  printf("文件截断成功，文件指针为：%d\n",ftell(fp));
            }
           printf("文件传输结束\n");
           fclose(fp);
           return 1;
       }
    }
    fclose(fp);
    return 0;
}
/*
函数功能：从文件流中读取文件信息，封装成数据包后发给服务器
*/
void read_file_data_to_server(FILE *fp,unsigned int packet_start,unsigned int packet_all,int socket)
{
    Sock_data m_data_send;
    int  file_block_length=0;
    unsigned int  transmit_packet_num=packet_start;
    fseek(fp,BUFFER_SIZE*packet_start,SEEK_SET);           //将文件的指针移到客户端的断点处
    bzero(&m_data_send,sizeof(m_data_send));
    
    while((file_block_length=fread(m_data_send.data_buff,sizeof(char),            //添加暂停上传服务
                    BUFFER_SIZE,fp))>0){
         while(Is_pause){
            sleep(1);  
            printf("暂停文件上传传输，等待恢复\n");
         }
         
         transmit_packet_num++;        //传输的包数自动加一
         m_data_send.command='D';      //添加传输数据的标识符
         m_data_send.packet_num=transmit_packet_num;
         if(transmit_packet_num==packet_all){
            m_data_send.command='E';      //代表最后一包数据
            m_data_send.packet_num=file_block_length;  //最后一包数据的记录传输数据长度，小于BUFFER_SIZE
         }
         #ifdef DEBUG
            printf("发送文件传输帧的command:%c,packetnum:%d \n",m_data_send.command,m_data_send.packet_num);
        #endif
         Send_data_to_server(socket,&m_data_send);
         bzero(&m_data_send,sizeof(m_data_send));     //清空文件包   

    }
    fclose(fp);
}

/*
函数功能：传入文件名信息，发送给服务器端
*/
void  Send_file_to_server(char *file_name,int socket)
{
    FILE *fp;
    Sock_data m_data_send;
    Sock_data m_data_receive;
    unsigned int filesize;
    unsigned int file_packet_num;
    char file_md5[33]={0};          //计算文件唯一的md5校验
    if(0!=access(file_name,R_OK|W_OK|F_OK)){                    //判断文件是否存在
        printf("文件在本目录不存在%s\n",file_name);
    }else{
        if(0 ==(md5File(file_name,file_md5)))
        {
            printf("文件校验成功:%s \n",file_md5);
        } 
        fp=fopen(file_name,"rb+");
        if(NULL==fp){
            printf("faile:%s open failed!\n",file_name);
        }else{
            printf("打开文件：%s\n",file_name);
            calculate_file_size(fp,&filesize,&file_packet_num); //计算文件长度和需要传输的包数
            //printf("计算文件：%s,的大小：%d,需要传输的包数：%d\n",file_name,filesize,file_packet_num);
            bzero(&m_data_send,sizeof(m_data_send));
            bzero(&m_data_receive,sizeof(m_data_receive));
            m_data_send.command='u';
            m_data_send.packet_num=filesize;
            memcpy(m_data_send.data_buff,file_md5,sizeof(file_md5)-1);
            memcpy(m_data_send.data_buff+32,file_name,strlen(file_name)+1);
            Send_data_to_server(socket, &m_data_send);   //发送给客户端，包含文件大小和md5信息，文件名字
            m_data_receive=Receive_data_from_server(socket);
           // printf("---%c---%d\n",m_data_receive.command,m_data_receive.packet_num);
            if('M'==m_data_receive.command){    //服务器端存在tmp文件
                unsigned int tmp_file_packet=m_data_receive.packet_num;
                read_file_data_to_server(fp,tmp_file_packet,file_packet_num,socket);
            }else if('U'==m_data_receive.command){  //服务器端不存在tmp文件
                read_file_data_to_server(fp,0,file_packet_num,socket);
            }
        } //end if NULL==fp
    }//end if access(file_name,R_OK|W_OK|F_OK)
}
/*
函数功能：输入文件名与sock描述符，从服务器端下载文件
*/
void Receive_file_from_server(char *file_name,int socket)
{
    char file_name_tmp[FILE_NAME_MAX_SIZE+1]={0};    //末尾加上.tmp文件
    Sock_data m_data_send;
    Sock_data m_data_receive;
    char file_verify[36]={0};                       //文件里面保存的验证信息：4字节包数+32字节md5
    unsigned int filesize;
    FILE *fp;
    memcpy(file_name_tmp,file_name,strlen(file_name));         //添加文件末尾.tmp标识符
    memcpy(file_name_tmp+strlen(file_name),".tmp",5);
    bzero(&m_data_send,sizeof(m_data_send));
    bzero(&m_data_receive,sizeof(m_data_receive));   //提取出来需要传输的文件长度
    if(scan_present_file(LOCAL_FILE_PATH, file_name_tmp, 1))//存在.tmp文件
    {
         m_data_send.command='t';
         memcpy(m_data_send.data_buff+32,file_name,strlen(file_name)+1);  //将文件名信息拷贝到结构体中
         fp=fopen(file_name_tmp,"rb+");
         fseek(fp,-36,SEEK_END);                    //移动文件指针到校验数据区
         unsigned char tmp_file_packt_num[4];       //读取已经保存tmp文件的已经传输的包数
         fread(tmp_file_packt_num,sizeof(char),4,fp);
         m_data_send.packet_num=char_to_int(tmp_file_packt_num);     //获取已经传输的文件包数封装到结构体中
         fread(m_data_send.data_buff,sizeof(char),32,fp);  //将tmp文件的md5验证信息封装到数据包里面
         memcpy(file_verify+4,m_data_send.data_buff,32);            //将tmp文件的md5信息提取到数组中
         Send_data_to_server(socket,&m_data_send);                      //发送tmp文件信息
         m_data_receive=Receive_data_from_server(socket); //接收服务器验证信息
         filesize=m_data_receive.packet_num;             //提取文件的长度信息
         if('T'==m_data_receive.command){
            fseek(fp,-36,SEEK_END);                     //移到文件区末尾
            if(1==(recv_file_data_from_server(fp,filesize,file_verify,socket))){
                if(rename(file_name_tmp,file_name)==0)
                {
                    #ifdef DEBUG
                    printf("更改名字成功\n");
                    #endif
                }
            }
         }else if('N'==m_data_receive.command){
            memcpy(file_verify+4,m_data_receive.data_buff,strlen(m_data_receive.data_buff));
            fseek(fp,0,SEEK_SET);
            if(1==(recv_file_data_from_server(fp,filesize,file_verify,socket))){
                if(rename(file_name_tmp,file_name)==0)
                {
                    #ifdef DEBUG
                    printf("更改名字成功\n");
                    #endif
                }
            }
         }
    }else{          //不存在tmp文件，则新建文件
        m_data_send.command='d';
        strncpy(m_data_send.data_buff,file_name,strlen(file_name));  //将文件名信息拷贝到结构体中
        fp=fopen(file_name_tmp,"w");                       //新建有一个.tmp文件
        Send_data_to_server(socket,&m_data_send);    //发送给服务器，需要下载文件名字
        m_data_receive=Receive_data_from_server(socket); //接收信息
        filesize=m_data_receive.packet_num;
        memcpy(file_verify+4,m_data_receive.data_buff,32);//提取出来md5信息,复制到file_verify
        if(1==(recv_file_data_from_server(fp,filesize,file_verify,socket))){
               if(rename(file_name_tmp,file_name)==0)
              {
                   #ifdef DEBUG
                   printf("更改名字成功\n");
                   #endif
              }
       }   
    }
}
/*
函数功能：提供用户选择上传或下载文件
*/
void select_upordown_server(int socket,mqd_t mqid,char *msg,size_t msg_len)
{
    char file_name[FILE_NAME_MAX_SIZE + 1];   //输入的文件名
    while(1){
    printf("请输入u选择上传文件,d选择下载文件,q返回上一层\n");
    mq_receive(mqid,msg,msg_len,NULL);         //阻塞接收队列信息
    if('u'==*msg) 
    {
        printf("请输入文件名:\n");
        mq_receive(mqid,msg,msg_len,NULL);         //阻塞接收队列信息
        memcpy(file_name,msg,strlen(msg)+1);
        printf("文件名为：%s\n",file_name);
        Send_file_to_server(file_name,socket);
    }else if('d'==*msg){
        printf("请输入文件名:\n");
        mq_receive(mqid,msg,msg_len,NULL);         //阻塞接收队列信息
        memcpy(file_name,msg,strlen(msg)+1);
        printf("文件名为：%s\n",file_name);
        Receive_file_from_server(file_name,socket);
    }else if('q' == *msg)
    {
        printf("返回上一层：\n");
        return;
    }else{
        printf("输入有误，请重新输入！\n");
        continue;
    }
        break;                         //跳出循环
    }//end while
    return;
}
/*
函数功能：socket初始化
*/
int socket_init(char *ip)
{
        struct sockaddr_in client_addr;                     //客户端IP配置
        bzero(&client_addr,sizeof(client_addr));
        
        client_addr.sin_family = AF_INET;                   
        client_addr.sin_addr.s_addr = htons(INADDR_ANY);    //设置主机IP的地址
        client_addr.sin_port = htons(0);                    //系统自动分配一个
        
        int client_socket = socket(AF_INET, SOCK_STREAM, 0);  
        if (client_socket < 0)  
        {  
            printf("Create Socket Failed!\n");  
            exit(1);  
        } 
        if(bind(client_socket,(struct sockaddr*)&client_addr,sizeof(client_addr)))
        {
            printf("Client Bind Port failed!\n");
            exit(1);
        }
        
        struct sockaddr_in server_addr;                     //服务器端IP的配置
        bzero(&server_addr,sizeof(server_addr));
        server_addr.sin_family=AF_INET;
        char buffer[BUFFER_SIZE];
        if(inet_aton(ip,&server_addr.sin_addr) == 0){
            printf("Server IP Address Error!\n");
            exit(1);
        }
        server_addr.sin_port=htons(SERVER_PORT);
        socklen_t server_addr_length=sizeof(server_addr);
        if(connect(client_socket,(struct sockaddr*)&server_addr,server_addr_length)<0)      //连接服务器
        {
            printf("Can not Connect to %s!\n",ip);
            exit(1);
        }else{
            printf("socket connect success.\n");
            inet_ntop(AF_INET,&server_addr.sin_addr,buffer,sizeof(buffer));
            printf("server:%s\t port:%d connected!\n",buffer,ntohs(server_addr.sin_port));
        }
        return client_socket;
}

/*
函数功能：定义线程，扫描客户端输入信息
*/
static void *thread_input(void *data)
{
    char input_data[10];
    mqd_t mqid;
	mqid=mq_open(QUEUE_NAME,O_CREAT|O_RDWR,0666,NULL);
    if(mqid==-1){
		printf("mqd_t error!\n");
		pthread_exit(NULL);
	}
    while(1){                   //一致扫描用户端输入，将输入信息传到消息队列中
        scanf("%s",input_data);

        if(strcmp(input_data,"P")==0)
        {
             printf("接收到了暂停命令,输入R退出\n");
             printf("-------------------------\n");
             Is_pause=true;
             continue;
        }else if(strcmp(input_data,"R")==0){
            printf("接收到了恢复命令，继续传输文件\n");
            printf("-------------------------\n");
            Is_pause=false;
            continue;
        }
        if(mq_send(mqid,input_data,sizeof(input_data),0)<0)
		{
			printf("send error\n");
		}
    }
}
static void sig_int(int sig) //处理用户ctrl+c退出操作
{
    printf("用户强制退出程序\n");
    close(g_client_socket);
    exit(0);
}

int main(int argc,char **argv)
{
	if(2 != argc){
		printf("Usage: ./%s + IP地址：\n", argv[0]);
		exit(1);
	}
    pthread_create(&m_thread,NULL,thread_input,NULL);    //开一个线程监听用户输入
    pthread_detach(m_thread);                            //分离线程

    mqd_t mqid;
    mqid=mq_open(QUEUE_NAME,O_CREAT|O_RDWR,0666,NULL);
    struct mq_attr m_attr;
    if(mq_getattr(mqid,&m_attr)==-1){   //获取队列属性
		perror("get attr failed");
		exit(1);
	}
    char *msg_recv;
	size_t msg_len=m_attr.mq_msgsize;
	msg_recv=(char*)malloc(msg_len);   //分配接收区的大小
    
    int m_client_socket=socket_init(argv[1]);
    if(m_client_socket==1)
    {
        printf("socket init failed\n");
        exit(1);
    }
    g_client_socket=m_client_socket;
    
    if(SIG_ERR==signal(SIGINT,sig_int))
    {
        printf("信号SIGINT注册失败\n");
    }
	show_welcom();	//给用户提供选择
	while(1)
	{
		printf("------------------------------\n");
		mq_receive(mqid,msg_recv,msg_len,NULL);  //阻塞接收队列信息，即用户输入信息
		switch(*msg_recv){
			case '1':{
                printf("扫描本地文件夹内文件名：\n");
				scan_present_file(LOCAL_FILE_PATH,NULL,0);				
			    }break;
			case '2':{
				printf("扫描服务器端的文件名:\n");
				scan_server_file(m_client_socket);        //给服务器发送扫描文件指令,s				
			    }break;
            case '3':{
                select_upordown_server(m_client_socket,mqid,msg_recv,msg_len); //传入消息队列信息
                 }break;
			case '4':{
				printf("结束程序并退出\n");
				active_disconnect(m_client_socket);		
				close(m_client_socket);
				return 0;
			    }break;
			default:{
				printf("你的输入有误，请重新输入编号：\n");
			    }break;
		}
        sleep(0.5);
	}
	return 0;
}
