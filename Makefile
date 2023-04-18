

all:
	@echo "begain!"
	gcc ./client/file_client.c  ./client/file_md5.c -lrt -o ./client/client
	gcc ./server/file_server.c ./server/file_md5.c  -o ./server/server
clean:
	rm ./client/client ./server/server
