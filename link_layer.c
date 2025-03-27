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

//Valores Header
#define FLAG 0x7E
#define ESC 0x7D
#define A_TR 0x03 // Address field in frames that are commands sent by the Transmitter or replies sent by the Receiver (A_ER)
#define A_RT 0x01 // Address field in frames that are commands sent by the Receiver or replies sent by the Transmitter (A_RE)
#define C_SET 0x03
#define C_UA 0x07
#define C_I0 0x00
#define C_I1 0x40
#define C_RR0 0x05
#define C_RR1 0x85
#define C_REJ0 0x01
#define C_REJ1 0x81
#define C_DISC 0x0B

//Estados
#define START 0
#define Flag_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define STP 5


// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BAUDRATE B38400

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int STOP = FALSE;

//Serial port settings
static struct termios oldtio, newtio;

int alarmEnabled = FALSE;
int alarmcount = 0;

int reTransmissions = 0;
int timeout = 0;

int reTransmissions_g = 0;
int timeout_g = 0;

//Variaveis de controlo para saber se os frames Is são I0 ou I1

unsigned char cont_Tx = 0;
unsigned char cont_Rx = 0;


//Podiamos fazer uma função para uma máquina de estados, se tiver tempo faço

//Implementação do alarme 
void alarmHandler (int signal){
  alarmEnabled=FALSE;
  alarmcount++;
  printf("Alarm #%d\n", alarmcount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(int portNumber, LinkLayer connectionParameters)
{
    char serialPortName[20];
    sprintf(serialPortName, "/dev/ttyS%d", portNumber);
    
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }
    
    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        return -1;
    }
    
     // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Non-Blocking

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }
    
    //Set o sinal do alarm
    (void)signal(SIGALRM,alarmHandler);
    
    //Inicialização da máquina de estados
    int estado_atual = START;
    
    //Set condições
    LinkLayer role = connectionParameters.role;
    reTransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    
    //variavel para enviar os valores dos bytes 
    unsigned char frame[BUF_SIZE] = {0};
    
    //Variavel para confirmar os valores
    unsigned char buf;
    
    //máquina de estados para o papel, ou seja, se é recetor ou transmissor
    
    switch(role){
      case LlTx:
        (void)signal(SIGALRM,alarmHandler);
          while(reTransmissions != 0 && estado_atual != STP){
          unsigned char setframe[5] = {0};
          setframe[0] = FLAG;
          setframe[1] = A_TR;
          setframe[2] = C_SET;
          setframe[3] = frame[1] ^ frame[2];
          setframe[4] = FLAG;
          
          //Enviar o SET frame para inicializar
          write(fd, setframe, BUF_SIZE);
          alarm(timeout);
          timeout_g++;
          alarmEnabled = TRUE;
          
          //Esperar pela resposta até ao timeout acabar
          
          while(alarmEnabled == TRUE && estado_atual != STOP){
             
             int byte = read(fd,&buf,1);
              
              switch(estado_atual){
                  case START:
                  if(buf == FLAG){
                  estado_atual = Flag_RCV;
                  }
                  break;
                  
                  case Flag_RCV:
                  if(buf == A_RT){
                    estado_atual = A_RCV;
                  }else if(buf != FLAG){
                    estado_atual = START;
                  }
                  break;
                  
                  case A_RCV: 
                  if(buf == C_UA){
                  estado_atual = C_RCV;
                  }else if(buf == FLAG){
                    estado_atual = Flag_RCV;
                  }else{
                    estado_atual = START;
                  }
                  break;
                  
                  case C_RCV:
                  if(buf == (A_RT^C_UA)){
                    estado_atual = BCC_OK;
                  }else if(buf == FLAG){
                    estado_atual = Flag_RCV;
                  }else{
                    estado_atual = START;
                  }
                    break;
                
                  case BCC_OK:
                  if(buf == FLAG){
                    estado_atual = STOP;
                  }else{
                    estado_atual = START;
                  }
                  
                  case STP:
                    break;
              }
          
          }
          reTransmissions--;
          reTransmissions_g++;
          }
          
          //Retorna erro se a connecção falhar
         if(estado_atual !=STP){
          return -1;
         } 
         break;
         //Papel do recetor 
         
         case LlRx:
         while(estado_atual != STOP){
          int byt = read(fd,&buf,1);
                            switch (estado_atual) {
                        case START:
                            if ( buf == FLAG){ 
                            estado_atual = Flag_RCV;
                            }
                            break;
                        case Flag_RCV:
                            if (buf == A_TR){
                            estado_atual = A_RCV;
                            }else if ( buf != FLAG){ 
                            estado_atual= START;
                            }
                            break;
                        case A_RCV:
                            if (buf == C_SET){
                            estado_atual = C_RCV;
                            }else if ( buf == FLAG){ 
                            estado_atual = Flag_RCV;
                            }else{
                            estado_atual = START;
                            }
                            break;
                        case C_RCV:
                            if ( buf == (A_TR ^ C_SET)){
                            estado_atual = BCC_OK;
                            }
                            else if ( buf== FLAG){ 
                            estado_atual = Flag_RCV;
                            }else{
                            estado_atual = START;
                            }
                            break;
                        case BCC_OK:
                            if ( buf == FLAG){
                            estado_atual = STOP;
                            }else{
                            estado_atual = START;
                            }
                            break;
                        case STP:
                            return -1;
                            break;
                    }
                }
                
                //Enviar o sinal UA para confirmar a conecção
                unsigned char setFrame[BUF_SIZE] ={0};
                setFrame[0] = FLAG;
                setFrame[1] = A_RT;
                setFrame[2] = C_UA;
                setFrame[3] = setFrame[1] ^ setFrame[2];
                setFrame[4] = FLAG;
                
                int bytes = write(fd, setFrame, 5);
                
                case STP:
                  return -1;
                break;
         
         }
       return 1;
    }

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd,char *buffer, int length, LinkLayer connectionParameters)
{
    if(buffer ==NULL){
      perror("Buffer está vazio");
      return -1;
    }
    
    //Tamanho do frame completo, o max_playload_size mais o cabeçalho 
    unsigned char frame_completo[MAX_PAYLOAD_SIZE +6];
    
    //configuração do cabeçalho do frame
    frame_completo[0] = FLAG;
    frame_completo[1] = A_TR;
    frame_completo[2] = (cont_Tx == 0) ? C_I0 : C_I1; //Estamos a ver qual é o valor dos frames se é I0 ou I1
    frame_completo[3] = frame_completo[1] ^ frame_completo[2]; //Cálculo do BCC1
    
    int especial_cab = 4; //variável que serve para guardar no frame e é tbm importante para a posição no frame 
    int chars_escritas = 0;// variável para contar quantos bytes foram escritos 
    
    //Byte stuffing para os dados
    for(int i=0; i < length; i++){
      if(buffer[i]==FLAG){
      //Se for uma flag vamos colocar um ESC e o octeto escapotório
          frame_completo[especial_cab++] = ESC;
          frame_completo[especial_cab++] = 0x5E;
          chars_escritas +=2;
      }else if(buffer[i] == ESC){
      //Se for um ESC, vamos colocar outro ESC e o octeto escapatório
          frame_completo[especial_cab++] = ESC;
          frame_completo[especial_cab++] = 0x5d;
          chars_escritas += 2;
      }else{
        frame_completo[especial_cab++] = buffer[i];
        chars_escritas++;
      }
    }
    //Calcular o BCC2
    unsigned char BCC2 = buffer[0];
      for(int i = 1; i < length; i++){
        BCC2 ^= buffer[i];
    }
    
    //Byte stuffing para BCC2
    if (BCC2 == FLAG) {
        frame_completo[especial_cab++] = ESC;
        frame_completo[especial_cab++] = 0x5E;
    } else if (BCC2 == ESC) {
        frame_completo[especial_cab++] = ESC;
        frame_completo[especial_cab++] = 0x5D;
    } else {
        frame_completo[especial_cab++] = BCC2;
    }
    
    //Adicionar a útima flag 
    frame_completo[especial_cab++] = FLAG;
    
    //Vamos puxar a função alarme para a retransmissão 
    
    (void)signal(SIGALRM, alarmHandler);
    
    //váriaveis necessárias para a retransmissão
    int rejeitou = FALSE , aceitou = FALSE , cont_write =0;//cont_write--> é utilizado para controlar o número de retransmissões no protocolo Stop-and-Wait 
    
    reTransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    
    //Loop para fazer retransmissão de acordo com o protocolo Stop-and-Wait
    while(cont_write < reTransmissions){
      alarm(timeout);
      timeout_g++;
      alarmEnabled = TRUE;
      rejeitou = FALSE;
      aceitou = FALSE;
      
      //Enviar os frames 
      write(fd, frame_completo, chars_escritas + 6);
      
      unsigned char buffer = 0; //variavel que nos diz o que tem dentro do buffer 
      unsigned char reconhecimento = 0; //variavel que nos diz os reconhecimentos RR ou REJ
      
      //Vamos fazer este loop até ao timeout acabar, ou então quando há rejeição
      while(alarmEnabled == TRUE && rejeitou == FALSE && aceitou == FALSE){
          
          int estado = START;
            //Máquina de estados com os reconhecimentos: RR0,RR1 ou REJ0 ou REJ1 
            while(estado !=STOP && alarmEnabled == TRUE){
             
             int byte = read(fd,&buffer,1);
                switch (estado) {
                        case START:
                            if ( buffer == FLAG){ 
                              estado = Flag_RCV;
                            }
                            break;
                        case Flag_RCV:
                            if ( buffer == A_RT){
                              estado = A_RCV;
                            }else if ( buffer == FLAG){
                              estado = Flag_RCV;
                            }
                            break;
                        case A_RCV:
                            if (buffer == C_RR0 || buffer == C_RR1 || buffer == C_REJ0 ||buffer == C_REJ1) {
                                estado = C_RCV;
                                reconhecimento = buffer;
                            
                            } else if (buffer == FLAG){ estado = Flag_RCV;
                            
                            }else{ 
                            
                              estado = START;
                            
                            }break;
                        case C_RCV:
                            if (buffer == (A_RT ^ reconhecimento)){ 
                              estado = BCC_OK;
                            }else if (buffer == FLAG){ 
                              estado = Flag_RCV;
                            }else{ 
                              estado = START;
                            }break;
                            
                        case BCC_OK:
                            if (buffer == FLAG){ 
                              estado = STP;
                            }else{ 
                            estado = START;
                            }break;
                        case STP:
                            break;
                    }
            }
            
            //Processo de reconhecimento
            if (!reconhecimento){ 
              continue;
            }else if ( reconhecimento == C_REJ0 || reconhecimento == C_REJ1){ 
            
            rejeitou = TRUE;
            
            }else if (reconhecimento == C_RR0 || reconhecimento == C_RR1) {
                aceitou = TRUE;
                cont_Tx= (cont_Tx + 1) % 2; //Serve para alternar entre 0 e 1
            } else continue;
      
      }
        if(aceitou == 1){
        break;
        }
        cont_write++;
        
        //Vamos dar RESET ao alarm se necessário 
        if(alarmEnabled == FALSE){
          alarm(timeout);
          timeout_g++;
          alarmEnabled = TRUE;
        }
    }
    
      //Vamos limpar o frame, para enviar outro frame 
      free(frame_completo);
      frame_completo = NULL;
      //debug
      printf("Data: %d Bytes\n", chars_escritas);
      if(aceitou == 1){
        return chars_escritas;
      }else{
        llclose(-1);
        return -1;
      }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(int fd, char *buffer)
{
  unsigned char frame[2*MAX_SIZE+6];
  unsigned char no_byte;
  int frame_length = 0;
  
  int estado = START;
  int run = TRUE;
  
  while(run){
    if(alarmEnabled == FALSE){
      alarm(3);
      alarmEnabled = TRUE;
    }
    
    int byte = read(fd, &no_byte,1);
    
    switch(estado){
      case START:
          if (no_byte == FLAG)
            {
                frame_length = 0;
                frame[frame_length++] = no_byte;
                estado = Flag_RCV;
            }
            break;
        case Flag_RCV:
        if(no_byte == FLAG){
          frame_length = 1;
        }else if(no_byte == A_RCV){
          frame[frame_length++] = no_byte;
          estado = A_RCV;
        }else{
          estado = START;
          frame_length = 0;
        }
        break;
        case A_RCV:
        if(no_byte == FLAG){
          estado = Flag_RCV;
          frame_length = 1;
        }else if(no_byte == C_I0 || no_byte == C_I1){
          frame[frame_length++] = no_byte;
          estado = C_RCV;
        }else{
          estado = START;
          frame_length = 0;
        }
        break;
        case C_RCV:
        if(no_byte == FLAG){
          estado = Flag_RCV;
          frame_length = 1;
        }else if(no_byte == (frame[1]^frame[2])){
          frame[frame_length++] = no_byte;
          estado = BCC_OK;
        }else{
          estado = START;
          frame_length = 0;
        }
        break;
        case BCC_OK:
        
        if(no_byte == FLAG){
          frame[frame_length++] = no_byte;
          estado = STP;
        }else{
          frame[frame_length++]=no_byte;
        }
        break;
        default:
        estado = START;
        break;
    }
  }
  //Precisamos de ler o que está no data field e o BCC2, mas precisamos de fazer destuffing primeiro
  unsigned char BCC2_RCV;
  int foi_BCC2_stuffed = FALSE;
    // frame_length -1 -2 é para chegar à posição onde se posiciona o ESC 
    if(frame[frame_length-1 -2] == ESC){ 
    // frame_length -1 -1 é para chegar à posição do BCC2
      BCC2_RCV = frame[frame_length-1 -1] ^0x20;
      foi_BCC2_stuffed = TRUE;
    }else{
      BCC2_RCV = frame[frame_length-1 -1];
    }
    int tamanho_data = frame_length - 6 - foi_BCC2_stuffed; //Tamanho da data excluindo FLAG, A, C,BCC1,FLAG e BCC2
    
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
    
    unsigned char RR; // variavel para reconhecimento positivo 
    unsigned char REJ;//variável para reconhecimento negativo
    
    if(cont_Tx == 0){
      RR = C_RR0;
      REJ = C_REJ1;
    }
    if(cont_Rx == 1){
      RR = C_RR1;
      REJ = C_REJ0;
    }
    
    if(BCC2_RCV ==checkBCC2){
      estado = STP; //Se é válido o packet
      unsigned char frame_leitura[BUF_SIZE] = {0};
      //Enviar o frame de reconhecimento 
      frame_leitura[0] = FLAG;
      frame_leitura[1] = A_RT;
      frame_leitura[2] = RR;
      frame_leitura[3] = frame_leitura[1] ^ frame_leitura[2]; //Calcula o BCC1 para o reconhecimento
      frame_leitura[4] = FLAG;
      write(fd, frame_leitura, BUF_SIZE); //Enviar o frame de reconhecimento 
      cont_Rx = (cont_Rx + 1) % 2;
      
    }//Se BCC2 estiver errado, envia rejeição
    else{
      unsigned char frame_leitura[BUF_SIZE] = {0};
      frame_leitura[0] = FLAG;
      frame_leitura[1] = A_RT;
      frame_leitura[2] = REJ;
      frame_leitura[3] = frame_leitura[1] ^ frame_leitura[2];//Cálculo de BCC1 para rejeição
      frame_leitura[4] = FLAG;
      write(fd, frame_leitura, BUF_SIZE);
      return -1;
    }
    
    //este comando copia a data para o output do buffer
    memcpy(buffer, destuffed_data, destuffed_length);
    
    return destuffed_length;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd)
{
    // TODO

    return 1;
}
