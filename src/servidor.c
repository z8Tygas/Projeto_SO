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

//-----------------------Linked List funcs------------------------------------------
typedef struct node{
    int pid;
    char string[150];
    struct node *prox;
} *TLIST;

void newNode(TLIST *head, int pid, char *string)
{
    TLIST new_node = malloc(sizeof(struct node));
    new_node->pid = pid;
    strcpy(new_node->string, string);
    new_node->prox = NULL;
    if((*head) == NULL){
        (*head) = new_node;
        return;
    }
    TLIST l = (*head);
    while(l->prox != NULL){
        l = l->prox;
    }
    l->prox = new_node;
}

void deleteNode(TLIST* head_ref, int pid)
{
    // guarda cabeca da lista
    TLIST temp = *head_ref, ant;
 
    // se cabeca tem pid
    if (temp != NULL && temp->pid == pid) {
        *head_ref = temp->prox;
        free(temp);
        return;
    }
 
    // procura pid e guarda anterior
    while (temp != NULL && temp->pid != pid) {
        ant = temp;
        temp = temp->prox;
    }
 
    // nao encontrou
    if (temp == NULL)
        return;
 
    // encontrei, anterior aponta para o proximo
    ant->prox = temp->prox;
    free(temp);
}

//-----------------------------vars globais para serem usadas por sighandlers------------------------
int podeReceber = 1;

//inicializar lista de tasks
TLIST listaTasks = NULL;

//--------------------------------------------sig handlers--------------------------------------
void sigtermHandler(int signum){
    podeReceber = 0;
    write(1, "O servidor nao ira aceitar mais pedidos.\n", 41);
    if(listaTasks == NULL)
        exit(0);

}

//argv[1] == config file | argv[2] = FILENAME_filters
//---------------------------------------------Main-------------------------------------------
int main(int argc, char * argv[]){

    if(signal(SIGTERM, sigtermHandler) == SIG_ERR){
        perror("SIGTERM failed: ");
    }

    //-----------------verificar call--------------------
    if(argc == 1){
        write(1, "./servidor config-filename filters-folder\n", 42);
    }
    else if(argc != 3 ){
        write(1, "Chamada invalida.\n", 18);
    }
    else{
        //shared variables
        int *filtrosUsados[5];
        for(int i = 0; i < 5; i++){
            filtrosUsados[i] = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            *filtrosUsados[i] = 0;
        }
        int *numTask = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        *numTask = 0;

        //-------------------------------ler config---------------------
        int fdCnfg = open(argv[1], O_RDONLY);
        if(fdCnfg < 0){
            write(1, "Ficheiro config invalido.\n", 26);
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
                    if(strcmp(token, "alto") == 0)  //cnfg[0] -> alto
                        k = 0;
                    else if(strcmp(token, "baixo") == 0) //cnfg[1] -> baixo
                        k = 1;
                    else if(strcmp(token, "eco") == 0) //cnfg[2] -> eco
                        k = 2;
                    else if(strcmp(token, "rapido") == 0) //cnfg[3] -> rapido
                        k = 3;
                    else if(strcmp(token, "lento") == 0) //cnfg[4] -> lento
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

        //------------------------criar fifo------------------------------------
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

        //-------------------------receber pedidos---------------------------
        char buf2[MAX_BUF];
        while( (podeReceber || listaTasks != NULL ) && (read(fd, &buf2, MAX_BUF)) > 0 ){
            //------------------parse de cmd recebido--------------------------
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
                }
                else{
                    args[i] = token;    //args[0] pid | [1] transform/status | [2] infile | [3] outfile | [4] filtros
                }
                token = strtok(NULL, ",");
            }
            //---------------reconhecer pedido--------------------------
            //pedidos de gestao de lista de pedidos
            if(strcmp(args[1],"add") == 0){
                newNode(&listaTasks, atoi(args[0]), args[2]);
            }
            else if(strcmp(args[1], "remove") == 0){
                deleteNode(&listaTasks, atoi(args[0]));
            }
            else{//pedido de status ou transform
                //-----------------manager de pedido --------------------
                if(fork() == 0){
                    if(strcmp(args[1], "transform") == 0){
                        //abre fifo para responder a cliente
                        int fdR = open(pathFifo, O_WRONLY);
                        if(fdR < 0){
                            perror("Servidor - Open fifo cliente resp: ");
                        }
                        //------------------------------testa se pode correr---------------------
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
                        pid_t pidmon;
                        //---------------------------manager de execs-------------------------
                        if((pidmon = fork()) == 0){

                            //verifica se tem filtros suficientes siponiveis
                            if(filtrosNecessarios[0] > (filtrosMax[0] - *filtrosUsados[0]) || filtrosNecessarios[1] > (filtrosMax[1] - *filtrosUsados[1]) ||
                            filtrosNecessarios[2] > (filtrosMax[2] - *filtrosUsados[2]) || filtrosNecessarios[3] > (filtrosMax[3] - *filtrosUsados[3]) ||
                            filtrosNecessarios[4] > (filtrosMax[4] - *filtrosUsados[4])){
                                write(fdR, "Pending...\n", 11);
                            while(filtrosNecessarios[0] > (filtrosMax[0] - *filtrosUsados[0]) || filtrosNecessarios[1] > (filtrosMax[1] - *filtrosUsados[1]) ||
                            filtrosNecessarios[2] > (filtrosMax[2] - *filtrosUsados[2]) || filtrosNecessarios[3] > (filtrosMax[3] - *filtrosUsados[3]) ||
                            filtrosNecessarios[4] > (filtrosMax[4] - *filtrosUsados[4]))
                                sleep(1);
                            }
                            //adicionar filtros que vai gastar ao total
                            for(int i = 0; i < 5; i++){
                                *filtrosUsados[i] += filtrosNecessarios[i];
                            }

                            //diz a cliente que comecou a processar
                            write(fdR, "Processing...\n", 14);
                            close(fdR);

                            int fdSample,fdOut;
                            //abre sample e redir para stdin
                            if( (fdSample = open(args[2], O_RDONLY)) == -1){
                                perror("Abrir sample: "); return -1;
                            }
                            dup2(fdSample,0); close(fdSample);
                            
                            //abre o output e redir para stdout
                            if( (fdOut = open(args[3], O_TRUNC | O_WRONLY | O_CREAT, 0644)) == -1){
                                perror("Abrir output: "); return -1;
                            };
                            dup2(fdOut,1); close(fdOut);

                            //------------------aplicacao de filtros----------------------------
                            int p[2];
                            int i;
                            for(i = 0; i < (numFiltros-1); i++){
                                pipe(p);
                                if(fork() == 0){
                                    dup2(p[1], 1); close(p[0]); close(p[1]);
                                    //prepara path do filtro
                                    char filter[40] = "";
                                    strcat(filter, argv[2]); strcat(filter,"/"); strcat(filter, args[i+4]);
                                    //executa o filtro
                                    if(execl(filter, args[i+4], NULL) < 0){
                                        perror("Erro filtro: ");
                                        _exit(5);
                                    }
                                }
                                else{
                                    dup2(p[0], 0); close(p[0]); close(p[1]);
                                }
                            }
                            //prepara path do ultimo filtro (stdout -> outputfile)
                            char filter[40] = "";
                            strcat(filter, argv[2]); strcat(filter,"/"); strcat(filter, args[i+4]);
                            //executa o filtro
                            if(execl(filter, args[i+4], NULL) < 0){
                                perror("Erro filtro: ");
                                _exit(5);
                            }
                        } // -------------------- fim manager de execs ----------------------
                        else{ // codigo de manager de pedido
                            //-----------------------------adiciona/remove task a lista de tasks--------------------------------
                            char novaTask[190] = "";
                            char temp[150] = "";
                            for(int i = 2; i < (4 + numFiltros) ; i++){
                                strcat(temp, " ");
                                if(strcmp(args[i], cnfg[0][1] ) == 0){
                                    strcat(temp, cnfg[0][0]);
                                }
                                else if(strcmp(args[i], cnfg[1][1] ) == 0){
                                    strcat(temp, cnfg[1][0]);
                                }
                                else if(strcmp(args[i], cnfg[2][1] ) == 0){
                                    strcat(temp, cnfg[2][0]);
                                }
                                else if(strcmp(args[i], cnfg[3][1] ) == 0){
                                    strcat(temp, cnfg[3][0]);
                                }
                                else if(strcmp(args[i], cnfg[4][1] ) == 0){
                                    strcat(temp, cnfg[4][0]);
                                }
                                else{
                                    strcat(temp, args[i]);
                                }
                                
                            }
                            sprintf(novaTask, "%d,add,Task #%d: transform%s%c",pidmon, *numTask, temp,'\0');
                            *numTask += 1;
                            //pede ao servidor para adicionar esta task
                            write(fd2, &novaTask, strlen(novaTask)+1);

                            int status;
                            waitpid(pidmon, &status, 0);
                            
                            //depois de fazer filtros todos liberta filtros que usou
                            for(int i = 0; i < 5; i++){
                                *filtrosUsados[i] -= filtrosNecessarios[i];
                            }
                            //diz a cliente que acabou
                            write(fdR, "Finished\n", 9);
                            close(fdR);

                            sprintf(novaTask, "%d,remove%c",pidmon,'\0');
                            //pede ao servidor para tirar esta task
                            write(fd2, novaTask, strlen(novaTask)+1);
                            _exit(0);
                        }
                    }//fim if transform
                    else if(strcmp(args[1], "status") == 0){
                        //abre fifo para responder a cliente
                        int fdR = open(pathFifo, O_WRONLY);
                        if(fdR < 0){
                            perror("Servidor - Open fifo cliente resp: ");
                        }
                        //envia a cliente lista de tasks a correr e por correr
                        char status[MAX_BUF] = "";
                        while (listaTasks != NULL) {
                            sprintf(status, "%s\n%c", listaTasks->string, '\0');
                            write(fdR, status, strlen(status)+1);
                            listaTasks = listaTasks->prox;
                        }
                        //envia a cliente status dos filtros
                        for(int i = 0; i < 5; i++){
                            sprintf(status, "filter %s: %d/%d (running/max)\n%c",cnfg[i][0], *filtrosUsados[i], filtrosMax[i], '\0');
                            write(fdR, status, strlen(status)+1);
                        }
                        strcpy(status, "");
                        sprintf(status, "pid: %d\n%c",getppid(), '\0');
                        write(fdR, status, strlen(status)+1);
                        close(fdR);
                        _exit(0);
                    }
                }//fim monitor de pedido
            }
        }

        close(fd);
        close(fd2);

    }
    return 0;
}