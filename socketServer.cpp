#include <pthread.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>     
#include <stdlib.h>     
#include <iostream> 
#include <string.h>
#include <queue>  
#include <map> 
#include <netinet/in.h>
#include <fcntl.h>

#include <thread> 
#include <chrono> 

typedef unsigned int IPADDRESS;	
typedef int 				SOCKET_DESCRIPTOR;
typedef struct sockaddr		SOCKADDR;
typedef struct sockaddr_in	SOCKADDR_IN;
typedef SOCKADDR *			LPSOCKADDR;

#define BUFFER_SIZE		10 // size of the read buffer for recv function
#define PORT_NUMBER		7777
#define INVALID_SOCKET		(-1)
#define SOCKET_ERROR		(-1)
#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAENOTCONN			ENOTCONN
#define EnterCriticalSection(crit_sec_ptr)	pthread_mutex_lock(crit_sec_ptr)
#define TryEnterCriticalSection(crit_sec_ptr)	(!pthread_mutex_trylock(crit_sec_ptr))
#define LeaveCriticalSection(crit_sec_ptr)	pthread_mutex_unlock(crit_sec_ptr)
#define DeleteCriticalSection(crit_sec_ptr)	pthread_mutex_destroy(crit_sec_ptr)
#define Sleep(a) usleep((a)*1000)
#define zstruct(a)	memset(&(a),0,sizeof(a))

#define WSAECONNRESET		ECONNRESET
#define WSAECONNABORTED		ECONNABORTED
#define WSAESHUTDOWN		ESHUTDOWN
#define WSAEAGAIN			EAGAIN
#define WSAEHOSTUNREACH		EHOSTUNREACH
#define WSAEHOSTDOWN		EHOSTDOWN

enum ErrorType {	ERR_NONE,				// Success - No error occurred.
					ERR_MINOR_NOTE,
					ERR_NOTE,
					ERR_MINOR_WARNING,
					ERR_WARNING,
					ERR_SERIOUS_WARNING,
					ERR_MINOR_ERROR,
					ERR_ERROR,
					ERR_INTERNAL_ERROR,
					ERR_FATAL_ERROR } ;


enum THREAD_STATUS{ THREAD_STARTED,
					THREAD_STOPED,
					THREAD_EXIT	};


//EnterCriticalSection(&socketsLock);
//LeaveCriticalSection(&socketsLock);

typedef pthread_mutex_t	CRITICAL_SECTION;


void InitializeCriticalSection(CRITICAL_SECTION *crit_sec_ptr){
	*crit_sec_ptr = (CRITICAL_SECTION)PTHREAD_MUTEX_INITIALIZER;
};


inline int WSAGetLastError(void)  {return errno;}


bool Terminate = false;

void OurSignalHandlerRoutine(int signal, struct sigcontext sc)
{
	switch (signal) {
	case SIGTERM:
		Terminate=true;
		std::cout<<"SIGTERM proccesed !"<<std::endl;
		break;
	case SIGHUP:

		break;
	case SIGINT:	// usually ctrl+C

		break;
	case SIGPIPE:

		break;
	case SIGQUIT:

		break;
	case SIGBUS:
		exit(10);
		break;
	case SIGFPE:
		 exit(10);
		break;
	case SIGSEGV:
		exit(10);
		break;

	case SIGALRM:

		break;
	default:
		//Error(ERR_ERROR, "%s(%d) Unknown control signal received from OS (%d).",_FL,signal);
		break;
	};//switch
};//OurSignalHandlerRoutine

class packet{
	public:
		packet()=default;
		~packet()=default;
};

class client{
	public:
		SOCKET_DESCRIPTOR socket; //sock descriptor
		bool conectedFlag = false;
		client()=default;
		~client(){
			//close socket
			close(socket);
			//free memory
			while (!inPackets.empty()) {
				delete(inPackets.front());
				inPackets.pop();
			}
		};

	private:
		std::queue< packet *> inPackets;
};

class socketServer{
	public:
		//public variables
	   
	    SOCKET_DESCRIPTOR sockfd; //sock descriptor
        SOCKADDR_IN my_addr;
		int mainThreadStatus;
		int readThreadStatus;
	    
	    int conections;//Num active conections
	
	    
	
		//public functions
		socketServer(){
			//std::cout<<"Hola gato"<<std::endl;
			InitializeCriticalSection(&lockStop);
			InitializeCriticalSection(&socketsLock);
			InitializeCriticalSection(&clientsLock);
			InitializeCriticalSection(&descriptorsLock);
			mainThreadStatus=THREAD_STOPED;
			
			// borra los conjuntos de descriptores
			// maestro y temporal
			FD_ZERO(&masterDesciptors);    
			FD_ZERO(&read_fds);
			maxDescriptor=-1;
			conections=0;
			
			int err=createListenSocket(PORT_NUMBER);
			Sleep(5);
			if (err == ERR_NONE){
				//launch threads
				thAccepting = std::thread(&socketServer::acceptConnections,this);

				thClientsRequests = std::thread(&socketServer::processClientsRequests,this);

				  
			}
		};	

		~socketServer(){

			thAccepting.join();

			thClientsRequests.join();

			if(sockfd!=INVALID_SOCKET){
				close(sockfd);
				std::cout<<"Socket: "<< sockfd<<" closed"<<std::endl;
			};
			

			//clean all open client connections
			EnterCriticalSection(&clientsLock);
			EnterCriticalSection(&descriptorsLock);	

			for (auto x: clientsMap) {
				FD_CLR(x.second->socket, &masterDesciptors); 
		        conections--;
				delete(x.second);
			}
			clientsMap.clear();
			
			LeaveCriticalSection(&descriptorsLock);
			LeaveCriticalSection(&clientsLock);
			

		
			std::cout<<"Socket server destructor called"<<std::endl;
		}

		
		void  acceptConnections(){
			bool exitFlag= false;
			
			do{
				acceptNewClients();
				EnterCriticalSection(&lockStop);		   	
				exitFlag = this->stopFlag;
				LeaveCriticalSection(&lockStop);
				Sleep(1);
			} 
			while (!exitFlag);
			std::cout<<"Accept thread stoped !!"<<std::endl;
		};

		void stop(){
			EnterCriticalSection(&lockStop);		   	
				this->stopFlag = true;
			LeaveCriticalSection(&lockStop);
		};

		//NEW CONECTIONS
		ErrorType  acceptNewClients(){
	        client * newClient;
			SOCKET_DESCRIPTOR newsock;
			SOCKADDR_IN dest_addr;
			int size;
			size=sizeof(SOCKADDR_IN);

			newClient = new client();
			if(!newClient){
				std::cout<<"Cannot create a client object"<<std::endl;
				exit(1);
			};//

			newsock=accept(sockfd, (sockaddr *)&dest_addr, (unsigned *)&size);
			
			if(newsock==INVALID_SOCKET){		
				int err = WSAGetLastError();				
				if (err == WSAEWOULDBLOCK || err == WSAECONNRESET) {
					// Nobody is ready to connect yet.
					return ERR_NONE;	// no error, but no connection either.
				}else{
					std::cout<<"Error on accepting new connections"<<std::endl;			  
					exit(1);	
				};//if(err==WSAEWOULDBLOCK || err==WSAECONNRESET)
			}	

			fcntl(newsock, F_SETFL, O_NONBLOCK);//non blocking socket
			//TODO: nagle stuff
			//bool true_bool = true;
			//int err = setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (char *)&true_bool, sizeof(true_bool));
			//if (err) {
			//	printf("WARNING setsockopt() to disable nagle failed  WSA error = %d.\n", WSAGetLastError());		
			//};//if

			newClient->socket = newsock;
			newClient->conectedFlag = true;
			
			EnterCriticalSection(&clientsLock);
			EnterCriticalSection(&descriptorsLock);	

			//add socket to the master descriptors set
			FD_SET(newClient->socket, &masterDesciptors); 			
			//actualiza el maximo descriptor
			if(newsock>maxDescriptor){
				maxDescriptor=newsock;
			};
			conections++;//increment the number of conections
			clientsMap.insert(std::pair<SOCKET_DESCRIPTOR,client *>(newClient->socket,newClient));
		
			LeaveCriticalSection(&descriptorsLock);
			LeaveCriticalSection(&clientsLock);
					
			std::cout<<"New Connection detected"<<std::endl;
			return ERR_NONE;
			
		};//AcceptConnection


		void processClientsRequests(){
			int s;
			int tmpMaxDescrip;
			struct timeval tv;
			bool exitFlag= false;
			
			do{

				if(conections){		
					zstruct(tv);
					tv.tv_sec =0;
					//tv.tv_usec =0.2;
					tv.tv_usec = 125000;	// 125ms timeout
					//tv.tv_usec = 200;	// 125ms timeout
					FD_ZERO(&read_fds);  
					EnterCriticalSection(&descriptorsLock);
					read_fds = masterDesciptors;
					tmpMaxDescrip=maxDescriptor+1;		  
					LeaveCriticalSection(&descriptorsLock);	
					//whichs sockets have something to read	??
					if(conections>0){   //conections++
						s=select(tmpMaxDescrip, &read_fds, NULL, NULL, &tv);
						if(s==-1){
							std::cout<<"Error on select  !!"<<std::endl;
							exit(1);
						};//if  
						processSocketsInputRequest(&read_fds);
					};
				};
				
				EnterCriticalSection(&lockStop);		   	
				exitFlag = this->stopFlag;
				LeaveCriticalSection(&lockStop);
				//Sleep(1);
			} 
			while (!exitFlag);
			std::cout<<"read clients requests thread stoped !!"<<std::endl;
		};

		

	private:
		CRITICAL_SECTION  lockStop;
		CRITICAL_SECTION  clientsLock;
		CRITICAL_SECTION  socketsLock;
	    CRITICAL_SECTION  descriptorsLock;
		int maxDescriptor;//max descriptor number
	    fd_set masterDesciptors;   // master set of descriptors descriptores de fichero
        fd_set read_fds; // conjunto temporal de descriptores de fichero para select()

		std::thread thAccepting;
		std::thread thClientsRequests;

		bool stopFlag = false;

		std::map<SOCKET_DESCRIPTOR,client *> clientsMap;

		ErrorType createListenSocket(int portNumber){
			   
			sockfd = socket(AF_INET, SOCK_STREAM, 0); // ¡Comprobar errores!
			if(sockfd==INVALID_SOCKET){	
				std::cout<<"Error creating socket"<<std::endl;
				exit(1);
			}
			std::cout<<"Socket "<<sockfd<<" created" <<std::endl;

			fcntl(sockfd, F_SETFL, O_NONBLOCK);//non blocking socket
			my_addr.sin_family = AF_INET;         // Ordenación de máquina
			my_addr.sin_port = htons(portNumber);     // short, Ordenación de la red
			my_addr.sin_addr.s_addr = INADDR_ANY; // Rellenar con mi dirección IP
			memset(&(my_addr.sin_zero), '\0', 8); //

			if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))==INVALID_SOCKET){				
				std::cout<< "Error on binding"<<std::endl;
				exit(1);
			};
			
			if(listen(sockfd, 10)==INVALID_SOCKET){
				std::cout<< "Error on listing"<<std::endl;
				exit(1);
			};//if
			
			std::cout<<"Socket "<<sockfd<<" listing on port: "<< portNumber <<std::endl;
			return ERR_NONE;
		};//createListenSocket


	inline void processSocketsInputRequest(fd_set *read_fds){
		// int i;
		//CSOCKET* s;	
		//int p;
		//p=0;
		//printf("processSocketsInputRequest\n");
		//socketListIterator=socketList.begin();
		//printf("EnterCriticalSection processSocketsInputRequest .\n");
		EnterCriticalSection(&clientsLock);
		for (auto x: clientsMap) {
	
			if(FD_ISSET(x.first,read_fds)){		
				std::cout<<"Socket "<<x.first<<" says something "<<std::endl;
			}
		}
		/*
		for(int i=0;i<(int)socketList.size();i++){
			s=socketList.at(i);
			//if((FD_ISSET(s->socket,read_fds))&&(!s->ttl)){		 		 
			if(FD_ISSET(s->socket,read_fds)){		 		 
				#if USE_STRUCT
				//log->AddLog ("Before readPacket \n");
				//processSocket2(s);
				readPacket(s);
				//log->AddLog ("After readPacket \n");
				#else
				//log->AddLog ("Before processSocket \n");
				//processSocket(s);
				//log->AddLog ("After processSocket \n");
				#endif
				p++;		
			}; //if(FD_ISSET(s->socket,read_fds))
			socketListIterator++;
		};//for
		*/
		LeaveCriticalSection(&clientsLock);  
		//printf("LeaveCriticalSection processSocketsInputRequest.\n");
		//printf("%d Sockets procesed.\n",p);
		//log->AddLog("%d Sockets procesed.\n",p);
	};// processSocketsInputRequest
	    
};




int main(){
	signal(SIGHUP,  (void (*)(int))OurSignalHandlerRoutine);
	signal(SIGTERM, (void (*)(int))OurSignalHandlerRoutine);
	signal(SIGINT,  (void (*)(int))OurSignalHandlerRoutine);
	signal(SIGSEGV, (void (*)(int))OurSignalHandlerRoutine);
	signal(SIGBUS,  (void (*)(int))OurSignalHandlerRoutine);
	signal(SIGFPE,  (void (*)(int))OurSignalHandlerRoutine);
	signal(SIGQUIT, (void (*)(int))OurSignalHandlerRoutine);
	signal(SIGALRM, (void (*)(int))OurSignalHandlerRoutine);
	signal(SIGPIPE, SIG_IGN);	// ignore broken pipe signals (socket disconnects cause SIGPIPE)

    socketServer server;

	while(!Terminate){				
       // printf("Main thread.\n");
		Sleep(5);
	};//while
	server.stop();
	std::cout<<"Bye!"<<std::endl;
	return ERR_NONE;
}
