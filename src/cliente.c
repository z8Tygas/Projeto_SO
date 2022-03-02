#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUF 512

//fechar fd -> atraves de receber um signal?

void status(){}

//argv[0] - ./cliente
//argv[1] - transform | status
//argv[2] - path Sample
//argv[3] - out file
//argv[4] - filter #1  
//argv[x] - filter #x max 15
int main(int argc, char * argv[]){
    //open ponta de escrita do fifo
    int fdServer = open("./tmp/CtoS", O_WRONLY);
    if(fdServer < 0){
        printf("Servidor nao esta operacional.\n");
    }
    else{ //servidor esta operacional
        if(argc == 1){ // sem argumentos -> -h
            printf("./cliente transform input-filename output-filename filter-id-1 filter-id-2 ...\n./cliente status\n");
        }
        // pelo menos 5 argumentos (1 filtro +)
        else if( strcmp(argv[1], "transform") == 0 && argc > 4){ 
            //verifica sample existe
            int fdSample = open(argv[2], O_RDONLY);
            if(fdSample < 0){
                printf("Sample nao existe ou nao foi possivel abrir.\n");
                return 1;
            }
            close(fdSample);
            //verifica se ficheiro output ja existe
            int fdOut = open(argv[3], O_RDONLY);
            if(fdOut > 0){
                printf("File \"%s\" já existe.\n",argv[3]);
                return 1;
            }
            close(fdOut);
            //verificar filtros validos
            for(int i = 4; i < argc; i++){
                if( strcmp(argv[i], "alto" ) != 0 &&
                    strcmp(argv[i], "baixo" ) != 0 &&
                    strcmp(argv[i], "eco" ) != 0 &&
                    strcmp(argv[i], "rapido" ) != 0 &&
                    strcmp(argv[i], "lento" ) != 0 )
                {
                    printf("Chamada inválida, filtro: %s não é válido\n", argv[i]);
                    return 1;
                }
            }
            
            //cria fifo para receber dados do servidor
            char path[15] = "";
            sprintf(path, "./tmp/%d", getpid());
            if( mkfifo(path, 0644) < 0){
                perror("Cliente - Criar/Abrir fifo: ");
                return 1;
            }

            //prepara comando
            char cmd[100] = "";
            strcat(cmd, path); // path do fifo
            strcat(cmd, ",transform,"); // transform / argv[1]
            strcat(cmd, argv[2]); strcat(cmd, ","); // filename
            strcat(cmd, argv[3]); // output filename
            for(int i = 4; i < argc; i++){
                strcat(cmd, ",");
                strcat(cmd, argv[i]);
            }
            strcat(cmd, "\0");

            //envia comando para o servidor
            write(fdServer, &cmd, strlen(cmd)+1);

            //abre fifo para receber dados
            int fdR = open(path, O_RDONLY);
            if( fdR < 0){
                perror("Cliente - Criar/Abrir fifo: ");
            }
            
            //le do fifo
            int bytes_read;
            char buf[MAX_BUF];
            while( (bytes_read = read(fdR, &buf, MAX_BUF)) > 0 ){
                write(1, &buf, bytes_read);
            }
        }
        else if( strcmp(argv[1], "status") == 0){
            //cria fifo para receber dados do servidor
            char path[15] = "";
            sprintf(path, "./tmp/%d", getpid());
            if( mkfifo(path, 0644) < 0){
                perror("Cliente - Criar/Abrir fifo: ");
                return 1;
            }
            //prepara comando
            char cmd[25] = "";
            strcat(cmd, path); // path do fifo
            strcat(cmd, ",status"); // status / argv[1]
            strcat(cmd, "\0");

            //envia comando para o servidor
            write(fdServer, &cmd, strlen(cmd)+1);
            
            //abre fifo para receber dados
            int fdR = open(path, O_RDONLY);
            if( fdR < 0){
                perror("Cliente - Criar/Abrir fifo: ");
            }

            //le do fifo
            int bytes_read;
            char buf[MAX_BUF];
            while( (bytes_read = read(fdR, &buf, MAX_BUF)) > 0 ){
                write(1, &buf, bytes_read);
            }
            close(fdR);
        }
        else{
            printf("Chamada Invalida.\n");
        }    
    }
    return 0;
}