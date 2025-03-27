// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_PAYLOAD_SIZE 1000
#define MAX_CONTROL_PACKET_SIZE 512
#define MAX_DATA_PACKET_SIZE 1024
#define MAX_FILENAME_SIZE 255


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    //Esta camada serve para gerir a transferência de ficheiros, utilizando a DLL. Devemos começar com o llopen(), para inicializar a conecção. Depois para enviar um ficheiro ou recebê-lo através da função llwrite() e llread(). Para fechar a conexão utilizamos o llclose()
    
    //Preciso de ir buscar primeiro os parametros iniciais 
    LinkLayer connectionParameters;
    
    //Estamos a copiar a string serialPort para o campo serialPort, connection.Parameters é a string de destino, onde queremos armazenar o nome da porta serial, o serialPort é onde está o nome da serialPort e o sizeof(..)-1 garante que não copiamos mais caracteres do que a array suporta 
    strncpy(connectionParameters.serialPort, serialPort, sizeof(connectionParameters.serialPort)-1);
    //Estamos a ver qual é o papel que está na variavel role, ou seja, se é transmissor ou recetor
    connectionParameters.role = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;
    
    int fd = llopen(portNumber; connectionParameters);
    
    if( fd < 0){
      perror("Erro na connecção\n");
      return;
    }
    printf("Ligou\n");
    
    //Se o role for transmissor, vamos abrir o ficheiro
    if(connectionParameters.role == LlTx){
      FILE *File = fopen(filename, "rb");
      //Ver se o ficheiro não está vazio
      if(File == NULL){
        perror("Erro: Não tem nada no ficheiro");
        return;
      }
      //Vamos calcular o tamanho do File, o 0L diz que não se mexe no file, ele só vai para a posição SEEK_END
      fseek(File, 0L ,SEEK_END);
      long int File_size = ftell(File);
      fseek(File,0L,SEEK_SET); //Volta para a posição de inicio
      
      unsigned int controlPacket_length;//tamanho do packet de controlo  
      
      //Inicializar as varíaveis do control packet para o start packet
      const int fileSizeFieldlength_start =1; //tamanho do field file size
      const int fileNamelength_start = strlen(filename);//tamanho do filename
      controlPacket_length = 1 + 2 + fileSizeFieldlength_start + 2 + fileNamelength_start;
      
      //Control packet, está dentro do start packet
      unsigned char start_Packet[MAX_CONTROL_PACKET_SIZE] = {0};
      
      int start_Pos = 0;
      start_Packet[start_Pos++] = 2; //Identificador que diz se vai iniciar ou terminar, START = 2---> slides
      start_Packet[start_Pos++] = 0; //Indica se é o file size-->0 ou se é o filename-->1
      start_Packet[start_Pos++] = fileSizeFieldlength_start; //tamanho do do file size 
      
      //inserir no pack os valores 
      for(unsigned int index = 0; index < fileSizeFieldlength_start; index++){
        
        start_Packet[2 + fileSizeFieldlength_start - index] = File_size & 0xFF; //Obtém os 8 bits menos significativos, ou seja, guarda dos menos significativos para os mais significativos
        File_size >>= 8; //Dá shift de 8 em 8 bits
      }
      
      start_Pos += fileSizeFieldlength_start; //Temos de atualizar o valor da variavel da posição
      start_Packet[start_Pos++] = 1; //Identificar o filename field
      start_Packet[start_Pos++] = fileNamelength_start; //Tamanho do filename field
      memcpy(start_Packet+start_Pos, filename, fileNamelength_start);//Copiar o filename para o packet de controlo
      
      //Vamos enviar o start control packet
      if(llwrite(fd, start_Packet, (start_Pos+strlen(filename)), connectionParameters) == -1)   {
        perror("Erro na transmissão do START packet\n");
        return;
      }
      
      //Transmissão de dados
      unsigned char data_Packet[MAX_DATA_PACKET_SIZE] = {0};
      unsigned char sequence_Number = 0;
      long int bytes_Remaining = File_size;
      
      while(bytes_Remaining > 0){
        int chunk_size = (bytes_Remaining > MAX_DATA_PACKET_SIZE - 4) ? MAX_DATA_PACKET_SIZE - 4 : bytes_Remaining;
        unsigned char chunk_buffer[chunk_size] = {0};
        
        int dataPacket_length = 1 + 2 + chunk_size;
        data_Packet[0] = 1;//Identificador que diz se vai iniciar ou terminar, DATA  = 1---> slides
        data_Packet[1] = sequence_Number++; //numero da sequência 
        data_Packet[2] = (chunk_size >> 8) & 0xFF; //armazena os bits mais significativos
        data_Packet[3] = chunk_size & 0xFF; //armazena os bits menos significativos
        //memcpy(data_Packet + 4, chunk_buffer, chunk_size, File);
        
        //Envia o data packet
        if(llwrite(data_Packet, dataPacket_length) == -1){
          perror("Erro na transmissão do data packet\n");
          return;
        }
        bytes_Remaining -= chunk_size;
      }
      
      //Control packet fim 
      
      //Inicialização das variaveis do packet de controlo para o end packet
      const int fileSizeFieldlength_end =1; 
      const int fileNamelength_end = strlen(filename);
     controlPacket_length = 1 + 2 + fileSizeFieldlength_end + 2 + fileNamelength_end; //tamanho do packet de controlo
     unsigned char end_Packet[MAX_CONTROL_PACKET_SIZE]={0}; 
     
     unsigned int end_Pos = 0;
     
     end_Packet[end_Pos++] = 3; //Identificador do controlo packet end
     end_Packet[end_Pos++] = 0; //Identificador File size field 
     end_Packet[end_Pos++] = fileSizeFieldlength_end; //Tamanho do file size field
     
     //Inserir o file size no end packet
     for(unsigned char index = 0; index < fileSizeFieldlength_end; index++){
        
        end_Packet[2 + fileSizeFieldlength_end - index] = File_size & 0xFF; //Obtém os 8 bits menos significativos, ou seja, guarda dos menos significativos para os mais significativos
        
        File_size >>= 8; //dá shift de 8 bits 
     }
     end_Pos += fileSizeFieldlength_end;
     end_Packet[end_Pos++] = 1;//identificador que é do tipo fileName
     end_Packet[end_Pos++] = fileNamelength_end; //Tamnho do filename field
     memcpy(end_Packet+end_Pos, filename, fileNamelength_end); //copy the filename para o packet end
     
     //Envio do end control packet
     if(llwrite(end_Packet, controlPacket_length)){
      perror("Erro a enviar o end packet\n");
      return;
     }
     //Fechar o ficheiro
     
     if(fclose(File) != 0){
      perror("Erro a fechar o ficheiro\n");
      return;
     }
     
     llclose(fd); //vamos fechar o link
     printf("\nTransferência bem sucedida\n");
    }
    
    //Agora se o papel for do transmissor
    if(connectionParameters.role == LlRx){
      unsigned char incoming_Packet[MAX_PAYLOAD_SIZE] = {0}; //array com o tamanho correto
      int received_length = -1;
      
      received_length = llread(fd,incoming_Packet);
      
      if(received_length < 0){
        perror("Problema a ler\n");
        return;
      }
      FILE *output_File = fopen(filename, "wb+"); //abrimos o ficheiro para escrever a data que recebemos
      
      if(output_File == NULL){
        perror("Erro ao abrir o ficheiro");
        return;
      }
      //Vamos fazer loop para receber a data 
      while(1){
        int transmissions_tries = connectionParameters.nRetransmissions; //Vamos passar o numero de retransmissões para a variavel transmissions_tries
        
        while(((received_length = llread(incoming_Packet)) < 0) && (transmissions_tries >1)){ // Este loop serve para ler até receber um packet válido
     transmissions_tries --;     
        }
        if(received_length == 0) break;
        if(received_length < 0){
          llclose(1);
          perror("Problema a ler");
          return;
        }else if((incoming_Packet[0] != 1) && (incoming_Packet[0] != 3)){//Vamos ver agora os data packets que não são nem START nem END
          fwrite(incoming_Packet + 4, sizeof(unsigned char), received_length -4, output_File); // Escreve a data no file e na posição 4 e retira ao tamanho do que é recebido
          
        } else{
          break;
          }
        }
        free(incoming_Packet);
        llclose(fd);
        printf("\n Transferiu com sucesso - sem erros");
        if(fclose(output_File)!=0){
          perror("Erro a fechar o output file");
          return;
        }
      
      }
    
    }
