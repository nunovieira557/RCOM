// Link layer protocol implementation

#include "link_layer.h"

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
  alarmCount++;
  printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
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
    unsigned char buf 
    
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
          alarme(timeout);
          timeout_g++;
          alarmEnabled = TRUE;
          
          //Esperar pela resposta até ao timeout acabar
          
          while(alarmEnabled == TRUE && atual_estado != STOP){
             
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
                    estado_atual = FLAG_RCV;
                  }else{
                    estado_atual = START:
                  }
                  break;
                  
                  case C_RCV:
                  if(buf == (A_RT^C_UA)){
                    estado_atual = BCC_OK;
                  }else if(buf == FLAG){
                    estado_atual = FLAG_RCV;
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
          int byt = read(fd,&buff,1);
                            switch (state) {
                        case START:
                            if ( buff == FLAG){ 
                            estado_atual = FLAG_RCV;
                            }
                            break;
                        case FLAG_RCV:
                            if (buff == A_TR){
                            estado_atual = A_RCV;
                            }else if ( buff != FLAG){ 
                            estado_atual= START;
                            }
                            break;
                        case A_RCV:
                            if (buff == C_SET){
                            estado_atual = C_RCV;
                            }else if ( buff == FLAG){ 
                            estado_atual = FLAG_RCV;
                            }else{
                            estado_atual = START;
                            }
                            break;
                        case C_RCV:
                            if ( buff == (A_TR ^ C_SET)){
                            estado_atual = BCC_OK;
                            }
                            else if ( buff== FLAG){ 
                            estado_atual = FLAG_RCV;
                            }else{
                            estado_atual = START;
                            }
                            break;
                        case BCC_OK:
                            if ( buff == FLAG){
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
    frame_compelto[3] = frame_completo[1] ^ frame_completo[2] //BCC1
    
    int especial_cab = 4; //variável que serve para guardar no frame e é tbm importante para a posição no frame 
    int chars_escritas = 0;// variável para contar quantos bytes foram escritos 
    
    //Byte stuffing para os dados
    for(int i=0; i < length; i++){
      if(buffer[i]==FLAG){
      //Se for uma flag vamos colocar um ESC e o octeto escapotório
          frame_completo[especial_cab++] = ESC;
          frame_completo[especial_cab++] = 0x5E;
          chars_escritas +=2;
      }else if(bufferr[i] == ESC){
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
        frame_complete[especial_cab++] = BCC2;
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
      unsigned char reconhecimento 0; //variavel que nos diz os reconhecimentos RR ou REJ
      
      //Vamos fazer este loop até ao timeout acabar, ou então quando há rejeição
      while(alarmeEnabled == TRUE && rejeitou == FALSE && aceitou == FALSE){
          
          int estado = START;
            //Máquina de estados com os reconhecimentos: RR0,RR1 ou REJ0 ou REJ1 
            while(estado !=STOP && alarmEnabled == TRUE){
             
             int byte = read(fd,&buffer,1);
                switch (state) {
                        case START:
                            if ( buffer == FLAG){ 
                              estado = FLAG_RCV;
                            }
                            break;
                        case FLAG_RCV:
                            if ( buffer == A_RT){
                              estado = A_RCV;
                            }else if ( buffer == FLAG){
                              estado = FLAG_RCV;
                            }
                            break;
                        case A_RCV:
                            if (buffer == C_RR0 || buffer == C_RR1 || buffer == C_REJ0 ||buffer == C_REJ1) {
                                estado = C_RCV;
                                reconhecimento = buffer;
                            
                            } else if (buffer == FLAG){ state = FLAG_RCV;
                            
                            }else{ 
                            
                              state = START;
                            
                            }break;
                        case C_RCV:
                            if (buffer == (A_RT ^ reconhecimento)){ 
                              estado = BCC1_OK;
                            }else if (buffer == FLAG){ 
                              estado = FLAG_RCV;
                            }else{ 
                              estado = START;
                            }break;
                            
                        case BCC1_OK:
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
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd)
{
    // TODO

    return 1;
}
