// Write to serial port in non-canonical mode
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
#include <signal.h>

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
volatile int alarmEnabled = TRUE; // Alarme habilitado
int alarmCount = 0;  // Contador do número de alarmes

void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];
    int setstate = START;
 
    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
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
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

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

    // Create string to send
    unsigned char setframe[BUF_SIZE] = {0};

    setframe[0]=0x7E;
    setframe[1]=0x03;
    setframe[2]=0x03; 
    setframe[3]= setframe[1]^ setframe[2];
    setframe[4]=0x7E;

    
    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    //buf[5] = '\n';

    int bytes = write(fd, setframe,BUF_SIZE);
    printf("%d bytes written\n", bytes);
        
    /*while (alarmCount < 4)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }
    }*/

    unsigned char buf[BUF_SIZE] = {0};
    unsigned char frame[5] = {0};
    int index = 0;
    int connectionEstablished = FALSE;

    // Wait until all bytes have been written to the serial port
    sleep(1);
     while (connectionEstablished == FALSE) {
        if (alarmEnabled == TRUE) {
            write(fd, setframe, 5);  // Write again if alarm is triggered
        }
        int bytes = read(fd, buf, 1);
        printf("OLA\n");
        
        while (alarmCount < 4)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }
    }

        if (bytes > 0) {
            switch (setstate) {
                case START:

                printf("ola\n");

                    if (buf[0] == 0x7E) {
                        frame[index++] = buf[0];
                        setstate = Flag_RCV;
                    }

                    break;

                case Flag_RCV:

                    if (buf[0] == 0x01) {

                        frame[index++] = buf[0];
                        setstate = A_RCV;

                    } else if (buf[0] == 0x7E) {
                        index = 1;
                        setstate = Flag_RCV;

                    } else {
                        index = 0;
                        setstate = START;
                    }
                    break;

                case A_RCV:

                    if (buf[0] == 0x07) {
                        frame[index++] = buf[0];
                        setstate = C_RCV;
                    } else if (buf[0] == 0x7E) {
                        index = 1;
                        setstate = Flag_RCV;
                    } else {
                        index = 0;
                        setstate = START;
                    }
                    break;

                case C_RCV:
                    if (buf[0] == (0x01 ^ 0x07)) {
                        frame[index++] = buf[0];
                        setstate = BCC_OK;
                    } else if (buf[0] == 0x7E) {
                        index = 1;
                        setstate = Flag_RCV;
                    } else {
                        index = 0;
                        setstate = START;
                    }
                    break;

                case BCC_OK:
                    if (buf[0] == 0x7E) {
                        frame[index++] = buf[0];
                        setstate = STP;
                    } else {
                        index = 0;
                        setstate = START;
                    }
                    break;

                case STP:
                    connectionEstablished = TRUE;
                    alarm(0); // Disable the alarm
                    break;
            }
        } else if (bytes < 0) {
            perror("Erro ao ler da porta");
            break;
        }
    }

     /*while(STOP==FALSE){
    
    int bytes = read(fd, buf, BUF_SIZE);
    
    for(int i=0; i<5; i++){
    
    printf("0x%02X\n", buf[i]);
    
    }

    STOP=TRUE;
    }*/

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
