#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>
const int BUF_SIZE = 1024 * 10 ;

int local_listen_port =  8090 ; 
int remote_port = 8099 ; 
char remote_ip[100] = "10.21.2.181" ; 
int is_decode = 0 ; 
int ip_version = 0 ; 

typedef enum{
	GET , POST , CONNECT  
}HTTP_REQUEST_TYPE;

void useage(){
	printf("./local_listener useage\n");
	printf("\t-p\t\tlocal listen port\n");
	printf("\t-H4\t\tremote server ip-v4\n");
	printf("\t-H6\t\tremote server ip-v6\n");
	printf("\t-P\t\tremote server port\n");
	printf("\t-D\t\tDecode/Encode transfer data\n");
}

void Log( char *buf ) {
	time_t now_time ; 
	char *time_str ;

	now_time = time( NULL ) ;
	time_str = ctime( &now_time ) ;
	*(time_str + (int)strlen(time_str) - 1) = 0 ;
	printf("[%s] -- \"",time_str );
	for( ; *buf != '\r' && *buf != '\n' ; buf++ )
		printf("%c", *buf) ;
	printf("\"\n") ;
}

int parse_arg( int argc ,char **argv ){
	int i ;
	for( i=1 ; i<argc ; i++ ){
		if( argv[i][0] == '-' ){
			if( argv[i][1] == 'p' ){
				sscanf( argv[++i] , "%d" , &local_listen_port ) ; 
			}
			else if( argv[i][1] == 'P' ){
				sscanf( argv[++i] , "%d" , &remote_port ) ; 
			}
			else if( argv[i][1] == 'H' ){
				if( argv[i][2] == '4' )
					strcpy( remote_ip , argv[++i] ) ;	
				else if( argv[i][2] == '6' ){
					strcpy( remote_ip , argv[++i] ) ;
					ip_version = 1 ;
				}
				else{
					useage() ;
					return 1 ; 
				} 
			}
			else if( argv[i][1] == 'D' ){
				is_decode = 1 ; 
			}
			else{
				useage() ;
				return 1 ;
			}
		}
		else{
			useage() ;
			return 1 ; 
		}
	}
	return 0 ; 
}

int create_listen_socket(){
	int c_socket ;
	struct sockaddr_in c_sin ; 

	if( (c_socket=socket( AF_INET , SOCK_STREAM , 0 ) ) < 0 ){
		printf("create listen socket socket function error!\n");
		return -1 ; 
	}  
	memset( &c_sin , 0 , sizeof(c_sin) ) ; 
	c_sin.sin_family = AF_INET ; 
	c_sin.sin_addr.s_addr = htonl( INADDR_ANY ) ; 
	c_sin.sin_port = htons( local_listen_port ) ;

	if( bind(c_socket, (struct sockaddr*)(&c_sin) , sizeof( c_sin ) ) < 0 ){
		printf("create listen socket bind error!\n");
		return -1; 
	}
	if( listen( c_socket , 30 ) < 0 ){
		printf("create listen socket listen error!\n");
		return -1 ;
	}
	return c_socket ; 
}

void hint(){
	printf("************************************\n") ;
	printf("local listen port: %d\n" , local_listen_port );
	printf("remote server ip: %s\n" , remote_ip );
	printf("remote server port: %d\n" ,remote_port  );
	printf("decode/encode is: %s\n" , is_decode==1?"on":"off" ) ; 
	printf("************************************\n") ;
	printf("local port is listening....\n");
}

int create_server_socket(){
	int s_socket ; 
	struct sockaddr_in s_add ; 
	struct sockaddr_in6 s6_add ; 

	if( ip_version == 1 ){
		// ip_v6 
		if( (s_socket = socket( AF_INET6, SOCK_STREAM , 0 ) ) < 0 ){
			printf("create server socket error!\n");
			return -1 ; 
		}
		memset( &s6_add , 0 , sizeof(s6_add) ) ;
		s6_add.sin6_family = AF_INET6  ;
		if( inet_pton( AF_INET6 , remote_ip , &s6_add.sin6_addr ) < 0 ){
			printf("convert host ip %s to network ip error!\n" , remote_ip );
			return -1 ; 
		}
		s6_add.sin6_port = htons( remote_port ) ; 
		if( connect( s_socket , (struct sockaddr*)&s6_add , sizeof(s6_add) ) < 0 ){
			printf("create server socket connection error!\n");
			return -1; 
		}
	}
	else {
		// ip_v4 
		if( (s_socket = socket(AF_INET ,SOCK_STREAM , 0) ) < 0 ){
			printf("create server socket error!\n");
			return -1; 
		}
		memset( &s_add , 0 , sizeof(s_add) ) ; 
		s_add.sin_family = AF_INET ; 
		s_add.sin_addr.s_addr = inet_addr( remote_ip ) ;
		s_add.sin_port = htons( remote_port ) ; 

		if( connect( s_socket , (struct sockaddr *)(&s_add) ,sizeof(s_add) ) < 0 ){
			printf("create server socket connection error!\n");
			return -1; 
		}
	}
	return s_socket ;
}

int read_line( int c_socket , char *buf ){
	char ch[5] ;
	while( 1 ){
		if( recv( c_socket ,ch , 1 , 0 ) <= 0 ){
			*buf = '\0' ; return 1 ;
		}
		*buf = ch[0] ; buf++ ; 
		if( ch[0] == '\n' )	break ;
	}
	*buf = '\0' ; 
	return 0 ;
}

int parse( char *buf , HTTP_REQUEST_TYPE *r_type ){
	if( strstr( buf , "GET" ) != NULL )
		*r_type = GET ;
	else if( strstr( buf , "POST" ) != NULL ){
		*r_type = POST ;
	}
	else if( strstr( buf , "CONNECT" ) != NULL ){
		*r_type = CONNECT ; 
	}
	else{
		printf("unknown HTTP REQUEST TYPE!\n");
		return 1 ; 
	}
	return 0 ;
}

int check_buffer( char *base_p , int buf_size , char *strline , int sz ){
	if( buf_size + sz >= BUF_SIZE )
		return 1 ;
	strncpy( base_p , strline , sz ) ; 
	return 0 ; 
}

int read_header( int c_socket , char *buf ) {
	char strline[ BUF_SIZE ];
	HTTP_REQUEST_TYPE r_type ;
	char *base_p = buf ; 
	int sz , buf_size = 0 ;
	int i ; 

	if( read_line( c_socket , strline ) )
		return 1 ; 	
	sz = (int)strlen( strline ) ; 
	if( parse( strline , &r_type ) )
		return 1 ;
	sz = (int)strlen( strline ) ;
	if( check_buffer( base_p , buf_size , strline , sz) ){
		printf("buffer overflow!\n");
		return 1 ;
	}
	base_p += sz ; buf_size += sz ; 
	
	while( 1 ) {
		if( read_line( c_socket , strline ) )
			return 1 ;
		sz = (int)strlen( strline ) ;
		if( check_buffer( base_p , buf_size , strline , sz ) ){
			printf("buffer overflow!\n");
			return 1 ; 
		}
		base_p += sz; buf_size += sz ; 
		if( strcmp( strline , "\r\n" )==0 || strcmp( strline , "\n" )==0 ){
			if( r_type == POST ){
				sz = recv( c_socket , strline , BUF_SIZE , 0 ) ; 
				if( check_buffer( base_p , buf_size , strline , sz ) ){
					printf("buffer overflow!\n");
					return 1; 
				}
				base_p += sz ; buf_size += sz ; 
			}	
			break ; 
		}
	}
	*base_p = 0 ;
	return 0 ; 
}

void Encode(char *ch , int sz){
	int i ; 
	for(i=0;i<sz;i++){
		ch[i] += 1 ; 
	}
}

void Decode(char *ch , int sz){
	int i ;
	for(i=0;i<sz;i++){
		ch[i] -= 1 ;
	}
}

int deal_with_request( int c_socket ){
	int s_socket ;
	char buf[ BUF_SIZE ] ; 
	int buf_size ; 
	int i ; 

	if( (s_socket = create_server_socket() ) < 0 ){
		printf("create server socket error!\n");
		return 1 ; 
	}
	if( fork() == 0 ){
		if( 0 == read_header( c_socket , buf ) ){
			buf_size = (int)strlen(buf) ; 
			Log(buf) ;
			if( is_decode )
				Encode( buf , buf_size ) ;
			if( send( s_socket , buf , buf_size , 0 ) > 0 ){
				while( 1 ){
					if(  (buf_size=recv( c_socket , buf , BUF_SIZE , 0 ) ) <= 0 )	break ; 
					if( is_decode )	
						Encode( buf , buf_size ) ;
					if( send( s_socket , buf , buf_size , 0 ) < 0 )	break ; 
				}
			} 
		}
		shutdown( c_socket , SHUT_RDWR ) ;
		close( c_socket ) ;
		shutdown( s_socket , SHUT_RDWR ) ; 
		close( s_socket ) ;
		exit( 0 )  ;
	}
	if( fork() == 0 ){
		while( 1 ) {
			buf_size = recv( s_socket , buf , BUF_SIZE , 0 ) ; 
			if( buf_size <= 0 )	break ;
			if( is_decode )
				Decode( buf , buf_size ) ; 
			if( send( c_socket , buf , buf_size , 0 ) <= 0 )	break ; 
		}
		shutdown( c_socket , SHUT_RDWR ) ;
		close( c_socket ) ;
		shutdown( s_socket , SHUT_RDWR ) ;
		close( s_socket ) ;
		exit( 0 ) ;
	}
	close( s_socket ) ; 
	close( c_socket ) ; 
}

void deal_sigchld( int sig ){
	while( waitpid(-1,NULL,WNOHANG) > 0 ) ;
}

int main(int argc,char **argv){
	int listen_socket ;
	int c_socket ; 
	struct sockaddr_in c_add ; 
	int client_size ; 

	if( parse_arg( argc , argv ) )
		return 0 ; 
	if( (listen_socket = create_listen_socket() ) < 0 ){
		printf("create listen socket error occur!\n");
		return 1 ; 
	}
	signal( SIGCHLD , deal_sigchld ) ;
	hint() ; 	
	while( 1 ){
		client_size = sizeof( c_add ) ;
		c_socket = accept( listen_socket , (struct sockaddr *)(&c_add) , &client_size ) ;
		if( c_socket == -1 ){
			printf("socket accept error!\n");
			return 1 ; 
		}
		if( fork() == 0 ){
			close( listen_socket ) ; 
			deal_with_request( c_socket ) ; 
			exit(0) ; 
		}
		close( c_socket ) ;
	}	
	return 0 ; 
}

