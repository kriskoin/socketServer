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
#include <set>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sstream>
#include <thread> 
#include <chrono> 
#include <ctype.h>

typedef unsigned int IPADDRESS;	
typedef int 				SOCKET_DESCRIPTOR;
typedef struct sockaddr		SOCKADDR;
typedef struct sockaddr_in	SOCKADDR_IN;
typedef SOCKADDR *			LPSOCKADDR;

#define MAX_CONNECTIONS 25
#define PORT_NUMBER		7777
#define SOCKET_TTL 5000 // 5 seg before close an inactive socket

#define BUFFER_SIZE		5 // size of the read buffer for the recv function
                          // using small value to ensure the procotol works 
						  //properly  

//#define INVALID_SOCKET		(-1)
#define SOCKET_ERROR		(-1)
#define WSAEWOULDBLOCK		EWOULDBLOCK

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




typedef pthread_mutex_t	CRITICAL_SECTION;


void InitializeCriticalSection(CRITICAL_SECTION *crit_sec_ptr){
	*crit_sec_ptr = (CRITICAL_SECTION)PTHREAD_MUTEX_INITIALIZER;
};

unsigned int GetTickCount(void){
	struct timeval now;
	gettimeofday(&now, NULL);
	static unsigned int initial_ms_value;
	if (!initial_ms_value) {
		// We've never initialized the initial ms value. Do so now.
		initial_ms_value = (now.tv_sec * 1000) + (now.tv_usec / 1000);
	}
	return (now.tv_sec * 1000) + (now.tv_usec / 1000) - initial_ms_value;
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
		std::string msg;
		packet(std::string amsg){msg=amsg;};
		~packet()=default;
};

class client{
	public:
		unsigned int ttl;//time to live
		std::string ip;
		SOCKET_DESCRIPTOR socket; //sock descriptor
		bool connected = false;
		std::string msg;
		client(){InitializeCriticalSection(&requestsLock);};
		~client(){
			
			if(connected){
				close(socket);
			}
			//free memory
			EnterCriticalSection(&requestsLock);
			while (!requestsQueue.empty()) {
				delete(requestsQueue.front());
				requestsQueue.pop();
			}
			LeaveCriticalSection(&requestsLock);
		};

		void addRequest(std::string request){
			packet * p = new packet(request);
			EnterCriticalSection(&requestsLock);
			requestsQueue.push(p);
			LeaveCriticalSection(&requestsLock);
		}

		void addResponse(std::string response){
			packet * p = new packet(response);
			EnterCriticalSection(&responsesLock);
			responsesQueue.push(p);
			LeaveCriticalSection(&responsesLock);
		}

		packet * getResponse (){
			packet * p = NULL;
			EnterCriticalSection(&responsesLock);
			if(responsesQueue.size()){
				p = responsesQueue.front();
				responsesQueue.pop();
			}
			LeaveCriticalSection(&responsesLock);
			return p;
		}

		packet * getRequest (){
			packet * p = NULL;
			EnterCriticalSection(&requestsLock);
			if(requestsQueue.size()){
				p = requestsQueue.front();
				requestsQueue.pop();
			}
			LeaveCriticalSection(&requestsLock);
			return p;
		}


	private:
		std::queue< packet *> requestsQueue;
		std::queue< packet *> responsesQueue;
		CRITICAL_SECTION  requestsLock;
		CRITICAL_SECTION  responsesLock;
		int packetSize = 0;
	};

class socketServer{
	public:
	   
	    SOCKET_DESCRIPTOR sockfd; //sock descriptor
		SOCKADDR_IN my_addr;
		int mainThreadStatus;
		int readThreadStatus;
	    
	    int conections;//Num active conections
	
		//public functions
		socketServer(){
			InitializeCriticalSection(&lockStop);
		
			InitializeCriticalSection(&clientsLock);
			InitializeCriticalSection(&descriptorsLock);
			mainThreadStatus=THREAD_STOPED;
			
			// borra los conjuntos de descriptores
			// maestro y temporal
			FD_ZERO(&masterDesciptors);    
			FD_ZERO(&read_fds);
			maxDescriptor=-1;
			conections=0;
						
		};	

		~socketServer(){

			thAccepting.join();

			thReadRequests.join();

			thProcessRequests.join();
			Sleep(300);
			thProcessResponses.join();

			if(sockfd!=SOCKET_ERROR){
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
				if( this->conections < MAX_CONNECTIONS){
					acceptNewClients();
				}
				EnterCriticalSection(&lockStop);		   	
				exitFlag = this->stopFlag;
				LeaveCriticalSection(&lockStop);
				Sleep(1);
			} 
			while (!exitFlag);
			std::cout<<"Accept thread stoped !!"<<std::endl;
		};

		void start(int portNumber){
			int err=createListenSocket(portNumber);
			Sleep(5);
			if (err == ERR_NONE){
				//launch threads
				thAccepting = std::thread(&socketServer::acceptConnections,this);
				Sleep(300);                                 
				thReadRequests = std::thread(&socketServer::readIncommingRequests,this);
				Sleep(300);
				thProcessRequests =  std::thread(&socketServer::processClientRequests,this); 
				Sleep(300);
				thProcessResponses =  std::thread(&socketServer::processClientResponses,this); 
				
			}
		}

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
			
			if(newsock==SOCKET_ERROR){		
				int err = WSAGetLastError();				
				if (err == WSAEWOULDBLOCK || err == WSAECONNRESET) {
					// Nobody is ready to connect yet.
					return ERR_NONE;	// no error, but no connection either.
				}else{
					std::cout<<"Error on accepting new connections"<<std::endl;			  
					exit(1);	
				};//
			}	

			fcntl(newsock, F_SETFL, O_NONBLOCK);//non blocking socket
			//TODO: nagle stuff
			//bool true_bool = true;
			//int err = setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (char *)&true_bool, sizeof(true_bool));
			//if (err) {
			//	printf("WARNING setsockopt() to disable nagle failed  WSA error = %d.\n", WSAGetLastError());		
			//};//if

			newClient->socket = newsock;
			newClient->connected = true;
			char str[100];
		    sprintf(str, "%d.%d.%d.%d", (dest_addr.sin_addr.s_addr) & 255,(dest_addr.sin_addr.s_addr>>8) & 255,(dest_addr.sin_addr.s_addr>>16) & 255,(dest_addr.sin_addr.s_addr>>24) & 255);
			newClient->ip.assign(str);
			
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
					
			std::cout<<"New Connection accepted [ "<<newClient->ip<<" ]"<<std::endl;
			return ERR_NONE;
			
		};

        void processClientRequests(){
			bool exitFlag= false;
			packet * p;
			std::set<client *> clients;
			do{
				
				EnterCriticalSection(&clientsLock);
				if(clientsMap.size()){
					for (auto it: clientsMap) {
						if (it.second->connected)
						clients.insert(it.second);
					}
				}
				LeaveCriticalSection(&clientsLock);

				for (auto c: clients) {
					p = c->getRequest();
					if(p){
						// queued response
						if (p->msg.find("ping")!=std::string::npos ||
						    p->msg.find("PING")!=std::string::npos
						){
							std::string response = "S pong\n";
							c->addResponse(response);
						}

						if (p->msg.find("cat")!=std::string::npos ||
						    p->msg.find("CAT")!=std::string::npos
						){
							std::string response = findFile(p->msg);
							c->addResponse(response);
						}

						if (p->msg.find("sum")!=std::string::npos ||
						    p->msg.find("SUM")!=std::string::npos
						){
							std::string response = getSum(p->msg);
							c->addResponse(response);
						}
						
						delete(p);
					}
				}
				clients.clear();
				
				EnterCriticalSection(&lockStop);		   	
				exitFlag = this->stopFlag;
				LeaveCriticalSection(&lockStop);
				Sleep(50);
			} 
			while (!exitFlag);
			std::cout<<"process clients requests thread stoped !!"<<std::endl;
		};


		void processClientResponses(){
			bool exitFlag= false;
			packet * p;
			std::set<client *> clients;
			do{
				
				EnterCriticalSection(&clientsLock);
				if(clientsMap.size()){
					for (auto it: clientsMap) {
						if (it.second->connected)
						clients.insert(it.second);
					}
				}
				LeaveCriticalSection(&clientsLock);

				for (auto c: clients) {
					p = c->getResponse();
					if(p){
						if(p->msg.find("SENDFILE")!=std::string::npos){
							//send a file
							std::string fileName = p->msg.substr(p->msg.find(" ")+1);
							sendFile(c->socket,fileName);
						}else{					
							send(c->socket,p->msg .c_str(),p->msg.size(),MSG_CONFIRM);
							delete(p);
						}
					}
				}
				clients.clear();
				
				EnterCriticalSection(&lockStop);		   	
				exitFlag = this->stopFlag;
				LeaveCriticalSection(&lockStop);
				Sleep(50);
			} 
			while (!exitFlag);
			std::cout<<"process clients responses thread stoped !!"<<std::endl;
		};

		void readIncommingRequests(){
			std::set<client *> clients;
			int s;
			int tmpMaxDescrip;
			struct timeval tv;
			bool exitFlag= false;
			
			do{
				shutdownDiscClients();
				if(conections){		
					zstruct(tv);
					tv.tv_sec =0;
					tv.tv_usec = 125000;	// 125ms timeout					
					FD_ZERO(&read_fds);  
					EnterCriticalSection(&descriptorsLock);
					read_fds = masterDesciptors;
					tmpMaxDescrip=maxDescriptor+1;		  
					LeaveCriticalSection(&descriptorsLock);	
					//whichs sockets have something to read	??
					if(conections>0){  
						s=select(tmpMaxDescrip, &read_fds, NULL, NULL, &tv);
						if(s==-1){
							std::cout<<"Error on select  !!"<<std::endl;
							exit(1);
						};//if  
						clients = getReadableSockets(&read_fds);
						readIncommingSockets(&clients);
						clients.clear();
					};
				};
				
				EnterCriticalSection(&lockStop);		   	
				exitFlag = this->stopFlag;
				LeaveCriticalSection(&lockStop);
				//Sleep(1);
			} 
			while (!exitFlag);
			std::cout<<"read IN requests thread stoped !!"<<std::endl;
		};

		

	private:
		CRITICAL_SECTION  lockStop;
		CRITICAL_SECTION  clientsLock;
	    CRITICAL_SECTION  descriptorsLock;
		int maxDescriptor;//max descriptor number
	    fd_set masterDesciptors;   // master set of descriptors descriptores de fichero
        fd_set read_fds; // conjunto temporal de descriptores de fichero para select()

		std::thread thAccepting;
		std::thread thReadRequests;
		std::thread thProcessRequests;
		std::thread thProcessResponses;

		bool stopFlag = false;

		std::map<SOCKET_DESCRIPTOR,client *> clientsMap;

		void sendFile (SOCKET_DESCRIPTOR s,std::string fileName){
			FILE * file_to_send;
			int ch;
			char toSEND[1];
			file_to_send = fopen (fileName.c_str(),"r");
			fseek (file_to_send, 0, SEEK_END);     
    		rewind(file_to_send);
			toSEND[0]='S';
			send(s, toSEND, 1, 0);
			toSEND[0]=' ';
			send(s, toSEND, 1, 0);
			while((ch=getc(file_to_send))!=EOF){
				toSEND[0] = ch;
				send(s, toSEND, 1, 0);
			}
			fclose(file_to_send);
			toSEND[0]='\n';
			send(s, toSEND, 1, 0);
		};

		ErrorType createListenSocket(int portNumber){
			   
			sockfd = socket(AF_INET, SOCK_STREAM, 0); // ¡Comprobar errores!
			if(sockfd==SOCKET_ERROR){	
				std::cout<<"Error creating socket"<<std::endl;
				exit(1);
			}
			

			fcntl(sockfd, F_SETFL, O_NONBLOCK);//non blocking socket
			my_addr.sin_family = AF_INET;         // Ordenación de máquina
			my_addr.sin_port = htons(portNumber);     // short, Ordenación de la red
			my_addr.sin_addr.s_addr = INADDR_ANY; // Rellenar con mi dirección IP
			memset(&(my_addr.sin_zero), '\0', 8); //

			if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))==SOCKET_ERROR){				
				std::cout<< "Error on binding"<<std::endl;
				exit(1);
			};
			
			if(listen(sockfd, 10)==SOCKET_ERROR){
				std::cout<< "Error on listing"<<std::endl;
				exit(1);
			};//if
			
			std::cout<<" Listing on port: "<< portNumber <<std::endl;
			return ERR_NONE;
		};//createListenSocket


	inline std::set<client *> getReadableSockets(fd_set *read_fds){
		std::set<client *> clients;
		EnterCriticalSection(&clientsLock);
		for (auto x: clientsMap) {
			if(FD_ISSET(x.first,read_fds) && x.second->connected){		
				clients.insert(x.second);
			}
		}
		LeaveCriticalSection(&clientsLock);  
		return clients;
	};// 

    //remove disconnected clients
	inline void shutdownDiscClients (){
		
		EnterCriticalSection(&clientsLock);
		EnterCriticalSection(&descriptorsLock);
		auto it = clientsMap.cbegin();
		while (it != clientsMap.cend())
		{
			if(!it->second->connected){		
				std::cout<<"[ "<< it->second->ip<<" ] disconnected! "<<std::endl;	
				FD_CLR(it->first, &masterDesciptors); 		
				close(it->second->socket);        
				delete (it->second);
				it = clientsMap.erase(it);
				conections--;
			}else{
				++it;
			}
		}
		LeaveCriticalSection(&descriptorsLock);
		LeaveCriticalSection(&clientsLock);	
		
	};//

	inline void readIncommingSockets(std::set<client *> * clients){
		char buffer[BUFFER_SIZE];	
		int received;
		for (auto c: *clients) {
			received = 0;
			memset(buffer ,0 , BUFFER_SIZE);
			received =  recv(c->socket , buffer , BUFFER_SIZE , 0);
			switch (received){
				case -1: 
					//error
					// TODO:  get futher to handle this 
					/*
						WSAENOTCONN 
						WSAECONNRESET 
						WSAECONNABORTED 
						WSAEHOSTUNREACH 
						WSAEHOSTDOWN 
						EPIPE 
						ETIMEDOUT 
						WSAESHUTDOWN
			 		*/

					break;
				case 0: 
						//connection closed
						// handle lost connection
						if (!c->ttl){
							c->ttl=GetTickCount()+SOCKET_TTL;
						}else{
							if(GetTickCount()>c->ttl){
								c->connected =false;				
							};
						};
					break;
				
				default:
						//we get data
						c->msg.append(buffer,received);
						c->ttl=0;
						if(buffer[received-1]=='\n'){
							if(
								c->msg.find("ping")!=std::string::npos ||
								c->msg.find("PING")!=std::string::npos ||
								c->msg.find("cat")!=std::string::npos ||
								c->msg.find("CAT")!=std::string::npos ||
								c->msg.find("sum")!=std::string::npos ||
								c->msg.find("SUM")!=std::string::npos 
							){						
								c->msg.pop_back();
								c->addRequest(c->msg);
							}						
							c->msg.clear();
						}
					break;
			}
		}
	}

	void sendData(SOCKET_DESCRIPTOR s,char *buf, int *len){
		int total = 0;        
		int bytesleft = *len;
		int n;
		n=0;
		while(total < *len) {
			n = send(s, buf+total, bytesleft, 0);
			if (n == -1) { break; }
			total += n;
			bytesleft -= n;
		}		
	};

	std::string findFile (std::string request) {
		std::string result = "E file dont exits\n";
		std::string fileName = request.substr(request.find(" ")+1);
		FILE * pFile;
		pFile = fopen (fileName.c_str(),"r");
		if (pFile!=NULL)
		{
			//just confirm that file exits
			//it 'll transmit with output thread
			fclose (pFile);
			result="SENDFILE ";
			result.append(fileName);
			return result;
		}else{
			result="E cannot access ";
			result.append(fileName);
			result.append("\n");
			return result;
		}
		
		return result;
	};


	std::string getSum (std::string request) {
		std::string result="S ";
		std::vector<std::string> v; 
		std::stringstream ss(request); 
		double val;
		double total = 0;
		while (ss.good()) { 
			std::string substr; 
			getline(ss, substr, ' '); 
			v.push_back(substr); 
		} 
	
		for (size_t i = 0; i < v.size(); i++){ 
			if (!(v[i].compare("SUM")==0) && !(v[i].compare("sum")==0)  ){
				if ( v[i].size()!= 0){
					try {
						val = std::stod(v[i]);
					}
					catch (std::invalid_argument& e) {						
						result = "E invalid_argument\n";
						return result;
					}
					catch (std::out_of_range& e) {						
						result = "E out_range\n";
						return result;
					}
					catch (...) {
						result = "E other_error\n";
						return result;
					}					
					total +=  val;
				}
			}
		}
        result.append(std::to_string(total));
		result.append("\n");
		return result;
	};


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

	unsigned int echoTTL = GetTickCount()+30000 ;//30 seg
    socketServer server;
	server.start(PORT_NUMBER);
    
	while(!Terminate){				
       // printf("Main thread.\n");
	    if(echoTTL < GetTickCount()){
			echoTTL = GetTickCount()+30000 ;
			std::cout<<std::endl<<server.conections<<" active connections, 'pkill server' to terminate!"<<std::endl;
		}
		Sleep(5);
	};//while
	
	server.stop();
	std::cout<<"Bye!"<<std::endl;
	return ERR_NONE;
}
