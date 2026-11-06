#include <stdio.h> // Lib padrão de entrada e saída, tem o fprint, printf, scan, etc.
#include <stdlib.h> // Lib padrão geral, tem o exit(), atoi(), etc.
#include <unistd.h>   // getopt() está nessa lib, e também várias outras systemcalls do POSIX.
#include <netinet/in.h> // struct sockaddr_in, htons()
#include <arpa/inet.h>   // inet_pton()
#include <string.h>      // memset()
#include <sys/socket.h>   // socket(), sendto(), recvfrom(), bind()
#include <sys/types.h>    // tipos usados por várias system calls de rede 
#include <sys/stat.h>
#include <ctype.h>



void cmd_login(char *linha, char *comando, char *uid_logged, char *password_logged, int sockfd, struct sockaddr_in *ds_addr, char *peerport, int *its_logged){

    // criando as variáveis para armazenar o user id e passwor que serão lidos na sequência
    // Importante lembrar que o UID precisa ter 6 caracteres e a password 8, colocamos um a mais por que em C sabemos que strings terminam com \n
    char uid[8]; // Coloquei 8 para termos uma mragem no sscanf, ele vai ler até 7 caracteres em caso de que colocarmos mais de 6
    char password[10]; // Mesma coisa aqui, aumento a password para mais um no sscanf botamos o limite de leitura e escrita para 9.
    
    int n = sscanf(linha, "%29s %7s %9s", comando, uid, password); // O scan lê as três strings do input e armazena nas variáveis acima.
    // Detalhe importante dessa vez o sscanf está sendo armazenado em n, isso é por que o método retorna um valor int que representa quantas palavras ele armazenou nas variáveis.
    // No caso do enunciado precisamos ter certeza de que foram inseridas as três palavras
    
    if(n!= 3){
        fprintf(stderr, "Erro: comando login precisa de UID e password\n");
    }
    // Também precisaremos saber se os valores digitados são de exatamente 6 e 8 caracteres
    // strlen devolve o comprimento em quanidade de caracteres que uma string tem.
    else if (strlen(uid) != 6 || strlen(password) != 8){
    
        fprintf(stderr, "Erro: UID deve ter 6 dígitos e password 8 caracteres\n");
    } 
    else {
        // Depois de todas as validações devemos mandar o login e senha para o servidor e imprimir a mensagem LIN
    
        char mensagem[64];
        sprintf(mensagem, "LIN %s %s %s\n", uid, password, peerport); // Cada %s corresponde na ordem, a uid, password e peerport.
    
        // Agora vamos para a função que envia ao servidor, sendto() tem 6 parâmetros
        // 1 -> sockfd - especifica qual socket vamos usar para mandar dados, nesse caso o socket que criamos lá em cima.
        // 2 -> mensagem - Um ponteiro para os dados que queremos enviar, no caso a mensagem acima criada.
        // 3 -> strlen(mensagem) - Quantos bytes o programa vai exatamente mandar, usamos o strlen para pegar o valor exato.
        // 4 -> flags no nosso caso não usaremos nenhuma flag de comportamento.
        // 5 -> Para onde mandaremos os dados já definimos acima o nosso ds_addr
        // 6 -> Tamanho em bytes da struct de endereço 
    
        int bytes_enviados = sendto(sockfd, mensagem, strlen(mensagem), 0, (struct sockaddr *)ds_addr, sizeof(*ds_addr));
    
        // Sendto() retorna um número positivo de quantos realmente foram enviados.
    
        // Caso houve algum erro e nada foi enviado sendto() retorna -1
        if (bytes_enviados == -1) {
            fprintf(stderr, "Erro ao enviar mensagem para o DS\n");
            return; // Sai do cmd_login sem chegar ao rcvfrom
        }
    
        // Agora vamos para a resposta
    
        char resposta[64]; // Buffer onde vamos guardar os bytes que chegarem do servidor
    
        struct sockaddr_in from_addr; // Uma struct de endereço, vai guardar em from_addr de onde veio a resposta (contendo o IP e porta de quem enviou)
        socklen_t from_len = sizeof(from_addr); // Define o tamanho de from_len para o tamanho de from_addr
    
        // A função recvfrom() é bem parecida com a função sendto()
        // 1 -> sockfd - especifica qual socket vamos usar para mandar dados, nesse caso o socket que criamos lá em cima.
        // 2 -> para onde os dados recebidos vão ser escritos
        // 3 -> Quantos bytes no máximo aceitar, temos o - 1 por que precisamos adicionar o byte terminador ao final do texto e recvfrom não sabe que estamos lidando com texto.
        // 4 -> Flags também não usaremos nenhuma flag aqui.
        // 5 -> Onde guardar o endereço de quem mandou essa resposta.
        // 6 -> Fromlen é um ponteiro(&), por que recvfrom() modifica from_len, então recebe o endereço de from_len para preencher nela.
        int bytes_recebidos = recvfrom(sockfd, resposta, sizeof(resposta) - 1, 0,(struct sockaddr *)&from_addr, &from_len);
    
        if (bytes_recebidos == -1){
            fprintf(stderr, "Erro ao receber a resposta do DS\n");
            return;
        }
            resposta[bytes_recebidos] = '\0'; // Adiciona o byte terminador a resposta
    
            // Declaramos os dois arrays que vamos guardar as palavras que vamos extrair de resposta
            char tipo_resposta[20]; // Isso vai ser enviado pelo servidor do técnico, pode ser: RLI (Reply to Login), RLO (Reply to Logout) ou RUR (Reply to unregister)
            char status[16]; // Devolvido pelo servidor: OK (login ok), NOK (senha incorreta) ou REG (novo UID registrado)
    
            // Extrai a primeira palvra para tipo de resposta que deve ser algo como RLI e a segunda para status que deve ser algo como OK.
            sscanf(resposta, "%s %s", tipo_resposta, status);
    
            // Compara status com a string a seguir, strcmp retorna 0 quando as strings são iguais
            if(strcmp(status, "OK") == 0){ // Caso login efetuado com sucesso
                printf("Login efetuado com sucesso!\n");
                strcpy(uid_logged, uid); // Copia o UID que acabou de logar para a variável declarada no inicio.
                strcpy(password_logged, password);  // Copia a password que acabou de logar para a variável declarada no inicio.
                *its_logged = 1; // Agora its_logged é 1 o que seria true e indentifica que alguem logou
            }
            else if (strcmp(status, "NOK") == 0) { // Caso senha incorreta
                printf("Senha incorreta.\n");
            }
    
            else if (strcmp(status, "REG") == 0) { // Caso novo usuário seja registrado
                printf("Novo usuário registrado com sucesso!\n");
                strcpy(uid_logged, uid); // Copia o UID que acabou de logar para a variável declarada no inicio.
                strcpy(password_logged, password);  // Copia a password que acabou de logar para a variável declarada no inicio.
                *its_logged = 1; // Agora its_logged é 1 o que seria true e indentifica que alguem logou
            }
            // Se status não bater com nenhum desses casos acima, entramos nesse else
            else {
                printf("Resposta inesperada do servidor: %s\n", resposta);
        }
    }

}

void cmd_logout(char *uid_logged, char *password_logged, int sockfd, struct sockaddr_in *ds_addr, int *its_logged){
    if(*its_logged == 0){

        fprintf(stderr, "Erro: nenhum usuário está logado\n");

        }
        else{
            
        char mensagem[64];
        sprintf(mensagem,"LOU %s %s\n", uid_logged, password_logged);// monta a mensagem LOU usando UID_logged e password_logged (não pede pro usuário digitar de novo!)

        // Agora vamos para a função que envia ao servidor, sendto() tem 6 parâmetros
        // 1 -> sockfd - especifica qual socket vamos usar para mandar dados, nesse caso o socket que criamos lá em cima.
        // 2 -> mensagem - Um ponteiro para os dados que queremos enviar, no caso a mensagem acima criada.
        // 3 -> strlen(mensagem) - Quantos bytes o programa vai exatamente mandar, usamos o strlen para pegar o valor exato.
        // 4 -> flags no nosso caso não usaremos nenhuma flag de comportamento.
        // 5 -> Para onde mandaremos os dados já definimos acima o nosso ds_addr
        // 6 -> Tamanho em bytes da struct de endereço 

        int bytes_enviados = sendto(sockfd, mensagem, strlen(mensagem), 0, (struct sockaddr *)ds_addr, sizeof(*ds_addr));

        // Sendto() retorna um número positivo de quantos realmente foram enviados.

        // Caso houve algum erro e nada foi enviado sendto() retorna -1
        if (bytes_enviados == -1) {
        fprintf(stderr, "Erro ao enviar mensagem para o DS\n");
        return; // Sai do cmd_logout sem chegar ao rcvfrom

        }

        // Agora vamos para a resposta

        char resposta[64]; // Buffer onde vamos guardar os bytes que chegarem do servidor

        struct sockaddr_in from_addr; // Uma struct de endereço, vai guardar em from_addr de onde veio a resposta (contendo o IP e porta de quem enviou)
        socklen_t from_len = sizeof(from_addr); // Define o tamanho de from_len para o tamanho de from_addr

        // A função recvfrom() é bem parecida com a função sendto()
        // 1 -> sockfd - especifica qual socket vamos usar para mandar dados, nesse caso o socket que criamos lá em cima.
        // 2 -> para onde os dados recebidos vão ser escritos
        // 3 -> Quantos bytes no máximo aceitar, temos o - 1 por que precisamos adicionar o byte terminador ao final do texto e recvfrom não sabe que estamos lidando com texto.
        // 4 -> Flags também não usaremos nenhuma flag aqui.
        // 5 -> Onde guardar o endereço de quem mandou essa resposta.
        // 6 -> Fromlen é um ponteiro(&), por que recvfrom() modifica from_len, então recebe o endereço de from_len para preencher nela.
        int bytes_recebidos = recvfrom(sockfd, resposta, sizeof(resposta) - 1, 0,(struct sockaddr *)&from_addr, &from_len);

        if (bytes_recebidos == -1){
            fprintf(stderr, "Erro ao receber a resposta do DS\n");
            return;
        }
            resposta[bytes_recebidos] = '\0'; // Adiciona o byte terminador a resposta

            // Declaramos os dois arrays que vamos guardar as palavras que vamos extrair de resposta
            char tipo_resposta[20]; // Isso vai ser enviado pelo servidor do técnico, pode ser: RLI (Reply to Login), RLO (Reply to Logout) ou RUR (Reply to unregister)
            char status[16]; // Devolvido pelo servidor: OK (login ok), NOK (senha incorreta) ou REG (novo UID registrado)

            // Extrai a primeira palvra para tipo de resposta que deve ser algo como RLI e a segunda para status que deve ser algo como OK.
            sscanf(resposta, "%s %s", tipo_resposta, status);

            // Compara status com a string a seguir, strcmp retorna 0 quando as strings são iguais
            if(strcmp(status, "OK") == 0){ // Caso logout efetuado com sucesso
                printf("Logout efetuado com sucesso!\n");
                *its_logged = 0; // Agora its_logged é 0 o que seria false e indentifica que não tem ninguém mais logado
            }
            else if (strcmp(status, "NLG") == 0) { // Caso o usuário não esteja logado
                printf("Erro: usuário não estava logado.\n");
            }

            else if (strcmp(status, "UNR") == 0) { // Caso o usuário não esteja registrado
                printf("Erro: usuário não registrado.\n");
            }
            else if (strcmp(status, "WRP") == 0) { // Caso a senha seja incorreta
                printf("Erro: senha incorreta.\n");
            }
            // Se status não bater com nenhum desses casos acima, entramos nesse else
            else {
                printf("Resposta inesperada do servidor: %s\n", resposta);
         }
    }
}

void cmd_unregister(char *uid_logged, char *password_logged, int sockfd, struct sockaddr_in *ds_addr, int *its_logged){

    // Verifica se tem alguém logado
    if(*its_logged == 0){ 
        fprintf(stderr, "Erro: nenhum usuário está logado\n");
    } else {
    
    
        char mensagem[64];
        sprintf(mensagem, "UNR %s %s\n", uid_logged, password_logged);
    
        int bytes_enviados = sendto(sockfd, mensagem, strlen(mensagem), 0, (struct sockaddr *)ds_addr, sizeof(*ds_addr));
        if (bytes_enviados == -1) {
            fprintf(stderr, "Erro ao enviar mensagem para o DS\n");
            return; // Sai do cmd_unregister sem chegar ao rcvfrom

        }
    
        char resposta[64];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
    
        int bytes_recebidos = recvfrom(sockfd, resposta, sizeof(resposta) - 1, 0, (struct sockaddr *)&from_addr, &from_len);
    
        if (bytes_recebidos == -1) {
            fprintf(stderr, "Erro ao receber a resposta do DS\n");
            return;
        } 
            resposta[bytes_recebidos] = '\0';
    
            char tipo_resposta[20];
            char status[16];
            sscanf(resposta, "%s %s", tipo_resposta, status);
    
            if (strcmp(status, "OK") == 0) {
                printf("Unregister efetuado com sucesso!\n");
                *its_logged = 0; // logout automático, conforme o enunciado
            } else if (strcmp(status, "NOK") == 0) {
                printf("Erro: usuário não estava logado.\n");
            } else if (strcmp(status, "UNR") == 0) {
                printf("Erro: usuário não registrado.\n");
            } else if (strcmp(status, "WRP") == 0) {
                printf("Erro: senha incorreta.\n");
            } else {
                printf("Resposta inesperada do servidor: %s\n", resposta);
            }
        }
    

}

void cmd_list(int sockfd, struct sockaddr_in *ds_addr){

    char *mensagem = "LST\n";

    int bytes_enviados = sendto(sockfd, mensagem, strlen(mensagem), 0, (struct sockaddr *)ds_addr, sizeof(*ds_addr));

    if(bytes_enviados == -1){
        fprintf(stderr, "Erro ao enviar a mensagem para o DS !\n");
        return; // Sai do cmd_lst sem chegar ao rcvfrom
    }

    char resposta[1300]; // Aqui colocamos o tamnho da resposta como 1300 pois no pior caso podemos ter 50 filenames.

    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    int bytes_recebidos = recvfrom(sockfd, resposta, sizeof(resposta) - 1 , 0, (struct sockaddr *)&from_addr, &from_len);

    if(bytes_recebidos == -1){

        fprintf(stderr, "Erro ao receber a resposta do DS\n");
        return;
    }

    resposta[bytes_recebidos] = '\0';

    char *reply_type = strtok(resposta, " \n");

    if(reply_type == NULL){
        fprintf(stderr, "Resposta vazia do DS !\n");
        return;
    }

    if(strcmp(reply_type, "RLS") != 0){ // Em caso de a resposta seja diferente de RLS que é o esperado, sendo igual o strcmp devolve 0
        fprintf(stderr, "Resposta inesperada: %s\n", reply_type);
        return;
    }
    
    char *status = strtok(NULL, " \n");

    if(status == NULL){
        fprintf(stderr, "O servidor mandou uma resposta incompleta\n");
        return;
    }

    if(strcmp(status, "NOK") == 0){
        fprintf(stdout, "Não há ficheiros publicados ! \n");
        return;
    }

    if(strcmp(status, "OK") != 0){ // Significa que o status é diferente de OK
        fprintf(stderr, "Status diferente de OK: %s \n", status);
        return;
    }

    // Vamos ao loop para imprimir os nomes dos files

    char *nome = strtok(NULL, " \n");
    if(nome == NULL){
        fprintf(stdout, "Não foram recebidos nomes do DS \n");
        return;
    }
        
    int contador = 0;

    while(nome != NULL){
        contador ++;
        fprintf(stdout, " %d.  %s\n", contador, nome);

        nome = strtok(NULL, " \n");
    }
    fprintf(stdout, "%d ficheiros encontrados !\n", contador);
}

void cmd_versions(char *linha, char *comando, struct sockaddr_in *ds_addr){

    char filename[26];

    int n = sscanf(linha, "%29s %25s", comando, filename);

    if(n != 2){
        fprintf(stderr, "Erro: Comando versions precisa do filename !\n");
        return;
    }

    else if (strlen(filename) > 24){
    
        fprintf(stderr, "Erro: filename tem que ter no máximo 24 caracteres\n");
        return;

    } 

    int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0); // Para a conexão TCP mudamos o tipo de argumento do socket.

    // Verificação se houve erro na criação do socket.
    if(tcp_sockfd == -1){
        fprintf(stderr, "Erro ao criar socket\n");
        return;
    }

    int ret_connect = connect(tcp_sockfd, (struct sockaddr *)ds_addr, sizeof(*ds_addr));

    if(ret_connect == -1){ // Em caso de erro o connect devolve -1
        fprintf(stderr, "Erro a conectar-se com o servido !\n");
        close(tcp_sockfd); // Fecha o socket
        return;
    }

    // Enviar a mensagem para o servidor
    char mensagem[64];
    int tamanho_mensagem = sprintf(mensagem, "VRS %s\n", filename); 

    int acumulado = 0;
    while(tamanho_mensagem > acumulado){

        ssize_t return_write = write(tcp_sockfd,mensagem + acumulado, tamanho_mensagem - acumulado);

        if(return_write == -1){
            fprintf(stderr, "Erro no envio da mensagem para o servidor ! \n");
            close(tcp_sockfd);
            return;
        }

        acumulado = acumulado + return_write;
        
    }

    // Leitura da resposta

    char resposta[4000];
    int total_lido = 0;

    // Loop de leitura
    while(1){

        if((sizeof(resposta) - total_lido - 1) == 0){
            fprintf(stderr, "A mensagem recebida pelo servidor é maior que o buffer da resposta (4000 bytes) !\n");
            close(tcp_sockfd);
            return;
        }

        ssize_t return_read = read(tcp_sockfd, resposta + total_lido, sizeof(resposta)- total_lido - 1);

        if(return_read == -1){
            fprintf(stderr, "Erro na leitura da resposta do servidor !\n");
            close(tcp_sockfd);
            return;
        }

        if(return_read == 0){ // O DS fechou a ligação antes de mandar o \n
            fprintf(stderr, "Erro: O DS fechou a ligação inesperadamente !\n");
            close(tcp_sockfd);
            return;
        }

        total_lido = total_lido + return_read;

        resposta[total_lido] = '\0'; // Adiciona o byte terminador a resposta

        if(strchr(resposta, '\n') != NULL){ // Em caso de já termos lido a mensagem toda saimos do loop
            break;
        }
    }

    close(tcp_sockfd);

    char *reply_type = strtok(resposta, " \n");

    if(reply_type == NULL){
        fprintf(stderr, "Resposta vazia do DS !\n");
        return;
    }

     if(strcmp(reply_type, "RVR") != 0){ // Em caso de a resposta seja diferente de RVR que é o esperado, sendo igual o strcmp devolve 0
        fprintf(stderr, "Resposta inesperada: %s\n", reply_type);
        return;
    }
    
    char *status = strtok(NULL, " \n");

    if(status == NULL){
        fprintf(stderr, "O servidor mandou uma resposta incompleta\n");
        return;
    }

    if(strcmp(status, "NOK") == 0){
        fprintf(stdout, "Nenhum utilizador tem esse ficheiro disponível ! \n");
        return;
    }

    if(strcmp(status, "ERR") == 0){
        fprintf(stderr, "o ficheiro informado está incorreto, por favor conferir o nome e extensão do ficheiro ! \n");
        return;
    }

    if(strcmp(status, "OK") != 0){ // Significa que o status é diferente de OK
        fprintf(stderr, "Status diferente de OK: %s \n", status);
        return;
    }

    char *uid = strtok(NULL, " \n");

    if(uid == NULL){
        fprintf(stdout, "Não foram recebidos campos (peer) do DS \n");
        return;
    }

    int contador = 0;

    while(uid != NULL){
        contador ++;

        char *tamanho = strtok(NULL, " \n");

        char *label = strtok(NULL, " \n");

        char *data = strtok(NULL, " \n");

        char *disponibilidade = strtok(NULL, " \n");


        // Verificamos se alguma das informações faltaram
        if(tamanho == NULL || label == NULL || data == NULL || disponibilidade == NULL){
            fprintf(stderr, "Informação incompleta para o utilizador: %s !\n", uid);
            return;
        }

        fprintf(stdout, "%d. Utilizador: %s\n", contador, uid);

        fprintf(stdout, " Tamanho: %s bytes\n", tamanho);
        fprintf(stdout, " Label: %s\n", label);
        fprintf(stdout, " Data: %s\n", data);
        fprintf(stdout, " Disponibilidade: %s\n", disponibilidade);
        
        fprintf(stdout, "\n");

        uid = strtok(NULL, " \n");
    }
    

}

void cmd_publish(char *linha, char *comando, char *uid_logged, char *password_logged, int sockfd, struct sockaddr_in *ds_addr, int *its_logged){

    if(*its_logged == 0){
                fprintf(stderr, "Erro: nenhum usuário está logado\n");
            } else{
                char filename[26];
                char label[22];

                int n = sscanf(linha, "%29s %25s %21s", comando, filename, label);

                if(n!= 3){
                    fprintf(stderr, "Erro: comando publish precisa de filename e label\n");
                }
                // Também precisaremos saber se os valores digitados são nos intervalos 1-24 e 1-20 caracteres
                // strlen devolve o comprimento em quanidade de caracteres que uma string tem.
                else if (strlen(filename) > 24 || strlen(label) > 20){
                    fprintf(stderr, "Erro: filename deve ter no máximo 24 caracteres e label no máximo 20 caracteres\n");
                } 
                else if (strlen(filename) < 5 || filename[strlen(filename) - 4] != '.'){
                    fprintf(stderr, "Erro: filename inválido. Formato: nome.ext, até 24 caracteres; nome com letras, dígitos, '-' ou '_'; extensão com 3 letras ou dígitos.\n");
                } else{

                int filename_valido = 1;
                int label_valido = 1;
                
                // verificando se os caracteres do filename antes do ponto são alfanuméricos ou são '-', '_'
                for(int i = 0; i < strlen(filename) - 4; i++){
                    if(isalnum(filename[i]) == 0 && filename[i] != '-' && filename[i] != '_'){
                        filename_valido = 0;
                        break;
                    }
                }
                
                // verificando se os caracteres do filename após do ponto (extension do ficheiro, o type) são alfanuméricos.
                for(int i = strlen(filename) - 3; i < strlen(filename); i++){
                    if(isalnum(filename[i]) == 0){
                        filename_valido = 0;
                        break;
                    }
                }

                // verificando se os caracteres do label são alfanuméricos ou são '-', '_'
                for(int i = 0; i < strlen(label); i++){
                    if(isalnum(label[i]) == 0 && label[i] != '-' && label[i] != '_'){
                        label_valido = 0;
                        break;
                    }
                }

                if(filename_valido == 0){
                    fprintf(stderr, "Erro: filename inválido. Formato: nome.ext, até 24 caracteres; nome com letras, dígitos, '-' ou '_'; extensão com 3 letras ou dígitos.\n");
                } else if(label_valido == 0){
                    fprintf(stderr, "Erro: label inválido. Formato: label, até 20 caracteres; nome com letras, dígitos, '-' ou '_'.\n");
                } else{
                    struct stat file_info;


                    if (stat(filename, &file_info) == -1) {
                        fprintf(stderr, "Erro: O ficheiro não existe ou o acesso ao ficheiro foi negado (Permission denied).\n");  
                    } else{
                        if(!S_ISREG(file_info.st_mode)){
                            fprintf(stderr, "Erro: Este ficheiro não é um file que pode ser publicado (Ex. Diretório) .\n");
                        } else if(file_info.st_size > 10000000){
                            fprintf(stderr, "Erro: O tamanho de este ficheiro não pode ser supérior a 10,000,000 bytes.\n");
                        } else{
                            char mensagem[76];

                            sprintf(mensagem, "PUB %s %s %s %lld %s\n", uid_logged, password_logged, filename, (long long) file_info.st_size, label);

                            int bytes_enviados = sendto(sockfd, mensagem, strlen(mensagem), 0, (struct sockaddr *)ds_addr, sizeof(*ds_addr));

                            if (bytes_enviados == -1) {
                                fprintf(stderr, "Erro ao enviar mensagem para o DS\n");
                                return;
                            }

                            char resposta[64];

                            struct sockaddr_in from_addr;
                            socklen_t from_len = sizeof(from_addr); 

                            int bytes_recebidos = recvfrom(sockfd, resposta, sizeof(resposta) - 1, 0,(struct sockaddr *)&from_addr, &from_len);

                            if (bytes_recebidos == -1){
                                fprintf(stderr, "Erro ao receber a resposta do DS\n");
                            } else{
                                resposta[bytes_recebidos] = '\0';

                                char tipo_resposta[20]; 
                                char status[16]; 

                                sscanf(resposta, "%s %s", tipo_resposta, status);

                                if(strcmp(status, "OK") == 0){ // Caso publish efetuado com sucesso
                                    printf("publish efetuado com sucesso!\n");
                                }
                                else if (strcmp(status, "NLG") == 0) { // Caso o usuário não esteja logado
                                    printf("Erro: usuário não estava logado.\n");
                                }

                                else if (strcmp(status, "UNR") == 0) { // Caso o usuário não esteja registrado
                                    printf("Erro: usuário não registrado.\n");
                                }
                                else if (strcmp(status, "WRP") == 0) { // Caso a senha seja incorreta
                                    printf("Erro: senha incorreta.\n");
                                } else if(strcmp(status, "NOK") == 0){
                                    printf("Erro: O publish não foi efetuado com sucesso por causa de um erro.\n");
                                }
                                // Se status não bater com nenhum desses casos acima, entramos nesse else
                                else {
                                    printf("Resposta inesperada do servidor: %s\n", resposta);
                                }
                            }
                        }

                    }
                }
                
                }
            }

}


// Função que vai administrar os comandos do user recebe essas três variáveis
// sockfd -> File descriptor do socket UDP usado para falar com o DS (Directory Server)
// ds_addr -> Ponteiro para a struct com o endereço (IP + Porta) do DS (Directory Server)
// peerport -> String com a porta TCP deste User (Enviada no LIN)
void user_interface (int sockfd, struct sockaddr_in *ds_addr, char *peerport){

    // Iniciando a interação do usuário com o terminal.

    // Váriaveis que vão armazenar o User Id e password caso tenha alguem logado
    char uid_logged[7] = "";
    char password_logged[9] = "";
    int its_logged = 0; // Se estiver logado é 1 se não é 0

    char linha[126]; // Buffer onde a linha digitada pelo usuário será guardada.

    int flag = 1; // Variável para controle do loop

      while (flag){

        char *fgets_return = fgets(linha, sizeof(linha), stdin); // Pega o que foi digitado com a função fgets()

        // Em caso de erro do fgets temos que tratar OU Em caso de Cntrl + D do usuário
        if (fgets_return == NULL){ // Em caso de erro ele devolve NULL
            fprintf(stderr, "Fim do input, encerrando o programa ...\n");
            break; // Quebra o while
            // Evitamos o loop infinito.
        }

        // Precisamos tratar o fato de se o usuário inserir mais de 125 caracteres, se sim apagarmos da memória

        if(strchr(linha, '\n') == NULL){ // Isso quer dizer que, se depois de percorrer os 126 chars, não encontrarmos o quebra linha o string reacher devolve NULL
            // Limpeza
            int c;
            // O getchar vai ler cada caractere e tirar da stdin, assim limpamos. Já que o comando real já foi copiado para linha
            while ((c = getchar()) != '\n' && c != EOF); // getchar tira um caractere do stdin e devolve ele. Continua até chegar no quebra linha ('\n')

            fprintf(stderr, "Erro: a linha de interação foi longa de mais, por favor inserir uma linha com no máximo 125 caracteres !\n");
            continue; // reinicia o while 
        }

        // Agora vamos fazer o scan do que foi digitado no terminal, para saber como agir.

        // Extraindo a primeira palavra, que de acordo com o enunciado é o nosso comando.
        char comando[30]; 
        
        // Os três argumentos de sscanf
        // 1 -> Declaramos a string onde vamos extrair os dados que o user digitou, no caso (linha).
        // 2 -> "%s" -> significa o formato do scan, leia uma palavra, quando chegar a um espaço pare.
        // 3 -> para onde o resultado vai ser guardado, no caso (comando)

       int n_comando = sscanf(linha, "%29s", comando);
        // n_comando retorna 1 se conseguiu extrair uma palavra com sucesso.
        // Retorna 0 (linha vazia/só espaços) ou -1/EOF (erro de leitura) caso contrário.
        // Por isso checamos "!= 1": qualquer coisa diferente de sucesso completo
        // significa que "comando" não foi preenchido de forma confiável nessa iteração.
        if(n_comando != 1){
            continue; // Pula pro início do while, sem processar um comando inválido/vazio
        }

        

        // Tratando o primeiro comando

        // Utilizamos o metodo strcmp (string comparation)
        // Ele recebe dois atributos, as duas strings que vamos comparar.
        // Se as strings forem iguais o metodo devolve 0.

        if(strcmp(comando, "login") == 0){ // Ou seja, comando digitado foi login.

            cmd_login(linha, comando,  uid_logged, password_logged, sockfd, ds_addr, peerport, &its_logged);

        }

        else if(strcmp(comando, "logout") == 0){ // Ou seja, comando digitado foi logout.

            cmd_logout(uid_logged,password_logged,sockfd, ds_addr,&its_logged);
    }
    

        else if(strcmp(comando, "unregister") == 0){ // Ou seja, comando digitado foi unregistrer

            cmd_unregister(uid_logged, password_logged, sockfd, ds_addr, &its_logged);

        }
        else if(strcmp(comando, "exit") == 0){ // Ou seja, comando digitado foi exit.

            // Se tivermos alguém com sessão iniciada não podemos só sair, precisamos antes fazer logout
            if(its_logged != 0){
                fprintf(stderr, "Antes de sair executar o logout !\n");

            }else{
                
                flag = 0; // Comando dado foi de exit então saimos do loop
            }
        }else if(strcmp(comando, "list") == 0){

            cmd_list(sockfd, ds_addr);

        }else if(strcmp(comando, "versions") == 0){

            cmd_versions(linha, comando, ds_addr);

        }else if(strcmp(comando, "publish") == 0){
            cmd_publish(linha, comando, uid_logged, password_logged, sockfd, ds_addr, &its_logged);
        }
        else{
            fprintf(stderr, "Comando desconhecido: %s\n", comando);
        }


    }
}

// argc -> quantidade de argumentos (contando o nome do programa)
// argv -> array de strings com cada argumento
int main(int argc, char *argv[]){

    int opt; // Variável que vai guardar, a cada interação do loop qual flag foi encontrada(O char 'm', 'n', 'p' ou -1 quando acabar)

    // Três ponteiros para string inicializados como NULL (Ainda não tem valor atribuido). Depois nos permite checar se a flag foi passada ou não.
    char *peerport = NULL;
    char *dsip = NULL;
    char *dsport = NULL;

    // Ciclo do while opt = getopt(argc, argv, "m:n:p:") -> chama getopt(), que processa uma flag por vez e guarda o resultado em opt
    while((opt = getopt(argc, argv, "m:n:p:")) != -1){ // O while continua enquanto o opt não for -1, getopt() retorna -1 quando já processou todas as flags disponíveis.

        switch(opt){ // Baseado em qual flag foi encontrada entramos no switch case.

            case 'm': // Se a flag for 'm' guarda o valor associado a ela (que está em optarg) dentro de peerport.
                peerport = optarg;
                break;

            case 'n': // Se a flag for 'n' guarda o valor associado a ela (que está em optarg) dentro de dsip.
                dsip = optarg;
                break;
            
            case 'p': // Se a flag for 'p' guarda o valor associado a ela (que está em optarg) dentro de dsport.
                dsport = optarg;
                break;
            
            // Se getopt() encontrar alguma flag não reconhecida ele cai aqui imprimindo um erro.
            default:
                fprintf(stderr, "Uso: %s -m peerport [-n DSIP] [-p DSport]\n", argv[0]);
                exit(1);

        }

    }
    
    // Peerport não pode ser nulo.
    if (peerport == NULL) {
        fprintf(stderr, "Erro: -m (peerport) é obrigatório\n");
        exit(1);
        }
    
    // Checagem se DSIP é nulo, no enunciado é possível que seja nulo, então o progama pode receber nulo, mas se receber nulo, atribui os valores padrão.
    if (dsip == NULL){
        dsip = "193.136.138.142";
    }

    // Checagem se DSPORT é nulo, no enunciado é possível que seja nulo, então o progama pode receber nulo, mas se receber nulo, atribui os valores padrão.
    if(dsport == NULL){
        dsport = "59000";
    }


    // Declarando a struct que vai guardar o enderço do DS
    struct sockaddr_in ds_addr; 

    memset(&ds_addr, 0, sizeof(ds_addr)); // Zera toda a struct antes de preencher, (Evita o lixo na memória)

    ds_addr.sin_family = AF_INET; // Diz que é um endereço IPv4

    ds_addr.sin_port = htons(atoi(dsport)); // Converte a porta (string -> int (atoi) -> network byte order (htons é o que converte para o padrão utilizado mundialmente))

    inet_pton(AF_INET, dsip, &ds_addr.sin_addr); // // converte o IP em formato texto ("193.136.138.142") pro formato binário da struct

    // Criando o socket

    // Armazena o file descriptor do socket na variável sockfd
    // socket() tem três argumentos. 
    // 1 -> Qual o sistema de endereçamento usaremos, nesse caso IPv4 por isso AF_INET
    // 2 -> Qual o tipo de comunicação "Quero um socket que manda datagramas" -> Pacotes independentes, sem conexão estabelecida, sem garantia de entrega ou ordem. MODELO UDP
    // 3 -> Protocolo específico dentro da família+tipo escolhidos. Nesse caso usamos o ("0" -> O sistema escolherá automáticamente o protocolo padrão pra essa combinação)
    // Resultando no protocolo UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
    
    // Verificação se houve erro na criação do socket.
    if(sockfd == -1){
        fprintf(stderr, "Erro ao criar socket\n");
        exit(1);
    }

    // Chamada da função user interface.
    user_interface(sockfd, &ds_addr, peerport);

}