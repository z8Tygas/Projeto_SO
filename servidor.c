#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define MAX_BUF 1024
#define Max_FILTROS 15

int podeReceber = 1;

void sigtermHandler(int signum){
    podeReceber = 0;
    printf("O servidor nao ira aceitar mais pedidos.\n");
}

//argv[1] == config file argv[2] = FILENAME_filters
int main(int argc, char * argv[]){
    
    int *filtrosUsados[5];

    for(int i = 0; i < 5; i++){
        filtrosUsados[i] = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    }
    

    if(signal(SIGTERM, sigtermHandler) == SIG_ERR){
        perror("SIGTERM failed: ");
    }

    //sem argumentos -> -h
    if(argc == 1){
        printf("./servidor config-filename filters-folder\n");
    }
    else if(argc != 3 ){
        printf("Chamada invalida.\n");
    }
    else{
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
        //divide cada linha por espaco e da sort no array cnfg
        char *cnfg[5][3];
        for(int i = 0,k = -1; i < 5; i++){
            token = strtok(cnfglines[i], " ");
            for(int j = 0; token != NULL; j++){
                if(j==0){
                    if(strcmp(token, "alto") == 0)
                        k = 0;
                    else if(strcmp(token, "baixo") == 0)
                        k = 1;
                    else if(strcmp(token, "eco") == 0)
                        k = 2;
                    else if(strcmp(token, "rapido") == 0)
                        k = 3;
                    else if(strcmp(token, "lento") == 0)
                        k = 4;
                }
                cnfg[k][j] = token;
                token = strtok(NULL, " ");
            }
        }
        //guarda o numero max de ocorrencias concorrentes de cada filtro
        int filtrosMax[5];
        for(int i = 0; i < 5; i++){
            filtrosMax[i] = atoi(cnfg[i][2]);
        }

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
        char buf2[MAX_BUF];
        while( (bytes_read = read(fd, &buf2, MAX_BUF)) > 0 ){
            
            //parse do cmd recebido para um array de strings
            int numFiltros = 0;
            char * args[4 + Max_FILTROS];
            char * token;
            char * pathFifo;
            token = strtok(buf2, ",");
            pathFifo = token;
            int filtrosNecessarios[5];
            for(int i = 0; i < 5; i++) filtrosNecessarios[i] = 0;
            for( int i = 0; i < 4+Max_FILTROS && token != NULL; i++){
                if(i >= 4){
                    numFiltros++;
                    if(strcmp(token, cnfg[0][0] ) == 0){
                        args[i] = cnfg[0][1];
                        filtrosNecessarios[0]++;
                    }
                    else if(strcmp(token, cnfg[1][0] ) == 0){
                        args[i] = cnfg[1][1];
                        filtrosNecessarios[1]++;
                    }
                    else if(strcmp(token, cnfg[2][0] ) == 0){
                        args[i] = cnfg[2][1];
                        filtrosNecessarios[2]++;
                    }
                    else if(strcmp(token, cnfg[3][0] ) == 0){
                        args[i] = cnfg[3][1];
                        filtrosNecessarios[3]++;
                    }
                    else if(strcmp(token, cnfg[4][0] ) == 0){
                        args[i] = cnfg[4][1];
                        filtrosNecessarios[4]++;
                    }
                    else{
                        printf("%s\n",token);
                    }
                }
                else{
                    args[i] = token;
                }
                token = strtok(NULL, ",");
            }

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
                    //teste se o servidor tem recursos suficientes para processar
                    if(filtrosNecessarios[0] > filtrosMax[0] || filtrosNecessarios[1] > filtrosMax[1] ||
                       filtrosNecessarios[2] > filtrosMax[2] || filtrosNecessarios[3] > filtrosMax[3] || 
                       filtrosNecessarios[4] > filtrosMax[4]){
                        write(fdR, "Servidor nao tem recursos suficientes para processar este pedido.\n", 67);
                        close(fdR);
                        _exit(0);
                    }
                    //verifica se tem filtros suficientes siponiveis
                    if(filtrosNecessarios[0] > (filtrosMax[0] - *filtrosUsados[0]) || filtrosNecessarios[1] > (filtrosMax[1] - *filtrosUsados[1]) ||
                       filtrosNecessarios[2] > (filtrosMax[2] - *filtrosUsados[2]) || filtrosNecessarios[3] > (filtrosMax[3] - *filtrosUsados[3]) ||
                       filtrosNecessarios[4] > (filtrosMax[4] - *filtrosUsados[4]))
                        write(fdR, "Pending...\n", 11);
                    while(filtrosNecessarios[0] > (filtrosMax[0] - *filtrosUsados[0]) || filtrosNecessarios[1] > (filtrosMax[1] - *filtrosUsados[1]) ||
                       filtrosNecessarios[2] > (filtrosMax[2] - *filtrosUsados[2]) || filtrosNecessarios[3] > (filtrosMax[3] - *filtrosUsados[3]) ||
                       filtrosNecessarios[4] > (filtrosMax[4] - *filtrosUsados[4])){
                        sleep(1);
                    }
                    //adicionar filtros que vai gastar ao total
                    for(int i = 0; i < 5; i++){
                        *filtrosUsados[i] += filtrosNecessarios[i];
                    }

                    //diz a cliente que comecou a processar
                    write(fdR, "Processing...\n", 14);
                    

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
                    //depois de fazer filtros todos
                    //liberta filtros que vai usou
                    for(int i = 0; i < 5; i++){
                        *filtrosUsados[i] -= filtrosNecessarios[i];
                    }
                    //diz a cliente que acabou
                    write(fdR, "Finished\n", 9);
                    close(fdR);
                    _exit(0);
                }
            }
            else if(strcmp(args[0], "status") == 0){
                //abre fifo para responder a cliente
                int fdR = open(pathFifo, O_WRONLY);
                if(fdR < 0){
                    perror("Servidor - Open fifo resp: ");
                }
                char status[MAX_BUF] = "";
                for(int i = 0; i < 5; i++){
                    char saux[2] = ""; sprintf(saux, "%d", *filtrosUsados[i]);
                    strcat(status, "filter "); strcat(status, cnfg[i][0]); strcat(status, ": "); strcat(status, saux);
                    sprintf(saux, "%d", filtrosMax[i]);
                    strcat(status, "/"); strcat(status, saux); strcat(status, " (running/max)\n");
                }
                char saux[2] = ""; sprintf(saux, "%d",getpid());
                strcat(status, "pid: "); strcat(status, saux); strcat(status, "\0");

                write(fdR, status, strlen(status)+1);
                close(fdR);
            }
        }

        close(fd);
        close(fd2);

    }
    return 0;
}
