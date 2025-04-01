// Application layer protocol implementation

#include "../include/application_layer.h"
#include "../include/link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename){

    LinkLayer connectionParameters;
    //verificar se a serial port foi devidamente copiada
    strncpy(connectionParameters.serialPort, serialPort, sizeof(connectionParameters.serialPort) - 1);
    connectionParameters.serialPort[sizeof(connectionParameters.serialPort) - 1] = '\0';  
    connectionParameters.role = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    int showStatistics = llopen(connectionParameters);

    if(showStatistics < 0){
        perror("Connection error\n");
        return;
    }
    printf("Opened\n");

    if(connectionParameters.role == LlTx){ //se for transmissor

        //abrir ficheiro em read-binary
        FILE *openFile = fopen(filename, "rb");
        if(openFile == NULL){  
            perror("Error: Unable to locate file\n");
            return;
        }

    
        fseek(openFile, 0L, SEEK_END);//vai ate ao fim do ficheiro
        long int file_size = ftell(openFile);//calcula tamanho fiheiro
        long int static_file_size= file_size;
        fseek(openFile, 0L, SEEK_SET);//volta a posicao de inicio

        unsigned int controlPacketLength;

        //start packet
        const int fileSizeFieldlength_start = 1;//tamanho do espaco p tamanho do ficheiro
        const int fileNamelength_start = strlen(filename);//tamanho nome do ficheiro
        controlPacketLength = 1 + 2 + fileSizeFieldlength_start + 2 + fileNamelength_start;//calcula tamanho control packet
        unsigned char *startPacket = (unsigned char *)malloc(controlPacketLength);

        unsigned int start_pos = 0;
        startPacket[start_pos++] = 1; 
        startPacket[start_pos++] = 0;//identificador file size
        startPacket[start_pos++] = fileSizeFieldlength_start;

        //coloca tamanho do ficheiro no control packet
        for(unsigned char index = 0; index < fileSizeFieldlength_start; index++){
            startPacket[2 + fileSizeFieldlength_start - index] = file_size& 0xFF;
            file_size>>= 8;
        }

        start_pos += fileSizeFieldlength_start;
        startPacket[start_pos++] = 1;  
        startPacket[start_pos++] = fileNamelength_start; 
        memcpy(startPacket + start_pos, filename, fileNamelength_start); 

        //manda start control packet
        if(llwrite(startPacket, controlPacketLength) == -1){
            perror("Abort: Start packet transmission failed\n");
            return;
        }

        unsigned char sequenceNumber = 0;
        unsigned char *bufferContent = (unsigned char *)malloc(sizeof(unsigned char) * static_file_size+ 50);
        fread(bufferContent, sizeof(unsigned char), static_file_size, openFile);//le ficheiro p memoria

        long int bytes_remaining = static_file_size;//qnts bytes faltam enviar
        int offset = 0;

        //loop p mandar ficheiro em chunks
        while(bytes_remaining > 0){

            //determina tamanho da prox chunk
            int chunk_size = bytes_remaining > (long int) (MAX_PAYLOAD_SIZE/2)-4 ? (MAX_PAYLOAD_SIZE/2)-4 : bytes_remaining;
            unsigned char *chunk_buf = (unsigned char *)malloc(chunk_size);
            memcpy(chunk_buf, bufferContent + offset, chunk_size);//copia chunk do ficheiro
            int dataPacket_length = 1 + 1 + 2 + chunk_size;//calcula tamanho data packet

            unsigned char *dataPacket = (unsigned char *)malloc(dataPacket_length);
            dataPacket[0] = 2;  //2 significa data packet
            dataPacket[1] = sequenceNumber;
            dataPacket[2] = chunk_size >> 8 & 0xFF;//upper byte chunk size
            dataPacket[3] = chunk_size & 0xFF;//lower byte chunk size
            memcpy(dataPacket + 4, chunk_buf, chunk_size);//copia chunk p packet

            if(llwrite(dataPacket, dataPacket_length) == -1){
                perror("Abort: Data packet transmission failed\n");
                return;
            }

            bytes_remaining -= (long int) (MAX_PAYLOAD_SIZE/2)-4;//diminui nr bytes q faltam
            offset += chunk_size;  //manda ponteiro p frente no ficheiro
            sequenceNumber++; 
        }

        //end packet
        const int fileSizeField_length_end = 1;
        const int fileName_length_end = strlen(filename);
        controlPacketLength = 1 + 2 + fileSizeField_length_end + 2 + fileName_length_end;
        unsigned char *end_Packet = (unsigned char *)malloc(controlPacketLength);

        unsigned int end_pos = 0;
        end_Packet[end_pos++] = 3;//3 significa end packet
        end_Packet[end_pos++] = 0;//identificador file size
        end_Packet[end_pos++] = fileSizeField_length_end;

        for(unsigned char index = 0; index < fileSizeField_length_end; index++){
            end_Packet[2 + fileSizeField_length_end - index] = file_size& 0xFF;
            file_size>>= 8;
        }

        end_pos += fileSizeField_length_end;
        end_Packet[end_pos++] = 1;//identificador file name 
        end_Packet[end_pos++] = fileName_length_end; 
        memcpy(end_Packet + end_pos, filename, fileName_length_end);
        
        if(llwrite(end_Packet, controlPacketLength) == -1){
            perror("Abort: End packet transmission failed\n");
            return;
        }

        if(fclose(openFile) != 0){
            perror("Error closing input file");
            return ;
        }
        llclose(showStatistics);
        printf("\nSuccessfully Transfered - No Errors\n");
    }

    if (connectionParameters.role == LlRx) {//se for recetor

        unsigned char *incoming_Packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
        int received_length = -1;
        printf("Preparing to receive...\n");
        received_length = llread(incoming_Packet);//le primeiro packet
        if(received_length < 0){
            perror("Abort: Problem Reading\n");
            return;
        }

        FILE *closeFile = fopen(filename, "wb+");//abre ficheiro p escrever o recebido 
        if(closeFile == NULL){
            perror("Error opening file");
            return;
        }
        
        //loop p receber data
        while(1){
            int trans_tries = connectionParameters.nRetransmissions;
            while(((received_length = llread(incoming_Packet)) < 0) && (trans_tries > 1)){//le smp ate receber um packet valido
                trans_tries--;
            }

            if(received_length == 0){
                break; //sai se ja nao houver mais packets
            }
            
            if(received_length < 0){
                llclose(-1);
                perror("Abort: Problem Reading\n");
                return;

            }else if((incoming_Packet[0] != 1) && (incoming_Packet[0] != 3)){//verifica se n e start e end packet
                fwrite(incoming_Packet + 4, sizeof(unsigned char), received_length - 4, closeFile);//escreve data no ficheiro
            }else{
                break;
            }
        }
        free(incoming_Packet);
        incoming_Packet = NULL;
        llclose(showStatistics);
        printf("\nSuccessfully Transfered - No Errors\n");
        if (fclose(closeFile) != 0) {
            perror("Error closing output file");
            return ;
        }
    }
}