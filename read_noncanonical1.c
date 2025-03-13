// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

#define START 0
#define Flag_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define STP 5

volatile int STOP = FALSE;




int main(int argc, char *argv[])

{
	int estado_atual = START;
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

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
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    unsigned char frame[5] = {0};
    int index = 0;
    
    
    while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        /*int bytes = read(fd, buf, BUF_SIZE);
        //buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
        
        for(int i=0 ;i<5 ;i++){
        
        printf("0x%02X\n", buf[i]);
        //if (buf[0] == 'z')*/
        
        int bytes = read(fd,buf,1);
        
        //printf("0x%02X\n", buf[0]); diz nos o que está na posição 0
        
        switch(estado_atual){
			case START:
			
			//printf("Estado START\n");
			
			if(buf[0]==0x7E){
				
				frame[index++]=buf[0];
				estado_atual=Flag_RCV;
				
				}else{
				
					estado_atual=START;
				
					}
			 
			break;
			
			case Flag_RCV:
			
			//printf("Estado Flag_RCV\n");
			
			if(buf[0]==0x03){
				frame[index++]=buf[0];
				estado_atual=A_RCV;
			
				}else if(buf[0]==0x7E){
					index = 1;
					estado_atual=Flag_RCV;
			
					}else{
						index=0;
						estado_atual=START;
						}
				break;
				
				
			case A_RCV:
			
			//printf("Estado A_RCV\n");
			
			if(buf[0]==0x03){
				
				frame[index++]=buf[0];
				estado_atual=C_RCV;
				
				}else if(buf[0]==0x7E){
					
					index=1;
					estado_atual=Flag_RCV;
					
					}else{
						index = 0;
						estado_atual=START;
						}
				break;
			
			
			case C_RCV:
			
			//printf("Estado C_RCV\n");
			
			if(buf[0]== 0x03^0x03){
				
				frame[index++]=buf[0];
				
				estado_atual=BCC_OK;
				
				}else if(buf[0]==0x7E){
					index = 1;
					estado_atual=Flag_RCV;
					
					}else{
						
						index=0;
						estado_atual=START;
						
						}
				break;
			
			case BCC_OK:
			
			if(buf[0]==0x7E){
				
				frame[index++]= buf[0];
				estado_atual=STP;
				
				}else{
					
					index = 0;
					estado_atual=START;
				
					}
				break;
			
			case STP:
			// Ver se recebemos o frame
				printf("SET frame recebido.\n");

					// Vamos imprimir os frames
					
					printf("Frame recebido:\n ");
					
					for (int i = 0; i < 5; i++) {
						printf("0x%02X\n", frame[i]);
					}
					
					
					unsigned char setFrame[BUF_SIZE] = {0};
					setFrame[0] = 0x7E;
					setFrame[1] = 0x01;
					setFrame[2] = 0x07;
					setFrame[3] = setFrame[1]^setFrame[2]; 
					setFrame[4] = 0x7E;
					
					
					int bytes = write(fd, setFrame, 5);
					printf("cheguei\n");
					
					STOP = TRUE;
				break;
        
	}
            
		
    }
    
		
    /*buf[0]=0x7E;
    buf[1]=0x01;
    buf[2]=0x07;
    buf[3]=buf[1]^buf[2];
    buf[4]=0x7E;

    int bytes =write(fd, buf,5);
    printf("%d bytes written\n", bytes);
    
    sleep(1);*/
    
    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
