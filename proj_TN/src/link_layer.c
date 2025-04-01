// Link layer protocol implementation

#include "link_layer.h"
#include <unistd.h>
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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int fd = -1; //descritor do file para abrir a serial port
struct termios oldtio; //definicoes da serial port para restaurar quando esta a fechar  

/////////////////////////////////////////////////////////////////////
//Funcoes para ler e escrever os bytes, e para abrir a serial port //
/////////////////////////////////////////////////////////////////////

//abrir e configurar a serial port
//se houver erro retorna -1

int openSerialPort(const char *serialPort, int baudRate){
    int open_flags = O_RDWR | O_NOCTTY | O_NONBLOCK; //RDWR-leitura e escrita, NOCTTY-processo continua sem ser afetado por eventos na porta, NONBLOCK-abertura sem bloqueio
    fd = open(serialPort, open_flags);

    if(fd < 0){
        perror(serialPort);
        return -1;
    }

    //guarda as definicoes atuais da port
    if(tcgetattr(fd, &oldtio) == -1){
        perror("tcgetattr");
        return -1;
    }
    
    struct termios newtio;
    memset(&newtio, 0, sizeof(newtio)); //limpar struct p novos settings

    newtio.c_cflag = CS8 | CLOCAL | CREAD; //taxa de transmissao, 8 bits, ignora modem e permite rececao.
    newtio.c_iflag = IGNPAR; //ignora erros de paridade
    newtio.c_oflag = 0; //envia dados como estao

    newtio.c_lflag = 0; //byte by byte
    newtio.c_cc[VTIME] = 1; //funcao read espera 100ms por dados ate retornar
    newtio.c_cc[VMIN] = 0; //retorna mesmo que n receba nenhum byte
    
    tcflush(fd, TCIOFLUSH);//descarta dados antes de iniciar comunicacao

    //colocar novas definicoes de port
    if(tcsetattr(fd, TCSANOW, &newtio) == -1){
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    //agora vamos limpar a flag O_NONBLOCK para garantir leituras
    open_flags ^= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, open_flags) == -1){
        perror("fcntl");
        close(fd);
        return -1;
    }

    return fd;
}

//restaurar definicoes antigas p fechar port
int close_Port(void){
    if(tcsetattr(fd, TCSANOW, &oldtio) == -1){
        perror("tcsetattr");
        return -1;
    }

    return close(fd);
}

//espera p receber byte da port 
//return -1 p erro, 0 se n recebeu nada, 1 se recebeu.
int readByte_Port(unsigned char *byte){
    return read(fd, byte, 1);
}

//usada quando e preciso ler mais q um byte(ex: SET, DISC, etc)
int readBytes(unsigned char *byte, int numBytes){
    return read(fd, byte, numBytes);
}

//escreve numBytes na serialport
//return -1 p erro, nmro de bytes escritos p sucesso.
int write_Bytes(unsigned char *bytes, int numBytes){
    return write(fd, bytes, numBytes);
}

int alarmEnabled = FALSE;
int alarmCount = 0;

//extern int sequenceNumber;

unsigned char counter_Tx = 0;
unsigned char counter_Rx = 1;

int reTrans = 0;
int timeout = 0;
int reTrans_total=0;
int timeout_total=0;
int frames_global=0;

LinkLayer set;

void alarmHandler(int signal){
    alarmEnabled = FALSE;
    alarmCount++;
    timeout_total++;
    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters){

    //abre serial port c parametros fornecidos
    if(openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0){
        return -1;
    }

    (void)signal(SIGALRM, alarmHandler);

    //inicializa estado da maquina p comecar
    StateMachine state = START;

    set = connectionParameters;

    LinkLayerRole role = connectionParameters.role;
    reTrans = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    //variavel p guardar o q recebemos
    unsigned char buf;

    //verifica se e transmissor ou recetor
    switch(role){
        case LlTx:
            (void) signal(SIGALRM, alarmHandler);
            while(reTrans != 0 && state != STOP){
                unsigned char frame[BUF_SIZE] = {0};
                frame[0] = FLAG;
                frame[1] = A_TR;
                frame[2] = C_SET;
                frame[3] = frame[1] ^ frame[2];
                frame[4] = FLAG;

                write_Bytes(frame, BUF_SIZE);//manda SET p inicializar comunicacao
                alarm(timeout);//liga alarme qnd passar timeout
                alarmEnabled = TRUE;

                //espera por uma resposta ate timeout
                while(alarmEnabled == TRUE && state != STOP){
                    if(readByte_Port(&buf) > 0){
                        //maquina de estados p processar se a informacao recebida esta correta 
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
                                if(buf == (A_RT ^ C_UA)){
                                    state = BCC1_OK;
                                }else if(buf == FLAG){
                                    state = FLAG_RCV;
                                }else{
                                state = START;
                                }   
                            break;

                            case BCC1_OK:
                                if(buf == FLAG){ 
                                    state = STOP;
                                }else{
                                    state = START;
                                }
                                break;

                            default:
                            break;
                        }
                    }
                }
                reTrans--;
                reTrans_total++;
            }

            //retorna -1 se n se estabeleceu conexao
            if(state != STOP){
                return -1;
            }
            break;

        case LlRx:
            while(state != STOP){
                if(readByte_Port(&buf) > 0){
                    //maquina de estados p processar se a informacao recebida esta correta
                    switch(state){
                        case START:
                            if(buf == FLAG){
                                state = FLAG_RCV;
                            }
                        break;

                        case FLAG_RCV:
                            if(buf == A_TR){ 
                                state = A_RCV;
                            }else if(buf != FLAG){
                                state = START;
                            }
                        break;

                        case A_RCV:
                            if(buf == C_SET){
                                state = C_RCV;
                            }else if(buf == FLAG){
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                        break;

                        case C_RCV:
                            if(buf == (A_TR ^ C_SET)){
                                state = BCC1_OK;
                            }else if(buf == FLAG){
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                        break;

                        case BCC1_OK:
                            if(buf == FLAG){
                                state = STOP;
                            }else{
                                state = START;
                            }
                        break;

                        default:
                            return -1;
                        break;
                    }
                }
            }

            //manda UA para confirmar conexao completa
            unsigned char frame[BUF_SIZE] = {0};
            frame[0] = FLAG;
            frame[1] = A_RT;
            frame[2] = C_UA;
            frame[3] = frame[1] ^ frame[2];
            frame[4] = FLAG;
            write_Bytes(frame, BUF_SIZE);
            break;

        default:
            return -1;
        break;
    }
    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {

    //aloca memoria p frame e para os controlos(dai o +6) 
    unsigned char *framecompl = (unsigned char *)malloc(MAX_PAYLOAD_SIZE + 6);

    //header
    framecompl[0] = FLAG;
    framecompl[1] = A_TR;
    framecompl[2] = (counter_Tx  == 0) ? C_I0 : C_I1;
    framecompl[3] = framecompl[1] ^ framecompl[2];

    int bufSize_grow = bufSize;
    int chars_written = 0;
    int counter = 0;
    int pos = 4;

    //processa o frame p poder ter caracteres especiais(byte stuffing)
    while(bufSize_grow > counter){
        if(buf[counter] == FLAG){
            framecompl[pos++] = ESC;
            framecompl[pos] = 0x5E;
            chars_written += 2;
            bufSize_grow++;
        }else if(buf[counter] == ESC){
            framecompl[pos++] = ESC;
            framecompl[pos] = 0x5D;
            chars_written += 2;
            bufSize_grow++;
        }else{
            framecompl[pos] = buf[counter];
            chars_written++;
        }
        counter++;
        pos++;
    }

    //calcula BCC2 p verificar data q recebemos
    unsigned char BCC2 = buf[0];
    for(int i = 1; i < bufSize; i++){
        BCC2 ^= buf[i];
    }

    //byte stuffing p BCC2
    if(BCC2 == FLAG){
        framecompl[counter + 4] = ESC;
        framecompl[counter + 5] = 0x5E;
        framecompl[counter + 6] = FLAG;
    }else if(BCC2 == ESC){
        framecompl[counter + 4] = ESC;
        framecompl[counter + 5] = 0x5D;
        framecompl[counter + 6] = FLAG;
    }else{
        framecompl[counter + 4] = BCC2;
        framecompl[counter + 5] = FLAG;
    }

    (void)signal(SIGALRM, alarmHandler);

    int rejected = FALSE;
    int accepted = FALSE;
    int counterwrite = 0;

    reTrans = set.nRetransmissions;
    timeout = set.timeout;

    //loop ate nr de retransmissoes for menor q limite de retrans
    while(counterwrite < reTrans){
        //define alarme p disparar apos timeout segundos e ativa alarme
        alarm(timeout);
        //timeout_total++;
        alarmEnabled = TRUE;
        rejected = FALSE;
        accepted = FALSE;

        //envia frame
        write_Bytes(framecompl, chars_written + 6);

        unsigned char frame = 0;
        unsigned char recon = 0; //armazena se frame foi aceite ou rejeitado

        //espera p confirmacao ou rejeicao ate alarme disparar 
        while((alarmEnabled == TRUE) && (rejected == FALSE) && (accepted == FALSE)){

            StateMachine state = START;

            //verifica data recebida
            while((state != STOP) && (alarmEnabled == TRUE)){
                if(readByte_Port(&frame) > 0){
                    switch(state){
                        case START:
                            if(frame == FLAG){
                                state = FLAG_RCV;
                            }
                        break;

                        case FLAG_RCV:
                            if(frame == A_RT){
                                state = A_RCV;
                            }else if(frame == FLAG){
                                state = FLAG_RCV;
                            }
                        break;

                        case A_RCV:
                            if((frame == C_RR0) || (frame == C_RR1) || (frame == C_REJ0) || (frame == C_REJ1)){
                                state = C_RCV;
                                recon = frame;
                            }else if(frame == FLAG){
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                        break;
                        
                        case C_RCV:
                            if(frame == (A_RT ^ recon)){
                                state = BCC1_OK;
                            }else if(frame == FLAG){
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                        break;
                        
                        case BCC1_OK:
                            if(frame == FLAG){
                                state = STOP;
                            }else{
                                state = START;
                            }
                        break;
                        
                        default:
                        break;
                    }
                }
            }

            if(!recon){
                continue;
            }else if((recon == C_REJ0) || (recon == C_REJ1)){
                rejected = TRUE;
            }else if((recon == C_RR0) || (recon == C_RR1)){
                accepted = TRUE;
                counter_Tx  = (counter_Tx  + 1) % 2;
            }else{
                continue;
            }
        }

        //para retransmissoes se for aceite
        if(accepted){
            break;
        }
        counterwrite++;

        //restart no alarme se for preciso
        if(alarmEnabled == FALSE){
            alarm(timeout);
            //timeout_total++;
            alarmEnabled = TRUE;
        }
    }

    //limpar o frame p receber novos valores
    free(framecompl);
    framecompl = NULL;
    printf("Data Write: %d Bytes\n", chars_written);

    if(accepted){
        return chars_written;
    }else{
        llclose(-1);
        return -1;
    }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char frame_read; 
    unsigned char control_received; //guarda C_I0 ou C_I1
    int block = FALSE;//determina o comportamento do programa quando um byte de escapatoria e encontrado

    int index = 0;   
    StateMachine state = START;    

    while(state != STOP){
        if(readByte_Port(&frame_read) > 0){
            switch(state){
                case START:
                    if(frame_read == FLAG){
                        state = FLAG_RCV;
                    }
                break;

                case FLAG_RCV:
                    if(frame_read == A_TR){
                        state = A_RCV;
                    }else if(frame_read == FLAG){
                        state = FLAG_RCV;
                    }else{
                        state = START;
                    }
                break;

                case A_RCV:
                    if((frame_read == C_I0) || (frame_read == C_I1)){
                        control_received = frame_read;  
                        state = C_RCV;                
                    }else if(frame_read == FLAG){
                        state = FLAG_RCV;
                    }else{
                        state = START;
                    }
                break;

                case C_RCV:
                    if(frame_read == (A_TR ^ control_received)){
                        state = BCC1_OK;  
                    }else if(frame_read == FLAG){
                        state = FLAG_RCV;
                    }else {
                        state = START;
                    }
                break;

                case BCC1_OK:
                    if((frame_read == 0x5E) && (packet[index - 1] == ESC) && (!block)){
                        packet[index - 1] = FLAG;
                        block = FALSE;
                    }else if((frame_read == 0x5D) && (packet[index - 1] == ESC) && (!block)){
                        packet[index - 1] = ESC;
                        block = TRUE;
                    }else if(frame_read == FLAG){ //ultimo byte
                        index--; //volta atras p processar dados
                        unsigned char BCC2 = packet[index]; //ultimo dado
                        unsigned char BCC2_ACUM = packet[0]; //acumula verificacao de BCC2

                        int data_index = 1;

                        while(index > data_index){
                            BCC2_ACUM ^= packet[data_index]; 
                            data_index++;
                        }

                        unsigned char RR;  
                        unsigned char REJ; 

                        if(counter_Rx == 0){
                           RR = C_RR0;
                           REJ = C_REJ1;
                        }
                        if(counter_Rx == 1){
                           RR = C_RR1;
                           REJ = C_REJ0;
                        }

                        if(BCC2 == BCC2_ACUM){
                            state = STOP;  
                            unsigned char frameread[BUF_SIZE] = {0};
                            frameread[0] = FLAG;
                            frameread[1] = A_RT;
                            frameread[2] = RR;
                            frameread[3] = frameread[1] ^ frameread[2];
                            frameread[4] = FLAG;
                            write_Bytes(frameread, BUF_SIZE); 
                            counter_Rx = (counter_Rx + 1) % 2;
                        }else{
                            printf("BCC2 ERROR!\n");
                            unsigned char frameread[BUF_SIZE] = {0};
                            frameread[0] = FLAG;
                            frameread[1] = A_RT;
                            frameread[2] = REJ; //rejeicao pq BCC2 n e valido
                            frameread[3] = frameread[1] ^ frameread[2];
                            frameread[4] = FLAG;
                            write_Bytes(frameread, BUF_SIZE);
                            return -1; 
                        }

                    }else{
                        block = FALSE; 
                        packet[index] = frame_read; //guardamos byte atual
                        index++; 
                    }
                break;

                case STOP:
                break; //p sair do loop

                default:
                break;
            }
        }
    }

    printf("Data Read: %d Bytes\n", index); 
    return index;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    StateMachine state = START;
    (void) signal(SIGALRM, alarmHandler); 
    unsigned char byterd;

    while((state != STOP) && (reTrans > 0)){
        if(set.role == LlTx){
            //transmissor manda DISC
            unsigned char frame_close[BUF_SIZE] = {0};
            frame_close[0] = FLAG;
            frame_close[1] = A_TR;
            frame_close[2] = C_DISC;
            frame_close[3] = frame_close[1] ^ frame_close[2]; 
            frame_close[4] = FLAG;
            write_Bytes(frame_close, BUF_SIZE); 
            printf("%s\n", "First Disc Tx Sent");
        }
        alarm(timeout); //retransmitimos caso DISC se perca, pois e este que inicia a desconexao
        //timeout_total++; 

        int alarmEnabledClose = TRUE;

        //recetor recebe DISC
        while((state != STOP) && (alarmEnabledClose == TRUE) && (set.role == LlRx)){
            if((readByte_Port(&byterd) > 0)){
                switch(state){
                    case START:
                        if(byterd == FLAG){
                            state = FLAG_RCV; 
                        }
                    break;

                    case FLAG_RCV:
                        if(byterd == A_TR){
                            state = A_RCV;
                        }else if(byterd != FLAG){
                            state = START;
                        }
                    break;

                    case A_RCV:
                        if(byterd == C_DISC){
                            state = C_RCV; 
                        }else if(byterd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;

                    case C_RCV:
                        if(byterd == (A_TR ^ C_DISC)){
                            state = BCC1_OK; 
                        }else if(byterd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;

                    case BCC1_OK:
                        if(byterd == FLAG){
                            state = STOP;
                        }else{
                            state = START;
                        }
                    break;

                    default:
                    break; 
                }
            }
        }

        //DISC recebido, agora enviamos como recetor
        if((state == STOP) && (set.role == LlRx)){
            printf("%s\n", "First disc rx received");
            unsigned char secnddisc[BUF_SIZE] = {0};
            secnddisc[0] = FLAG;
            secnddisc[1] = A_TR;
            secnddisc[2] = C_DISC; 
            secnddisc[3] = secnddisc[1] ^ secnddisc[2]; 
            secnddisc[4] = FLAG; 
            write_Bytes(secnddisc, BUF_SIZE); 
            printf("%s\n", "Second Disc Rx Sent");
            state = START; 
            alarmEnabledClose = FALSE;//recebemos DISC, fechamos alarme
        }//este DISC n precisa de alarm pq o recetor so envia DISC qnd o 1 foi recebido, logo a desconexao ja foi reconhecida

        while((state != STOP) && (alarmEnabledClose == TRUE) && (set.role == LlTx)){//Recebemos e verificamos DISC como transmissor
            if((readByte_Port(&byterd) > 0)){
                switch(state){
                    case START:
                        if(byterd == FLAG){
                            state = FLAG_RCV; 
                        }
                    break;

                    case FLAG_RCV:
                        if(byterd == A_TR){
                            state = A_RCV; 
                        }else if(byterd != FLAG){
                            state = START;
                        }
                    break;

                    case A_RCV:
                        if(byterd == C_DISC){
                            state = C_RCV; 
                        }else if(byterd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;
                    
                    case C_RCV:
                        if(byterd == (A_TR ^ C_DISC)){
                            state = BCC1_OK; 
                        }else if(byterd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;

                    case BCC1_OK:
                        if(byterd == FLAG){
                            state = STOP; 
                        }else{
                            state = START;
                        }
                    break;

                    default:
                    break; 
                }
            }
        }

        if((state == STOP) && (set.role == LlTx)){//DISC recebido, enviamos UA
            printf("%s\n", "Second disc tx received");
            unsigned char ua[BUF_SIZE] = {0};
            ua[0] = FLAG;
            ua[1] = A_TR;
            ua[2] = C_UA; 
            ua[3] = ua[1] ^ ua[2]; 
            ua[4] = FLAG; 
            write_Bytes(ua, BUF_SIZE); 
            printf("%s\n", "UA Tx Sent");
        }

        while((state != STOP) && (alarmEnabledClose == FALSE) && (set.role == LlRx)){//verifica ua
            if((readByte_Port(&byterd) > 0)){
                switch (state){
                    case START:
                        if(byterd == FLAG){
                            state = FLAG_RCV; 
                        }
                    break;

                    case FLAG_RCV:
                        if(byterd == A_TR){
                            state = A_RCV; 
                        }else if(byterd != FLAG){
                            state = START;
                        }
                    break;

                    case A_RCV:
                        if(byterd == C_UA){
                            state = C_RCV; 
                        }else if(byterd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;

                    case C_RCV:
                        if(byterd == (A_TR ^ C_UA)){
                            state = BCC1_OK; 
                        }else if(byterd == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                    break;

                    case BCC1_OK:
                        if(byterd == FLAG){
                            state = STOP; 
                        }else{
                            state = START;
                        }
                    break;

                    default:
                    break; 
                }
            }
        }

        reTrans--; 
        reTrans_total++; 
    }

    if(state == STOP){
        printf("%s\n", "Close Done");
    }
    if(state != STOP){
        printf("%s\n", "Error closing");
    }

    if (showStatistics) {
        printf("\n --- Communications Statistics --- \n");
        printf("Number of retransmissions: %d\n", reTrans_total);
        printf("Number of timeouts: %d\n", timeout_total);
    }
    int clstat = close_Port();
    return clstat;
}
