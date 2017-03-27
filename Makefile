#.PHONY:all
#all:96communiteclient 96communiteserver
#96communiteclient:96communiteclient.c
#	gcc -g -o $@ $^


chatroomServer:chatroomServer.c
	gcc -o $@ $^

.PHONY:clean
clean:
	rm -f chatroomServer
