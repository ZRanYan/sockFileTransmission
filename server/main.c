#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>


int main(int argc,char *argv[])
{
    if(2 != argc){
            printf("input two parament\n");
            return -1;
    }

    FILE *fp=NULL;
    int file_fd;
    
   
    fp=fopen(argv[1],"rb+");
	if(NULL == fp){
		perror("fopen file error");
		fclose(fp);
		return -2;
	}    

    file_fd=fileno(fp);
    fseek(fp,0,SEEK_END);
	int filesize=ftell(fp);       //文件流到文件描述符的转换
	fseek(fp,0,SEEK_SET);

    printf("文件:%s长度：%d-----文件描述符：%d\n",argv[1],filesize,file_fd);


    

    if((ftruncate(file_fd,filesize-906))==0)
    {
        printf("文件截断成功\n");
    }

	fclose(fp);
	
	return 0;
	
}
