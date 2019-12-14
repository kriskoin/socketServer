#include <pthread.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>     
#include <stdlib.h>     
#include <iostream> 

typedef unsigned int IPADDRESS;	

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

//EnterCriticalSection(&socketsLock);
//LeaveCriticalSection(&socketsLock);

typedef pthread_mutex_t	CRITICAL_SECTION;
typedef pthread_mutex_t	LOCK;

void InitializeCriticalSection(CRITICAL_SECTION *crit_sec_ptr){
	*crit_sec_ptr = (CRITICAL_SECTION)PTHREAD_MUTEX_INITIALIZER;
};

#define EnterCriticalSection(crit_sec_ptr)	pthread_mutex_lock(crit_sec_ptr)
#define TryEnterCriticalSection(crit_sec_ptr)	(!pthread_mutex_trylock(crit_sec_ptr))
#define LeaveCriticalSection(crit_sec_ptr)	pthread_mutex_unlock(crit_sec_ptr)
#define DeleteCriticalSection(crit_sec_ptr)	pthread_mutex_destroy(crit_sec_ptr)
#define Sleep(a) usleep((a)*1000)


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



class socketServer{
	public:
		//public variables
	    //CDebugLog *log;
		//CDebugLog *sockLog;
	    //SOCKET_DESCRIPTOR sockfd;
        //SOCKADDR_IN my_addr;
		int mainThreadStatus;
		int readThreadStatus;
	    
	    int conections;//Num active conections
	
	    
	
		//public functions
		socketServer(){
			std::cout<<"Hola gato"<<std::endl;
		};		
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
		
	    
	    int maxDescriptor;//maximo descriptor para la funcion select
	    fd_set master;   // conjunto maestro de descriptores de fichero
        fd_set read_fds; // conjunto temporal de descriptores de fichero para select()
		BROADCASTLIST broadcastList;
		SOCKETLIST socketList;
		CLIENTLIST clientList;
	    SOCKETLISTITERATOR socketListIterator;
		CLIENTLISTITERATOR clientListIterator;
		CRITICAL_SECTION  broadcastLock;
		CRITICAL_SECTION  clientsLock;
		CRITICAL_SECTION  socketsLock;
	    CRITICAL_SECTION  descriptorsLock;
	
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