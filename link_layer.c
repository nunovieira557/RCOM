// Link layer protocol implementation

#include "link_layer.h"

//Valores Header
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

int retransmissions = 0;
int timeout = 0;

int retransmissions_total = 0;
int timeout_total = 0;

//Variaveis de controlo para saber se os frames Is são I0 ou I1

unsigned char cont_Tx = 0;
unsigned char cont_Rx = 0;

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
                estado = FLAG_RCV;
            }
            break;
        case FLAG_RCV:
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
          estado = FLAG_RCV;
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
          estado = FLAG_RCV;
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
          frame[frame_length++]=no_byte
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
      BCC2_RCV = frame[frame_length-1 -1]
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
    if(cont_Tx == 0){
      RR = C_RR0;
      REJ = C_REJ1;
    }
    if(cont_Rx == 1){
      RR = C_RR1;
      REJ = C_REJ0;
    }
    
    if(BCC2 ==checkBCC2){
      estado = STP; //Se é válido o packet
      unsigned char frame_leitura[BUF_SIZE] = {0};
      //Enviar o frame de reconhecimento 
      frame_leitura[0] = FLAG;
      frame_leitura[1] = A_RT;
      frame_leitura[2] = RR;
      frame_leitura[3] = frame_leitura[1] ^ frame_leitura[2]; //Calcula o BCC1 para o reconhecimento
      frame_leitura[4] = FLAG;
      write(fd, frame_leitura, BUF_SIZE); //Enviar o frame de reconhecimento 
      cont_Tx = (cont_Rx + 1) % 2
      
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
    memcpy(buffer, destuffedData, destuffedLength);
    
    return destuffed_length;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd, int showStatistics)
{
    int state = START;
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

            int bytes = write(fd, disc, BUF_SIZE);

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
                        state = START:
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

            int bytes = write(fd, secdisc, BUF_SIZE);

            printf("%s\n", "Second DISC sent - Rx");

            state = START;
            alarmclosed = FALSE; //Recebemos DISC, fechamos alarme
        }//este DISC n precisa de alarm pq o recetor só envia DISC qnd o 1º foi recebido, logo a desconexao ja foi reconhecida
        
        int byte = read(fd, &byte_rd, 1);

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
                        state = START:
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

            int bytes = write(fd, ua, BUF_SIZE);

            printf("%s\n", "UA sent - Tx"); //n se usa alarme porque a conexao ja esta praticamente encerrada, ua e uma confirmacao
        }

        int byte = read(fd, &byte_rd, 1);

        while((state != STP) && (alarmclosed == FALSE) && (set.role == LlRx)){ 
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
                        state = START:
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
        
        retransmissions --;
        retransmissions_total ++;
    }

    if(showStatistics){
        printf("\n --- Communications Statistics --- \n");
        printf("Number of retransmissions: %d\n", retransmissions_total); 
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

