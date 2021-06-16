#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_BUF 512 

int podeReceber = 1;
int numAlto = 0;
int numBaixo = 0;
int numEco = 0;
int numLento = 0;
int numRapido = 0;

void sigtermHandler(int signum){
    podeReceber = 0;
    printf("O servidor nao ira aceitar mais pedidos.\n");
}

//argv[1] == config file argv[2] = FILENAME_filters
int main(int argc, char * argv[]){

    if(signal(SIGTERM, sigtermHandler) == SIG_ERR){
        perror("SIGTERM failed: ");
    }

    //receber parametros da config
    int fdCnfg = open(argv[1], O_RDONLY);
    if(fdCnfg < 0){
        printf("Ficheiro config invalido.\n");
        close(fdCnfg);
        return 1;
    }
    //le do ficheiro
    char buf[MAX_BUF];
    read(fdCnfg, &buf, MAX_BUF);
    close(fdCnfg);
    char *cnfglines[5];
    //divide por \n
    char *token;
    token = strtok(buf,"\n");
    for(int i = 0; token != NULL; i++){
        cnfglines[i] = token;
        token = strtok(NULL, "\n");
    }
    //divide cada linha por espaco
    char *cnfg[5][3];
    for(int i = 0; i < 5; i++){
        token = strtok(cnfglines[i], " ");
        for(int j = 0; token != NULL; j++){
            cnfg[i][j] = token;
            token = strtok(NULL, " ");
        }
    }
    //guarda o numero de ocorrencias concorrentes nas vars globais
    for(int i = 0; i < 5; i++){
    if(strcmp(cnfg[i][0], "alto") == 0)
        numAlto = atoi(cnfg[i][2]);
    else if(strcmp(cnfg[i][0], "baixo") == 0)
        numBaixo = atoi(cnfg[i][2]);
    else if(strcmp(cnfg[i][0], "eco") == 0)
        numEco = atoi(cnfg[i][2]);
    else if(strcmp(cnfg[i][0], "rapido") == 0)
        numRapido = atoi(cnfg[i][2]);
    else if(strcmp(cnfg[i][0], "lento") == 0)
        numLento = atoi(cnfg[i][2]);
    }
    

    //sem argumentos -> -h
    if(argc == 1){
        printf("./servidor config-filename filters-folder\n");
    }
    else if(argc != 3 ){
        printf("Chamada invalida.\n");
    }
    else{
        //criar fifo para receber pedidos
        if( mkfifo("./tmp/CtoS",0644) < 0){
            perror("Servidor - Fifo: ");
        }
        //abre ponta de leitura
        int fd = open("./tmp/CtoS", O_RDONLY);
        if(fd < 0){
            perror("Servidor - Open fifo: ");
        }
        //abre ponta de escrita para nao receber EOF
        int fd2 = open("./tmp/CtoS", O_WRONLY);
        if(fd2 < 0){
            perror("Servidor - Open fifo: ");
        }

        //receber pedidos
        int bytes_read;
        char buf[MAX_BUF];
        while( (bytes_read = read(fd, &buf, MAX_BUF)) > 0 ){
            
            //parse do cmd recebido para um array de strings
            int numFiltros = 0;
            char * args[10];
            char * token;
            char * pathFifo;
            token = strtok(buf, ",");
            pathFifo = token;
            int i;
            for( i = 0; i < 9 && token != NULL; i++){
                if(i >= 4){
                    numFiltros++;
                    if(strcmp(token, cnfg[0][0] ) == 0)
                        args[i] = cnfg[0][1];
                    else if(strcmp(token, cnfg[1][0] ) == 0)
                        args[i] = cnfg[1][1];
                    else if(strcmp(token, cnfg[2][0] ) == 0)
                        args[i] = cnfg[2][1];
                    else if(strcmp(token, cnfg[3][0] ) == 0)
                        args[i] = cnfg[3][1];
                    else if(strcmp(token, cnfg[4][0] ) == 0)
                        args[i] = cnfg[4][1];
                }
                else{
                    args[i] = token;
                }
                token = strtok(NULL, ",");
            }
            args[i] = NULL;

            if(strcmp(args[1], "transform") == 0){
                if(fork() == 0){ // cria monitor

                    //abre fifo para responder a cliente
                    int fdR = open(pathFifo, O_WRONLY);
                    if(fdR < 0){
                        perror("Servidor - Open fifo resp: ");
                    }
                    //se ja nao pode receber mais pedidos
                    if(!podeReceber){
                        write(fdR, "Servidor nao ira receber mais pedidos.\n", 39);
                        close(fdR);
                        _exit(0);
                    }
                    //diz a cliente que comecou a processar
                    else{
                        write(fdR, "Processing...\n", 14);
                    }
                    

                    //aplicacao de filtros
                    for(int i = 0; i < numFiltros; i++){
                        if(fork() == 0){
                        //abre o audio para aplicar filtros
                        if( i == 0){
                            int fdSample = open(args[2], O_RDONLY);
                            if(fdSample < 0){
                                perror("Erro abrir sample: ");
                                _exit(5);
                            }
                            dup2(fdSample, 0);
                            close(fdSample);
                        }
                        else{//ja aplicou o 1o filtro e o proximo filtro e no output
                            int fdOut = open(args[3], O_RDONLY);
                            if(fdOut < 0){
                                perror("Erro abrir output: ");
                            }
                            dup2(fdOut, 0);
                            close(fdOut);
                        }
                        //abre ficheiro output
                        int fdOut = open(args[3],O_CREAT | O_WRONLY, 0644);
                        dup2(fdOut, 1);
                        close(fdOut);

                        //prepara path do filtro
                        char filter[40] = "";
                        strcat(filter, argv[2]); strcat(filter,"/"); strcat(filter, args[i+4]);
                        //executa o filtro
                        execl(filter, args[i+4], NULL);
                        _exit(5);

                        }
                        else{//sequencial
                            int status;
                            wait(&status);
                            printf("status:%d\n",WEXITSTATUS(status));
                        }
                    }
                    //depois de fazer filtros todos diz a cliente q acabou e sai
                    write(fdR, "Finished\n", 9);
                    close(fdR);
                    _exit(0);
                }
            }
            else if(strcmp(args[0], "status") == 0){
                //write(fdR, getStatus, 99);
            }
        }

        close(fd);
        close(fd2);

    }
    return 0;
}