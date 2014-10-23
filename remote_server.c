#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

const int BUF_SIZE = 2048 * 10 ; 
const int MAX_CONCURRENT_CONNECTION = 50 ;
const int IP_LEN = 50 ;
const int HOSTNAME_LEN = 200 ; 

typedef enum{
	GET , POST , CONNECT  
}HTTP_REQUEST_TYPE;

int listen_port = 8099 ; 
int is_decode = 0 ;

void Log( char *buf ){
	time_t now_time ; 
	char *p,*p2 ; 
	char *str_time ;
	int len ; 

	now_time = time( NULL ) ;
	str_time = ctime( &now_time ) ;
	len = (int)strlen( str_time );
	*(str_time + len - 1 ) = 0 ;
	p = strstr( buf , "\r\n" ) ;
	if(p == NULL)
		p = strstr( buf , "\n" ) ;
	
	printf("[%s] -- \"" , str_time ) ;
	for( p2=buf;p2 !=p ; p2++ )	printf("%c" , *p2) ;
	printf("\"\n");
}

void Decode(char *ch ,int buf_size){
	int i ;
	for(i=0;i<buf_size;i++){
		ch[i] -= 1 ; 
	}
}
void Encode(char *ch ,int sz){
	int i ;
	for(i=0;i<sz;i++){
		ch[i] += 1 ; 
	}
}
int create_socket( int con_current_connect ){
	int s_socket ; 
	struct sockaddr_in s_add ;

	s_socket = socket( AF_INET, SOCK_STREAM , 0 );
	if( s_socket == -1 ){
		printf("socket initial failed!\n");
		return -1 ; 
	}
	bzero( &s_add , sizeof(struct sockaddr_in) ) ;
	s_add.sin_family = AF_INET ; 
	s_add.sin_addr.s_addr = htonl( INADDR_ANY ) ;
	s_add.sin_port = htons( listen_port ) ;

	if( bind(s_socket , (struct sockaddr *)(&s_add) , sizeof(struct sockaddr) ) == -1 ){
		printf("socket bind failed!\n"); 
		return -1 ; 
	}
	if( listen( s_socket,con_current_connect )==-1 ){
		printf("socket listen failed!\n") ;
		return -1 ; 
	}
	return s_socket ; 
}
 

int read_line( int c_socket , char *buf , char line_end_flag ){
	char ch[10] ;  
	char *p = buf ; 

	while(1){
		if( recv( c_socket , ch , 1 , 0 ) <= 0 ){
			*p = 0 ; return 1 ;
		}
		*p = ch[0] ;	p++;
		if( ch[0] == line_end_flag )	break ; 
	}
	*p = '\0' ;
	if( is_decode == 1 )
		Decode( buf , (int)strlen(buf) ) ; 
	return 0 ;
}

int check_http_type( char *line , HTTP_REQUEST_TYPE *r_type){
	if( strstr( line , "GET" ) != NULL ){
		*r_type = GET ; 
		return 0 ; 
	}
	else if( strstr( line , "POST" ) != NULL ){
		*r_type = POST ;
		return 0 ;
	}
	else if( strstr( line , "CONNECT" ) != NULL ){
		*r_type = CONNECT ; 
		return 0 ; 
	}
	else{
		return 1 ; 
	}
}

int buffer_check( char *base_p , int sz , char *line ){
	int line_len = (int)strlen( line ) ; 
	if( sz + line_len >= BUF_SIZE )
		return 1 ; 
	strncpy( base_p , line , line_len ) ; 
	return 0 ;
}

int read_header( int c_socket , char *buf , HTTP_REQUEST_TYPE *r_type){
	char *base_p = buf ; 
	int fill_size = 0 ; 
	char strline[ BUF_SIZE ] ; 
	int blank_line = 0 ;
	int sz ; 
	char line_end_flag = '\n' + (is_decode==1?1:0) ;

	if( read_line( c_socket , strline , line_end_flag ) ){
		return 1 ; 
	}
	if( check_http_type( strline , r_type ) ){
		printf("http header type is unknown!\n");
		return 1 ;
	} 
	if( buffer_check( base_p , fill_size  , strline ) ){
		printf("buffer is overflow!\n") ; 
		return 1 ; 
	}
	sz = (int)strlen( strline );
	base_p += sz ; fill_size += sz ; 
	
	while( 1 ){
		if( read_line( c_socket , strline , line_end_flag ) ){
			return 1 ; 
		}
		if( buffer_check( base_p , fill_size , strline) ){
			printf("buffer is overflow!\n") ; 
			return 1 ;
		}
		sz = (int)strlen( strline ) ;
		base_p += sz ; fill_size += sz ; 

		if( strcmp( strline , "\r\n" )==0 || strcmp( strline , "\n" )==0 ){
			blank_line = 1 ;
		}
		if( blank_line==1 && (*r_type == GET || *r_type == CONNECT) )	break ; 
		if( blank_line==1 && (*r_type == POST ) ){
			sz = recv( c_socket , strline , BUF_SIZE , 0 );
			strline[ sz ] = 0 ;
			if( is_decode == 1 )	Decode( strline , sz ) ; 
			if( buffer_check( base_p , fill_size , strline ) )
				return  1 ; 
			base_p += sz ; fill_size += sz ; 
			break ; 
		}
	}
	*base_p = 0 ;
	Log( buf ) ;
	return 0 ; 
}


int get_target_hostname(char *hostname ,char *ip , char *buf){
	char *p , *p2 , *p3 , *ph ; 
	struct hostent *h ; 
	
	p = strstr( buf , "Host" ) ;
	if( p == NULL ){
		printf("NO Host found!\n") ; 
		return 1 ;
	}
	p2 = strstr( p + 6 ,"\r" ) ; 
	if( p2 == NULL ){
		p2 = strstr( p + 6 , "\n" ) ; 
	}
	ph = hostname ; 
	for(p3 = p + 6 ; *p3 != ':' && p3 != p2 ; p3++,ph++ ){
		*ph = *p3 ; 
	}
	*ph = 0 ; 

	if( (h = gethostbyname(hostname) ) == NULL ){
		printf("can not conver hostname to ip address!\n");
		return 1 ; 
	}
	p = inet_ntoa(*((struct in_addr *)h->h_addr) ) ; 
	memcpy( ip , p , (int)strlen(p) );
	return 0 ;
}

void transfer_tunnel( int c_socket, int s_socket ){
	char buf[ BUF_SIZE ] ; 
	int buf_size ; 

	if( fork() == 0 ){
		strcpy( buf , "HTTP/1.1 200 Connection Established\r\n\r\n" ) ;
		buf_size = (int)strlen( buf ) ;
		if( is_decode == 1 ) 
			Encode( buf , buf_size ) ; 
		send( c_socket , buf , buf_size , 0 ) ; 
		while( 1 ){
			if( (buf_size = recv( s_socket , buf, BUF_SIZE , 0 ) ) <= 0 )	break ; 
			if( is_decode == 1 )
				Encode( buf , buf_size ) ;
			if( send( c_socket , buf , buf_size , 0 ) <= 0 )	break ;  
		}
		shutdown( c_socket , SHUT_RDWR ) ;
		close( c_socket ) ;
		shutdown( s_socket , SHUT_RDWR ) ;
		close( s_socket ) ;
		exit( 0 ) ; 
	}
	if( fork() == 0 ){
		while( 1 ){
			if( (buf_size = recv(c_socket , buf, BUF_SIZE , 0 )  ) <= 0 )	break ; 
			if( is_decode == 1 )
				Decode( buf , buf_size ) ; 
			if( send( s_socket , buf , buf_size , 0 ) <= 0 )	break ; 
		}
		shutdown( c_socket , SHUT_RDWR ) ;
		close( c_socket ) ;
		shutdown( s_socket , SHUT_RDWR ) ;
		close( s_socket ) ; 
		exit( 0 ) ;
	}
	close( c_socket ) ;
	close( s_socket ) ; 
}

int  transfer_request( int c_socket, char *buf , char *hostname , char *ip , int remote_port , HTTP_REQUEST_TYPE r_type ){
	int remote_socket ;
	struct sockaddr_in remote_addr ; 
	int sz , buf_size ;
	int i ;

	bzero( &remote_addr , sizeof(remote_addr ) )  ; 
	remote_addr.sin_family = AF_INET ;
	remote_addr.sin_addr.s_addr = inet_addr( ip ) ; 
	remote_addr.sin_port = htons( remote_port ) ; 

	if( (remote_socket = socket(AF_INET , SOCK_STREAM , 0) ) < 0 ){
		printf("get remote socket failed!\n");
		return 1 ;
	}
	if( connect( remote_socket , (struct sockaddr *)&remote_addr , sizeof(remote_addr) ) < 0 ){
		printf("connect remote socket failed!\n");
		return 1 ;
	}
	// for https
	if( r_type == CONNECT ){
		transfer_tunnel( c_socket, remote_socket ) ; 
		return 0 ;
	}

	// for http
	if( fork() == 0 ){
		buf_size = (int)strlen( buf ) ;
		sz = send( remote_socket , buf , buf_size , 0 ) ;
		while( 1 ){
			if( read_header( c_socket , buf , &r_type ) )
				break; 
			if( change_http_header( buf , r_type , &remote_port) )
				break ; 
			buf_size = (int)strlen( buf ) ;
			sz = send( remote_socket, buf , buf_size , 0 ) ;
			if( sz < 0 )	break ;
		}
		shutdown( c_socket , SHUT_RDWR ) ; 
		close( c_socket ) ;
		shutdown( remote_socket , SHUT_RDWR ) ; 
		close( remote_socket ) ; 
		exit(0) ; 
	}
	if( fork() == 0 ){
		while( (buf_size = recv( remote_socket , buf , BUF_SIZE ,0  ) ) > 0  ){
			buf[ buf_size ] = 0 ;
			if( is_decode == 1 )
				Encode( buf , buf_size ) ; 
			sz = send( c_socket , buf , buf_size , 0 ) ;
			if( sz < 0 )	break ; 
		}
		shutdown( remote_socket , SHUT_RDWR );
		close( remote_socket ) ; 
		shutdown( c_socket , SHUT_RDWR ) ;
		close( c_socket )  ;
		exit(0) ; 
	}
	close( remote_socket ) ; 
	close( c_socket ) ; 
	return 0;
}	

int change_http_header(char *ch , HTTP_REQUEST_TYPE r_type,int *remote_port ){
	char *p , *p2 , *p3 ;
	if( r_type == CONNECT ){
		p = strstr( ch , "CONNECT" ) ;
		p2 = strstr( p + 8 , ":" ) ;
		p = strstr( p2+1, " " ) ;
		*remote_port = 0 ; 
		for(p3=p2+1;p3!=p;p3++){
			*remote_port = (*remote_port)*10 + (*p3) - '0' ; 
		}
		return 0 ;	
	}
	if( r_type == GET ){
		p = strstr( ch ,"GET" ) ; 
		p += 4 ;
	}
	else{
		p = strstr( ch , "POST" ); 
		p += 5 ; 
	}

	p2 = strstr(p, "http://") ; 
	if(p2 == NULL){
		p2 = strstr(p,"/") ; 
	}
	else{
		p2 = strstr(p2+7 , "/") ; 
	}
	if(p2 == NULL ){
		printf("Can not find sub path in http url\n");
		return 1; 
	}
	for(; *p2!=0  ; p2++ ,p++ ){
		*p = *p2 ; 
	}
	*p = 0 ;
	*remote_port = 80 ; 
	return 0 ; 
}

int deal_with_request( int c_socket ){	
	char buf[ BUF_SIZE ] ; 
	char target_ip[ IP_LEN ] ; 
	char hostname[ HOSTNAME_LEN ] ; 
	int remote_port ; 
	int buf_size ;
	HTTP_REQUEST_TYPE request_type ; 

	if( read_header( c_socket , buf , &request_type ) )
		return 1 ;
	if( change_http_header( buf , request_type, &remote_port) )
		return 1 ; 
	if( get_target_hostname( hostname ,	target_ip , buf) )
		return 1; 
	if( transfer_request( c_socket , buf , hostname ,  target_ip , remote_port , request_type) )
		return 1 ;
	return 0 ; 
}

void sigchld_handler(int signal){
	// recycle resource
	while( waitpid(-1,NULL,WNOHANG) > 0 ) ; 
}

int usage(){
	printf("**************************************\n");
	printf("Listening port:	-p\n");
	printf("Decode/Encode:	-D\n");
	printf("**************************************\n");
}

int parse(int argc,char **argv){
	int i ; 
	for(i=1;i<argc;i++){
		if( argv[i][0] == '-' ){
			if( argv[i][1] == 'p' ){
				if( i+1 < argc ){
					sscanf( argv[++i] , "%d", &listen_port ) ; 
				}
				else
					return 1 ;  
			}
			else if( argv[i][1] == 'D' ){
				is_decode = 1 ;		
			}
			else
				return 1 ; 
		} 
		else 
			return 1 ; 
	}	
	return 0 ;
}

int main(int argc, char **argv ){
	int server_socket , c_socket ; 
	socklen_t client_size ; 
	int pid ;
	struct sockaddr_in c_add ;
	
	usage() ; 
	while( 1 ){
		if( parse( argc , argv ) ) 
			usage();
		else
			break ; 
	}
	printf("listening port is: %d\n" , listen_port );
	printf("decode/encode is: %s\n" , is_decode==1?"on":"off" );

	server_socket = create_socket( MAX_CONCURRENT_CONNECTION ) ;
	if( server_socket == -1 ){
		return 1 ; 
	}
	signal( SIGCHLD , sigchld_handler ) ; 

	while( 1 ){
		client_size = sizeof( c_add	 )  ;
		c_socket = accept( server_socket , (struct sockaddr *)(&c_add) , &client_size ) ; 
		if( c_socket == -1 ){
			printf("socket accept failed!\n");
			return -1 ; 
		}
		if( (pid = fork())==0 ){
			close( server_socket ) ; 
			deal_with_request( c_socket ) ; 
			exit(0) ; 
		}
		else if( pid > 0 ){
			close( c_socket ) ; 
		}
		else{
			printf("fork process error!\n");
			return 1 ; 
		}
	}

	return 0 ;
}
