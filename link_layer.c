// Link layer protocol implementation

#include "../include/link_layer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define ESC 0x7D
#define A_TR 0x03 //comando mandado pelo transmissor ou resportas mandadas pelo recetor
#define A_RT 0x01 //comando mandado pelo recetor ou resportas mandadas pelo transmissor
#define C_SET 0x03 //mandado pelo transmissor para iniciar conexao
#define C_UA 0x07 //confirmacao de rececao de um frame de supervisao
#define C_I0 0x00 //inf frame 0
#define C_I1 0x40 //inf frame 1
#define C_RR0 0x05 //recetor manda a dizer q esta pronto para receber inf frame 0
#define C_RR1 0x85 //recetor manda a dizer q esta pronto para receber inf frame 1
#define C_REJ0 0x01 //recetor manda a dizer q rejeita inf frame 0
#define C_REJ1 0x81 //recetor manda a dizer q rejeita inf frame 1
#define C_DISC 0x0B //indica termino da conexao

//estados
#define START 0
#define FLAG_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define STP 5

#define BUF_SIZE 256

int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;

int retrans = 0;
int timeout = 0;
int retrans_total = 0; 
int timeout_total = 0; 
int state = START;

//Variaveis de controlo para saber se os frames Is são I0 ou I1
unsigned char cont_Tx = 0;
unsigned char cont_Rx = 0;

LinkLayer set;

//Implementacao do alarme 
void alarmHandler (int signal){
    alarmEnabled=FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters){   
    char serialPortName[20];
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    
    if(fd < 0){
        perror(serialPortName);
        exit(-1);
    }

    //Set o sinal do alarm
    (void) signal(SIGALRM, alarmHandler);

    int retrans = connectionParameters.nRetransmissions;
    int timeout = connectionParameters.timeout;

    //variavel para enviar os valores dos bytes 
    unsigned char frame[BUF_SIZE] = {0};
    
    //Variavel para confirmar os valores
    unsigned char buf;
    
    //máquina de estados para o papel, ou seja, se é recetor ou transmissor
    switch(connectionParameters.role){
        case LlTx:
            (void) signal(SIGALRM, alarmHandler);

            while((connectionParameters.nRetransmissions != 0) && (state != STP)){
                unsigned char set[5] = {0};
                set[0] = FLAG;
                set[1] = A_TR;
                set[2] = C_SET;
                set[3] = set[1] ^ set[2];
                set[4] = FLAG;
          
                //Envia o SET frame para inicializar
                int bytes = write(fd, set, 5);

                alarm(timeout);
                timeout_total++;
                alarmEnabled = TRUE;

                //Esperar pela resposta até ao timeout acabar
                while((alarmEnabled == TRUE) && (state != STP)){
                    int byte = read(fd, &buf, 1);
                
                    switch(state){
                        case START:
                            if(buf == FLAG){
                                state = FLAG_RCV;
                            }
                        break;
                    
                        case FLAG_RCV:
                            if(buf == A_RT){
                                state = A_RCV;
                            }else if(buf != FLAG){
                                state = START;
                            }
                        break;
                    
                        case A_RCV: 
                            if(buf == C_UA){
                                state = C_RCV;
                            }else if(buf == FLAG){
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                        break;
                    
                        case C_RCV:
                            if(buf == (A_RT^C_UA)){
                                state = BCC_OK;
                            }else if(buf == FLAG){
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                        break;
                    
                        case BCC_OK:
                            if(buf == FLAG){
                                state = STP;
                            }else{
                                state = START;
                            }
                    
                        case STP:
                        break;
                    }
          
                }
                retrans--;
                retrans_total++;
            }
          
            //Retorna erro se a connecção falhar
            if(state != STP){
                return -1;
            } 
            break;
            //Papel do recetor 
         
        case LlRx:
            while(state != STOP){
                int byte = read(fd, &buf, 1);

                    switch (state) {
                        case START:
                            if (buf == FLAG){ 
                                state = FLAG_RCV;
                            }
                        break;

                        case FLAG_RCV:
                            if (buf == A_TR){
                                state = A_RCV;
                            }else if ( buf != FLAG){ 
                                state = START;
                            }
                        break;

                        case A_RCV:
                            if (buf == C_SET){
                                state = C_RCV;
                            }else if (buf == FLAG){ 
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                        break;

                        case C_RCV:
                            if (buf == (A_TR ^ C_SET)){
                                state = BCC_OK;
                            }else if (buf == FLAG){ 
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                        break;

                        case BCC_OK:
                            if (buf == FLAG){
                                state = STP;
                            }else{
                                state = START;
                            }
                        break;

                        case STP:
                            return -1;
                        break;
                            
                    }
            }
                
            //Enviar o sinal UA para confirmar a conecção
            unsigned char ua[BUF_SIZE] = {0};
            ua[0] = FLAG;
            ua[1] = A_RT;
            ua[2] = C_UA;
            ua[3] = ua[1] ^ ua[2];
            ua[4] = FLAG;
                
            int bytes = write(fd, ua, 5);
                
        default:
            return -1;
        break;
         
    }
    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize)
{
    if(buf == NULL){
        perror("Buffer está vazio");
        return -1;
    }
      
    //Tamanho do frame completo, max_playload_size mais o cabeçalho 
    unsigned char frame[MAX_PAYLOAD_SIZE + 6];
      
    //configuração do cabeçalho do frame
    frame[0] = FLAG;
    frame[1] = A_TR;
    frame[2] = (cont_Tx == 0) ? C_I0 : C_I1; //Estamos a ver qual é o valor dos frames se é I0 ou I1
    frame[3] = frame[1] ^ frame[2]; 
      
    int pos = 4; //variavel que serve para guardar posição no frame 
    int char_written = 0; //variavel para contar quantos bytes foram escritos 
      
    //Byte stuffing para os dados
    for(int i=0; i < bufSize; i++){
        if(buf[i] == FLAG){
            //Se for uma flag vamos colocar um ESC e o octeto escapatorio
            frame[pos] = ESC;
            frame[pos++] = 0x5E;
            char_written += 2;
        }else if(buf[i] == ESC){
            //Se for um ESC, vamos colocar outro ESC e o octeto escapatório
            frame[pos] = ESC;
            frame[pos++] = 0x5d;
            char_written += 2;
        }else{
          frame[pos] = buf[i];
          char_written++;
        }
    }

    //Calcular o BCC2
    unsigned char BCC2 = buf[0];
    
    for(int i=1; i < bufSize; i++){
        BCC2 ^= buf[i];
    }
    
    //Byte stuffing para BCC2
    if (BCC2 == FLAG) {
        frame[pos++] = ESC;
        frame[pos++] = 0x5E;
    } else if (BCC2 == ESC) {
        frame[pos++] = ESC;
        frame[pos++] = 0x5D;
    } else {
        frame[pos++] = BCC2;
    }
    
    //Adicionar a útima flag 
    frame[pos++] = FLAG;
    
    //Vamos puxar a função alarme para a retransmissão 
    (void)signal(SIGALRM, alarmHandler);
    
    //variaveis necessarias para a retransmissao
    int reject = FALSE, accept = FALSE, cont_write =0;//cont_write-> utilizado para controlar o numero de retransmissoes no protocolo Stop-and-Wait 
    
    retrans = set.nRetransmissions;
    timeout = set.timeout;
    
    //Loop para fazer retransmissão de acordo com o protocolo Stop-and-Wait
    while(cont_write < retrans){
        alarm(timeout);
        timeout_total++;
        alarmEnabled = TRUE;
        reject = FALSE;
        accept = FALSE;
      
        //Enviar os frames 
        write(fd, frame, char_written + 6);
      
        unsigned char buf = 0; //variavel que nos diz o que tem dentro do buffer 
        unsigned char recon = 0; //variavel que nos diz os reconhecimentos RR ou REJ
      
        //Vamos fazer este loop até ao timeout acabar, ou então quando há rejeição
        while((alarmEnabled == TRUE) && (reject == FALSE) && (accept == FALSE)){
          
            //Máquina de estados com os reconhecimentos: RR0,RR1 ou REJ0 ou REJ1 
            while((state != STOP) && (alarmEnabled == TRUE)){
                int byte = read(fd, &buf, 1);

                    switch (state) {
                        case START:
                            if (buf == FLAG){ 
                                state = FLAG_RCV;
                            }
                        break;

                        case FLAG_RCV:
                            if (buf == A_RT){
                                state = A_RCV;
                            }else if ( buf == FLAG){
                                state = FLAG_RCV;
                            }
                        break;

                        case A_RCV:
                            if ((buf == C_RR0) || (buf == C_RR1) || (buf == C_REJ0) || (buf == C_REJ1)) {
                                state = C_RCV;
                                recon = buf;  
                            } else if (buf == FLAG){
                                state = FLAG_RCV;
                            }else{ 
                                state = START;
                            }
                        break;
                        
                        case C_RCV:
                            if (buf == (A_RT ^ recon)){ 
                                state = BCC_OK;
                            }else if (buf == FLAG){ 
                                state = FLAG_RCV;
                            }else{ 
                                state = START;
                            }
                        break;
                        
                        case BCC_OK:
                            if (buf == FLAG){ 
                                state = STP;
                            }else{ 
                                state = START;
                            }
                        break;
                        
                        case STP:
                        break;
                    }
            }
        
            //Processo de reconhecimento
            if (!recon){ 
                continue;
            }else if ((recon == C_REJ0) || (recon == C_REJ1)){ 
                reject = TRUE;
            }else if ((recon == C_RR0) || (recon == C_RR1)) {
                accept = TRUE;
                cont_Tx= (cont_Tx + 1) % 2; //Serve para alternar entre 0 e 1
            } else 
                continue;
      
        }
        if(accept == 1){
            break;
        }

        cont_write++;
        
        //Vamos dar RESET ao alarm se necessário 
        if(alarmEnabled == FALSE){
          alarm(timeout);
          timeout_total++;
          alarmEnabled = TRUE;
        }
    }
    
    //Vamos limpar o frame, para enviar outro frame 
    memset(frame, 0, (MAX_PAYLOAD_SIZE + 6));
    
    //debug
    printf("Data: %d Bytes\n", char_written);
    
    if(accept == 1){
        return char_written;
    }else{
        llclose(fd, 1);
        return -1;
    }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(int fd, unsigned char *buffer)
{
    unsigned char frame[2 * (BUF_SIZE - 1) + 6];
    unsigned char no_byte;
    int frame_length = 0;
    int run = TRUE;
  
    while(run){
        if(alarmEnabled == FALSE){
            alarm(3);
            alarmEnabled = TRUE;
        }
    
        int byte = read(fd, &no_byte, 1);
    
        switch(state){
            case START:
                if(no_byte == FLAG){
                    frame_length = 0;
                    frame[frame_length++] = no_byte;
                    state = FLAG_RCV;
                }
            break;
            
            case FLAG_RCV:
                if(no_byte == FLAG){
                    frame_length = 1;
                }else if(no_byte == A_RCV){
                    frame[frame_length++] = no_byte;
                    state = A_RCV;
                }else{
                    state = START;
                    frame_length = 0;
                }
            break;

            case A_RCV:
                if(no_byte == FLAG){
                    state = FLAG_RCV;
                    frame_length = 1;
                }else if((no_byte == C_I0) || (no_byte == C_I1)){
                    frame[frame_length++] = no_byte;
                    state = C_RCV;
                }else{
                    state = START;
                    frame_length = 0;
                }
            break;

            case C_RCV:
                if(no_byte == FLAG){
                    state = FLAG_RCV;
                    frame_length = 1;
                }else if(no_byte == (frame[1]^frame[2])){
                    frame[frame_length++] = no_byte;
                    state = BCC_OK;
                }else{
                    state = START;
                    frame_length = 0;
                }
            break;

            case BCC_OK:
                if(no_byte == FLAG){
                    frame[frame_length++] = no_byte;
                    state = STP;
                }else{
                    frame[frame_length++] = no_byte;
                }   
                break;

            case STP:
                state = START;
            break;
        }
    }
    
    //Precisamos de ler o que esta no data field e o BCC2, mas precisamos de fazer destuffing primeiro
    unsigned char BCC2_RCV;
    int BCC2_stuffed = FALSE;
    
    // frame_length -1 -2 é para chegar à posição onde se posiciona o ESC 
    if(frame[frame_length - 1 - 2] == ESC){ 
        // frame_length -1 -1 é para chegar à posição do BCC2
        BCC2_RCV = frame[frame_length - 1 - 1] ^ 0x20;
        BCC2_stuffed = TRUE;
    }else{
        BCC2_RCV = frame[frame_length-1 -1];
    }
  
    int tamanho_data = frame_length - 6 - BCC2_stuffed; //Tamanho da data excluindo FLAG, A, C,BCC1,FLAG e BCC2
  
    unsigned char destuffed_data[tamanho_data];
    int destuffed_length = 0;
    
    //Vamos detetar o byte stuffing, mas temos que encontrar primeiro onde aparece a data e antes do BCC2
    for(int i = 4; i < tamanho_data+4 ; i++){
        if(frame[i] == ESC){ //Byte stuffing foi detetado, ou seja, o byte stuffing está feito no llwrite(), então temos que procurar o ESC ---> significa que existiu byte stuffing
            destuffed_data[destuffed_length++] = frame[++i] ^0x20;
        }else {
            destuffed_data[destuffed_length++] = frame[i];
        }
    }
  
    //Confirmar se o BCC2 está correto, mas temos que calcular primeiro o BCC2
    unsigned char checkBCC2 = 0;
  
    for(int i=0; i < destuffed_length; i++){
        checkBCC2 ^= destuffed_data[i]; 
    }

    unsigned char RR;   //variavel p reconhecimento positivo
    unsigned char REJ; //variavel p reconhecimento negativo

    if(cont_Tx == 0){
        RR = C_RR0;
        REJ = C_REJ1;
    }

    if(cont_Rx == 1){
        RR = C_RR1;
        REJ = C_REJ0;
    }
    
    if(BCC2_RCV == checkBCC2){
        state = STP; //Se e velido o packet
        
        unsigned char frame[BUF_SIZE] = {0};
        
        //Enviar o frame de reconhecimento 
        frame[0] = FLAG;
        frame[1] = A_RT;
        frame[2] = RR;
        frame[3] = frame[1] ^ frame[2]; //Calcula o BCC1 para o reconhecimento
        frame[4] = FLAG;
        
        int bytes = write(fd, frame, 5); //Enviar o frame de reconhecimento 
        
        cont_Tx = (cont_Rx + 1) % 2;
      
    }else{ //Se BCC2 estiver errado, envia rejeição
        unsigned char frame[BUF_SIZE] = {0};
        frame[0] = FLAG;
        frame[1] = A_RT;
        frame[2] = REJ;
        frame[3] = frame[1] ^ frame[2];//Calculo de BCC1 para rejeição
        frame[4] = FLAG;

        int bytes = write(fd, frame, 5);

        return -1;
    }
    
    //este comando copia a data para o output do buffer
    memcpy(buffer, destuffed_data, destuffed_length);
    
    return destuffed_length;

}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd, int showStatistics)
{
    (void) signal(SIGALRM, alarmHandler);
    unsigned char byte_rd;
   
    while((state != STP) && (retrans > 0)){
        if(set.role == LlTx){
            unsigned char disc[BUF_SIZE] = {0};
            disc[0] = FLAG;
            disc[1] = A_TR;
            disc[2] = C_DISC;
            disc[3] = disc[1]^disc[2];
            disc[4] = FLAG;

            int bytes = write(fd, disc, 5);

            printf("%s\n", "First DISC sent - Tx");
        }
        alarm(timeout); //retransmitimos caso DISC se perca, pois e este que inicia a desconexao
        timeout_total++;

        int alarmclosed = TRUE;
        int byte = read(fd, &byte_rd, 1);

        while((state != STP) && (alarmclosed == TRUE) && (set.role == LlRx)){
            if(byte_rd > 0){
                switch(state){
                    case START:
                        if(byte_rd == FLAG){
                            state = FLAG_RCV;
                        }
                    break;
                    
                    case FLAG_RCV:
                        if(byte_rd == A_TR){
                            state = A_RCV;
                        }else if(byte_rd != FLAG){
                            state = START;
                        }
                    break;
                    
                    case A_RCV: 
                        if(byte_rd == C_DISC){
                            state = C_RCV;
                        }else if(byte_rd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;
                    
                    case C_RCV:
                        if(byte_rd == (A_TR ^ C_DISC)){
                            state = BCC_OK;
                        }else if(byte_rd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;
                
                    case BCC_OK:
                        if(byte_rd == FLAG){
                            state = STP;
                        }else{
                            state = START;
                        }
                    
                    case STP:
                    break;
                }
            }
        }
        if((state == STP) && (set.role == LlRx)){ //DISC recebido, agora enviamos como recetor
            printf("%s\n", "First DISC received - Rx");
           
            unsigned char secdisc[BUF_SIZE] = {0};
            secdisc[0] = FLAG;
            secdisc[1] = A_RT;
            secdisc[2] = C_DISC;
            secdisc[3] = secdisc[1]^secdisc[2];
            secdisc[4] = FLAG;

            int bytes = write(fd, secdisc, 5);

            printf("%s\n", "Second DISC sent - Rx");

            state = START;
            alarmclosed = FALSE; //Recebemos DISC, fechamos alarme
        }//este DISC n precisa de alarm pq o recetor só envia DISC qnd o 1º foi recebido, logo a desconexao ja foi reconhecida
        
        byte = read(fd, &byte_rd, 1);

        while((state != STP) && (alarmclosed == TRUE) && (set.role == LlTx)){ //Recebemos e verificamos DISC como transmissor
            if(byte_rd > 0){
                switch(state){
                    case START:
                    if(byte_rd == FLAG){
                        state = FLAG_RCV;
                    }
                    break;
                    
                    case FLAG_RCV:
                    if(byte_rd == A_RT){
                        state = A_RCV;
                    }else if(byte_rd != FLAG){
                        state = START;
                    }
                    break;
                    
                    case A_RCV: 
                    if(byte_rd == C_DISC){
                        state = C_RCV;
                    }else if(byte_rd == FLAG){
                        state = FLAG_RCV;
                    }else{
                        state = START;
                    }
                    break;
                    
                    case C_RCV:
                        if(byte_rd == (A_RT ^ C_DISC)){
                            state = BCC_OK;
                        }else if(byte_rd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;
                
                    case BCC_OK:
                        if(byte_rd == FLAG){
                            state = STP;
                        }else{
                            state = START;
                        }
                    break;

                    case STP:
                    break;
                }
            }
        }
        if((state == STP) && (set.role == LlTx)){ //DISC recebido, enviamos UA 
            printf("%s\n", "Second DISC received - Tx");
           
            unsigned char ua[BUF_SIZE] = {0};
            ua[0] = FLAG;
            ua[1] = A_TR;
            ua[2] = C_UA;
            ua[3] = ua[1]^ua[2];
            ua[4] = FLAG;

            int bytes = write(fd, ua, 5);

            printf("%s\n", "UA sent - Tx"); //n se usa alarme porque a conexao ja esta praticamente encerrada, ua e uma confirmacao
        }

        byte = read(fd, &byte_rd, 1);

        while((state != STP) && (alarmclosed == FALSE) && (set.role == LlRx)){ //verifica ua
            if(byte_rd > 0){
                switch(state){
                    case START:
                        if(byte_rd == FLAG){
                            state = FLAG_RCV;
                        }
                    break;
                    
                    case FLAG_RCV:
                        if(byte_rd == A_TR){
                            state = A_RCV;
                        }else if(byte_rd != FLAG){
                            state = START;
                        }
                    break;
                    
                    case A_RCV: 
                        if(byte_rd == C_UA){
                            state = C_RCV;
                        }else if(byte_rd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;
                    
                    case C_RCV:
                        if(byte_rd == (A_TR ^ C_UA)){
                            state = BCC_OK;
                        }else if(byte_rd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;
                
                    case BCC_OK:
                        if(byte_rd == FLAG){
                            state = STP;
                        }else{
                            state = START;
                        }
                    
                    case STP:
                    break;
                }
            }
        } 
        retrans --;
        retrans_total ++;
    }

    if(showStatistics){
        printf("\n --- Communications Statistics --- \n");
        printf("Number of retransmissions: %d\n", retrans_total); 
        printf("Number of timeouts: %d\n", timeout_total);
    }

    if(state != STP){
        printf("%s\n", "Error closing.");
        return -1;
    }

    if(state == STP){
        printf("%s\n", "Closed");
    }

    return 1;
}
