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
#include <netinet/in.h>
#include <fcntl.h>

typedef unsigned int IPADDRESS;	
typedef int 				SOCKET_DESCRIPTOR;
typedef struct sockaddr		SOCKADDR;
typedef struct sockaddr_in	SOCKADDR_IN;
typedef SOCKADDR *			LPSOCKADDR;

#define INVALID_SOCKET		(-1)
#define SOCKET_ERROR		(-1)
#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAENOTCONN			ENOTCONN
#define EnterCriticalSection(crit_sec_ptr)	pthread_mutex_lock(crit_sec_ptr)
#define TryEnterCriticalSection(crit_sec_ptr)	(!pthread_mutex_trylock(crit_sec_ptr))
#define LeaveCriticalSection(crit_sec_ptr)	pthread_mutex_unlock(crit_sec_ptr)
#define DeleteCriticalSection(crit_sec_ptr)	pthread_mutex_destroy(crit_sec_ptr)
#define Sleep(a) usleep((a)*1000)

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




bool Terminate = false;

void OurSignalHandlerRoutine(int signal, struct sigcontext sc)
{
	switch (signal) {
	case SIGTERM:
		Terminate=true;
		printf("SIGTERM proccesed ! .\n");
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
		client()=default;
		~client(){
			//free memory
			while (!inPackets.empty()) {
				delete(inPackets.front());
				inPackets.pop();
			}
		};
	private:
		SOCKET_DESCRIPTOR sockfd; //sock descriptor
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
			std::cout<<"Hola gato"<<std::endl;
			InitializeCriticalSection(&broadcastLock);
			InitializeCriticalSection(&socketsLock);
			InitializeCriticalSection(&clientsLock);
			InitializeCriticalSection(&descriptorsLock);
			mainThreadStatus=THREAD_STOPED;
			
			// borra los conjuntos de descriptores
			// maestro y temporal
			FD_ZERO(&master);    
			FD_ZERO(&read_fds);
			maxDescriptor=-1;
			conections=0;
		};	

		~socketServer(){
			if(sockfd!=INVALID_SOCKET){
				close(sockfd);
			};
		}

		ErrorType createListenSocket(int portNumber){
			   
			sockfd = socket(AF_INET, SOCK_STREAM, 0); // ¡Comprobar errores!
			if(sockfd==INVALID_SOCKET){
				perror("socket .\n");
				exit(1);
			}else{
				printf("socket %d was created.\n",sockfd);
			};
			
			fcntl(sockfd, F_SETFL, O_NONBLOCK);//non blocking socket

			my_addr.sin_family = AF_INET;         // Ordenación de máquina
			my_addr.sin_port = htons(portNumber);     // short, Ordenación de la red
			my_addr.sin_addr.s_addr = INADDR_ANY; // Rellenar con mi dirección IP
			memset(&(my_addr.sin_zero), '\0', 8); // Poner a cero el resto de la estructura

			if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))==INVALID_SOCKET){
				perror("Bind.\n");
				return ERR_ERROR;
			};
			
			if(listen(sockfd, 10)==INVALID_SOCKET){
				perror("Listen.\n");
				return ERR_ERROR;
			};//if
			
		
			return ERR_NONE;
		};//createListenSocket

	private:
		CRITICAL_SECTION  broadcastLock;
		CRITICAL_SECTION  clientsLock;
		CRITICAL_SECTION  socketsLock;
	    CRITICAL_SECTION  descriptorsLock;
		int maxDescriptor;//maximo descriptor para la funcion select
	    fd_set master;   // conjunto maestro de descriptores de fichero
        fd_set read_fds; // conjunto temporal de descriptores de fichero para select()

	  /*  ~socketServer();
		ErrorType createListenSocket(int portNumber);
		void closeSocket();
	    
	  
	    void mainThread();
		void readThread();
		
		ErrorType startService(int port);
		void stopService();
		void stopReadThread();
		void pauseService();
		
	
		
	    void addClient(TRANSFERCLIENT *client);
		void addSocket(CSOCKET* sock);
		void removeSocket(CSOCKET* sock);

		
		
		
		void sendBuffer(char* b,int buffSize,SOCKET_DESCRIPTOR source);//test
		
	private:
		
	    
	   
		BROADCASTLIST broadcastList;
		SOCKETLIST socketList;
		CLIENTLIST clientList;
	    SOCKETLISTITERATOR socketListIterator;
		CLIENTLISTITERATOR clientListIterator;
		
	
	    //private methods

		//main thread
		ErrorType startMainThread();
		void startReadThread();
		
		void stopMainThread();

		void processClientList();
	    
	    inline void processSocket(CSOCKET* s);
		inline void processSocket2(CSOCKET* s);
		//I/O
	    void processSocketsOutputList();
		void processSocketInputList();
		void processBroadcastMessages();
		void addBroadcastMessage(CPACKET *packet,CSOCKET *source);
		//I
		inline void processSocketsInputRequest(fd_set *read_fds);
		//O
		inline void processSocketOutputQueue(CSOCKET *s);

		//new conections
		ErrorType acceptConnection(SOCKET_DESCRIPTOR sockServer);

	    #if USE_SSL
		 inline ErrorType  SSL_readSocket(CSOCKET* s,char * buff,int bufSize,int* realBytesRead);
		 inline ErrorType SSL_sendSocket(SSL *ssl,char *buff,int buffSize);
        #else
		 inline ErrorType readSocket(CSOCKET* s,char * buff,int bufSize,int* realBytesRead);
		 inline ErrorType sendSocket(SOCKET_DESCRIPTOR sock,char *buff,int buffSize);
        #endif

		void shutDownClient(TRANSFERCLIENT *client);
		void removeClient(TRANSFERCLIENT *client);

		void handleLostConecction(CSOCKET* s);
			    		    
	    void cleanSocketList();
		void cleanClientList();
		ErrorType readPacket(CSOCKET* s);
	    */
	    
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
	std::cout<<"Bye!"<<std::endl;
	return 0;
}