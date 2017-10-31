#include <iostream>
#include <map>
#include <utility>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <event.h>
#include <pthread.h>


using namespace std;

const int MAXFD = 10;


class Demultiplexer;
class Reactor;
class Handle;


//事件
class Handle
{
public:
	Handle(int fd = -1):Handlefd(fd) {}
	virtual ~Handle() { close(Handlefd); }

	virtual int handle_event(){}
	int get_fd() { return Handlefd; }
protected:
	int Handlefd;
};

class HandleB :public Handle
{
public:
	HandleB(int fd = -1) :Handle(fd) {}
	~HandleB() { close(Handlefd); }
	int handle_event();
};


class HandleA : public Handle
{
public:
	HandleA(int fd = -1) :Handle(fd) {}
	~HandleA() { close(Handlefd); }
	int handle_event();
};



//事件多路分发器
class Demultiplexer
{
public:
	Demultiplexer():epfd(-1){}
	~Demultiplexer(){}

	void demultiplexer();

	void register_event(Handle *H);
	void remove_event(Handle *H);
	void start_demu(int fd, char* host, unsigned short port);

private:
	map<int, Handle*> mhandle;
	int epfd;
	struct epoll_event fds[MAXFD];
};

void Demultiplexer::start_demu(int fd, char* host, unsigned short port)
{
	epfd = epoll_create(MAXFD);

	struct sockaddr_in ser;
	memset(&ser, 0, sizeof(ser));

	ser.sin_family = AF_INET;
	ser.sin_port = htons(port);
	ser.sin_addr.s_addr = inet_addr(host);

	if (bind(fd, (struct sockaddr*)&ser, sizeof(ser)) == -1)
	{
		cout << "bind error" << endl;
		exit(0);
	}

	if (listen(fd, 20) == -1)
	{
		cout << "listen error" << endl;
		exit(0);
	}
}

void Demultiplexer::register_event(Handle *H)
{
	int fd = H->get_fd();

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = fd;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1)
	{
		perror("epoll ctl error");
	}

	int oldflg = fcntl(fd, F_GETFL);
	int newflg = oldflg | O_NONBLOCK;

	if (fcntl(fd, F_SETFL, newflg) == -1)
	{
		perror("fcntl error");
	}

	mhandle.insert(make_pair(fd,H));
}

void Demultiplexer::remove_event(Handle *H)
{
	if (epoll_ctl(epfd, EPOLL_CTL_DEL, H->get_fd(), NULL) == -1)
	{
		perror("epoll ctl error");
	}

	mhandle.erase(H->get_fd());
}







//反应堆
class Reactor
{
public:
	static Reactor* Instance();
	~Reactor();

	void handle_events();

	void register_handle(Handle *H);
	void remove_handle(Handle *H);

	void start(int fd, char* host, unsigned short port);
	
private:
	Reactor() {}
	static Reactor* _Instance;
	Demultiplexer Demu;
};
Reactor* Reactor::_Instance = 0;

static Reactor* R = Reactor::Instance();

 Reactor* Reactor::Instance()
{
	if (_Instance == 0)
	{
		_Instance = new Reactor;
	}

	return _Instance;
}
Reactor::~Reactor()
{
	delete _Instance;
}
void Reactor::start(int fd, char* host, unsigned short port)
{
	Demu.start_demu(fd, host, port);
}
void Reactor::register_handle(Handle *H)
{
	Demu.register_event(H);
}
void Reactor::remove_handle(Handle *H)
{
	Demu.remove_event(H);
}

void Reactor::handle_events()
{
	Demu.demultiplexer();
}

void Demultiplexer::demultiplexer()
{
	struct sockaddr_in caddr;
	while (true)
	{
		int n = epoll_wait(epfd, fds, MAXFD, 5000);
		if (n == -1)
		{
			cout << "epoll wait error" << endl;
			continue;
		}
		else if (n == 0)
		{
			continue;
		}
		else
		{
			for (int i = 0; i<n; ++i)
			{
				if (fds[i].events & EPOLLIN)//
				{
					map<int, Handle*>::iterator it = mhandle.find(fds[i].data.fd);

					if (it != mhandle.end())
					{
						int rec = (it->second)->handle_event();//回调函数
						if(rec == -1)
						{
							Handle *p = it->second;
							R->remove_handle(it->second);
							delete (p);//释放对象Handle
							
						}//
					}

				}
			}//for
		}//else
	}//while
}






int HandleA::handle_event()
{
	struct sockaddr_in caddr;
	socklen_t len = sizeof(caddr);
	int c = accept(Handlefd, (struct sockaddr*)&caddr, &len);

	if (c < 0)
	{
		return 0;
	}
	
    cout<<"client connected! ip:"<<inet_ntoa(caddr.sin_addr)<<" port:"<<
    ntohs(caddr.sin_port)<<endl;

	HandleB* H = new HandleB(c);
	R->register_handle(H);
	return 0;
}


int HandleB::handle_event()
{
	while (true)
	{
		char buff[128] = { 0 };
		int m = recv(Handlefd, buff, 128, 0);
		if (m == -1)
		{
			send(Handlefd, "ok", 2, 0);
			break;
		}
		else if (m == 0)
		{
			
			cout<<"over"<<endl;
			return -1;
		}
		else
		{
			cout<<"recv:   "<<buff<<endl;
		}
	}
	return 0;
}
int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		cout << "command is invalid! ./a.out ip port!" << endl;
		exit(0);
	}

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	{
		cout << "socket error" << endl;
		exit(0);
	}
	
	unsigned short port = atoi(argv[2]);
	R->start(fd, argv[1], port);
	
	HandleA* a = new HandleA(fd);


	
	R->register_handle(a);

	R->handle_events();//循环监听

	delete a;
	delete R;
	return 0;
}


