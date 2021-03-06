#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

static char const DEVICE_NODE[] = "/dev/nxpnfc"; 

#if 0
#define PRINT_BUF(x,y,z)  {int loop; printf(x); \
                           for(loop=0;loop<z;loop++) printf("%.2x ", y[loop]); \
                           printf("\n");}
#else
#define PRINT_BUF(x,y,z)
#endif

static char gNfcController_generation = 0;

static int openNfc(int * handle)
{
    *handle = open(DEVICE_NODE, O_RDWR);
    if (*handle < 0) return -1;
	return 0;
}

static void closeNfc(int handle)
{
    close(handle);
}

static void reset(int handle)
{
    ioctl(handle, _IOW(0xE9, 0x01, unsigned long), 0);
    usleep(10 * 1000);
    ioctl(handle, _IOW(0xE9, 0x01, unsigned long), 1);
}

static int send(int handle, char *pBuff, int buffLen)
{
	int ret = write(handle, pBuff, buffLen);
	if(ret <= 0) {
		/* retry to handle standby mode */
		ret = write(handle, pBuff, buffLen);
		if(ret <= 0) return 0;
	}
	PRINT_BUF(">> ", pBuff, ret);
	return ret;
}

static int receive(int handle, char *pBuff, int buffLen)
{
    int numRead;
    struct timeval tv;
    fd_set rfds;
    int ret;

    FD_ZERO(&rfds);
    FD_SET(handle, &rfds);
    tv.tv_sec = 2;
    tv.tv_usec = 1;
	ret = select(handle+1, &rfds, NULL, NULL, &tv);
	if(ret <= 0) return 0;

    ret = read(handle, pBuff, 3);
    if (ret <= 0) return 0;
    numRead = 3;
	if(pBuff[2] + 3 > buffLen) return 0;

    ret = read(handle, &pBuff[3], pBuff[2]);
    if (ret <= 0) return 0;
    numRead += ret;

	PRINT_BUF("<< ", pBuff, numRead);

	return numRead;
}

static int transceive(int handle, char *pTx, int TxLen, char *pRx, int RxLen)
{
	if(send(handle, pTx, TxLen) == 0) return 0;
	return receive(handle, pRx, RxLen);
}

static void RfOn (int handle)
{
    char NCIStartDiscovery[] = {0x21, 0x03, 0x09, 0x04, 0x00, 0x01, 0x01, 0x01, 0x02, 0x01, 0x06, 0x01};
    char NCIRfOn[] = {0x2F, 0x3D, 0x02, 0x20, 0x01};
    char Answer[256];
    int NbBytes = 0;
	
	if (gNfcController_generation == 1) {
		printf("Continuous RF ON test - please tap a card\n");
		NbBytes = transceive(handle, NCIStartDiscovery, sizeof(NCIStartDiscovery), Answer, sizeof(Answer));
		if((Answer[0] != 0x41) || (Answer[1] != 0x03) || (Answer[3] != 0x00)) {
			printf("Cannot start discovery loop\n");
			return;
		}
		do {
			NbBytes = receive(handle,  Answer, sizeof(Answer));
		} while ((Answer[0] != 0x61) || ((Answer[1] != 0x05) && (Answer[1] != 0x03)));
	}
	else {
		printf("Continuous RF ON test\n");
		NbBytes = transceive(handle, NCIRfOn, sizeof(NCIRfOn), Answer, sizeof(Answer));
	}
	printf("NFC Controller is now in continuous RF ON mode - Press enter to stop\n");
	fgets(Answer, sizeof(Answer), stdin);
}

static void Prbs (int handle)
{
    char NCIPrbsPN7120[] = {0x2F, 0x30, 0x04, 0x00, 0x00, 0x01, 0x01};
    char NCIPrbsPN7160[] = {0x2F, 0x30, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF};
    char Answer[256];
    int NbBytes = 0;
	int tech, bitrate;
	
	printf("PRBS test:\n");
	printf(" Select technology (A=0, B=1, F=2: ");
	scanf("%d", &tech);
	printf(" Select bitrate (106=0, 212=1, 424=2, 848=3: ");
	scanf("%d", &bitrate);
	
	if (gNfcController_generation == 1) {
		NCIPrbsPN7120[3] = tech;
		NCIPrbsPN7120[4] = bitrate;
		NbBytes = transceive(handle, NCIPrbsPN7120, sizeof(NCIPrbsPN7120), Answer, sizeof(Answer));
	}
	else {
		NCIPrbsPN7160[5] = tech;
		NCIPrbsPN7160[6] = bitrate;
		NbBytes = transceive(handle, NCIPrbsPN7160, sizeof(NCIPrbsPN7160), Answer, sizeof(Answer));
	}
	printf("NFC Controller is now in PRBS mode - Press enter to stop\n");
	fgets(Answer, sizeof(Answer), stdin);
}

static void Standby (int handle)
{
    char NCIEnableStandby[] = {0x2F, 0x00, 0x01, 0x01};
    char Answer[256];

    transceive(handle, NCIEnableStandby, sizeof(NCIEnableStandby), Answer, sizeof(Answer));
    if((Answer[0] != 0x4F) || (Answer[1] != 0x00) || (Answer[3] != 0x00)) {
        printf("Cannot set the NFC Controller in standby mode\n");
        return;
    }

    /* Wait to allow PN71xx entering the standby mode */
    usleep(500 * 1000);

    printf("NFC Controller is now in standby mode - Press enter to stop\n");
    fgets(Answer, sizeof(Answer), stdin);
}

static void Functional (int handle)
{
    char NCIStartDiscovery[] = {0x21, 0x03, 0x09, 0x04, 0x00, 0x01, 0x01, 0x01, 0x02, 0x01, 0x06, 0x01};
    char NCIRestartDiscovery[] = {0x21, 0x06, 0x01, 0x03};
    char Answer[256];

    printf("Continuous RF ON test - please tap a card ...\n");
    transceive(handle, NCIStartDiscovery, sizeof(NCIStartDiscovery), Answer, sizeof(Answer));
    if((Answer[0] != 0x41) || (Answer[1] != 0x03) || (Answer[3] != 0x00)) {
        printf("Cannot start discovery loop\n");
        return;
    }
    printf("NFC Controller is now in functional mode - Press Crtl^Z to stop\n");
    while(1) {
    	do {
        	receive(handle,  Answer, sizeof(Answer));
    	} while ((Answer[0] != 0x61) || ((Answer[1] != 0x05) && (Answer[1] != 0x03)));
		printf(" - tag discovered, restarting discovery loop ...\n"); 
		transceive(handle, NCIRestartDiscovery, sizeof(NCIRestartDiscovery), Answer, sizeof(Answer));
    }
}

static void Dump (int handle)
{
    int i, j, l;
    char get_rf[] = {0x2F, 0x14, 0x02, 0x00, 0x00};
    char Answer[255];

    printf("Dumping all RF settings:\n");
    for (i=0; i<0x9B; i++)
    {
        get_rf[3] = i;
		for (j=0; j<255; j++)
		{
			if((j > 0x8F) && (j < 0xFE)) continue;
			get_rf[4] = j;
			transceive(handle, get_rf, sizeof(get_rf), Answer, sizeof(Answer));
			if ((Answer[0] != 0x4F) || (Answer[1] != 0x14) || (Answer[3] != 0x00)) continue;
			printf(" transition 0x%02X, register 0x%02X = ", i, j); for(l=0; l<Answer[2]-1; l++) printf("%02X ", Answer[4+l]); printf("\n");
		}
    }
}

static void SetRF (int handle)
{
    char set_rf[] = {0x20, 0x02, 0x00, 0x01, 0xA0, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char Answer[255];
    unsigned int temp[4];

    printf("Set RF settings menu (enter 0 during transition ID selection step to leave):");

    while (1)
    {
	printf("\n");

	printf("- enter transition ID in (hexadecimal): ");
	scanf("%x", &temp[0]);
	set_rf[7] = (char) temp[0];
	if (set_rf[7] == 0)
	{
	    printf("Leaving RF setting menu\n");
	    return;
	}

	printf("- enter register ID in (hexadecimal): ");
	scanf("%x", &temp[0]);
	set_rf[8] = (char) temp[0];
	if (set_rf[8] == 0)
	{
	    printf("Wrong register ID\n");
	    continue;
	}

	printf("- enter value length (1, 2 or 4): ");
	scanf("%x", &temp[0]);
	if ((temp[0] != 1) && (temp[0] != 2) && (temp[0] != 4))
	{
	    printf("Wrong value length\n");
	    continue;
	}
	set_rf[2] = 6 + temp[0];
	set_rf[6] = 2 + temp[0];

	printf("- enter value (in hexadecimal): ");
	switch(temp[0])
	{
	case 1:
	    scanf("%x", &temp[0]);
	    set_rf[9] = (char) temp[0];
	    break;
	case 2:
	    scanf("%x %x", &temp[0], &temp[1]);
	    set_rf[9]  = (char) temp[0];
	    set_rf[10] = (char) temp[1];
	    break;
	default:
	    scanf("%x %x %x %x", &temp[0], &temp[1], &temp[2], &temp[3]);
	    set_rf[9]  = (char) temp[0];
	    set_rf[10] = (char) temp[1];
	    set_rf[11] = (char) temp[2];
	    set_rf[12] = (char) temp[3];
	    break;
	}

	transceive(handle, set_rf, set_rf[2] + 3, Answer, sizeof(Answer));
	if ((Answer[0] != 0x40) || (Answer[1] != 0x02) || (Answer[3] != 0x00))
	    printf("Error, cannot write the RF setting\n");
	else
	    printf("RF setting successfully written\n");
    }
}

int main()
{
	int nHandle;
    char NCICoreReset[] = {0x20, 0x00, 0x01, 0x01};
    char NCICoreInit1_0[] = {0x20, 0x01, 0x00};
    char NCICoreInit2_0[] = {0x20, 0x01, 0x02, 0x00, 0x00};
    char NCIDisableStandby[] = {0x2F, 0x00, 0x01, 0x00};
    char Answer[256];
    int NbBytes = 0;
	
	printf("\n----------------------------\n");
	printf("NFC Factory Test Application\n");
	printf("----------------------------\n");

	if(openNfc(&nHandle) != 0) {
		printf("Cannot connect to NFC controller\n");
		return -1;
	}

	reset(nHandle);
	
	NbBytes = transceive(nHandle, NCICoreReset, sizeof(NCICoreReset), Answer, sizeof(Answer));
	
    /* Catch potential notification */
    usleep(100*1000);
    NbBytes = receive(nHandle,  Answer, sizeof(Answer));
    if((NbBytes == 12) && (Answer[0] == 0x60) && (Answer[1] == 0x00) && (Answer[3] == 0x02))
    {
	NbBytes = transceive(nHandle, NCICoreInit2_0, sizeof(NCICoreInit2_0), Answer, sizeof(Answer));
        if((NbBytes < 19) || (Answer[0] != 0x40) || (Answer[1] != 0x01) || (Answer[3] != 0x00))    {
            printf("Error communicating with NFC Controller\n");
            return -1;
        }

        printf("PN7160 NFC controller detected\n");
        gNfcController_generation = 3;
    }
    else
    {
	NbBytes = transceive(nHandle, NCICoreInit1_0, sizeof(NCICoreInit1_0), Answer, sizeof(Answer));
        if((NbBytes < 19) || (Answer[0] != 0x40) || (Answer[1] != 0x01) || (Answer[3] != 0x00))    {
            printf("Error communicating with PN71xx NFC Controller\n");
            return -1;
        }

        /* Retrieve NXP-NCI NFC Controller generation */
        if (Answer[17+Answer[8]] == 0x08) {
            printf("PN7120 NFC controller detected\n");
            gNfcController_generation = 1;
        }
        else if (Answer[17+Answer[8]] == 0x10) {
            printf("PN7150 NFC controller detected\n");
            gNfcController_generation = 2;
        }
        else {
            printf("Wrong NFC controller detected\n");
            return -1;
        }
    }

	NbBytes = transceive(nHandle, NCIDisableStandby, sizeof(NCIDisableStandby), Answer, sizeof(Answer));
	
    printf("Select the test to run:\n");
    printf("\t 1. Continuous RF ON mode\n");
    printf("\t 2. Functional mode\n");
    printf("\t 3. PRBS mode\n");
    printf("\t 4. Standby mode\n");
    printf("\t 5. Dump RF settings\n");
    printf("\t 6. Set RF setting\n");
    printf("Your choice: ");
    scanf("%d", &NbBytes);

    switch(NbBytes) {
        case 1: RfOn(nHandle);              break;
        case 2: Functional(nHandle);        break;
        case 3: Prbs(nHandle);     	    break;
        case 4: Standby(nHandle);  	    break;
        case 5: Dump(nHandle);     	    break;
        case 6: SetRF(nHandle);    	    break;
        default: printf("Wrong choice\n");  break;
    }

	fgets(Answer, sizeof(Answer), stdin);
	
	reset(nHandle);
	closeNfc(nHandle);
	
	return 0;
}
