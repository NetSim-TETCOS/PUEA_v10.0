#include "main.h"
#include "802_22.h"
typedef struct stru_ExtentedDSMAPIE
{
	DSMAP_IE* pstruDSMAPIE;
	struct stru_ExtentedDSMAPIE* pstruNext;
}ExtendedDSIE;
/** The DS-MAP message defines the access to the downstream information. 
The length of the DS-MAP shall be an integer number of bytes. */
NetSim_PACKET* fn_NetSim_CR_FormDSMAP(NETSIM_ID nDeviceId, NETSIM_ID nInterfaceId)
{
	BS_PHY* pstruBSPhy;
	BS_MAC* pstruBSMAC;
	unsigned int nDSIECount=0;
	NetSim_PACKET* pstruPacket;
	unsigned int nPacketSize;
	unsigned int nSID;
	int nDSBurst;
	unsigned int SlotRequire;
	ExtendedDSIE *pstruDSMAPIE = NULL;
	ExtendedDSIE *pstruTempDSMAPIE = NULL;
	ExtendedDSIE *pstruFirstDSMAPIE = NULL;
	pstruBSPhy = DEVICE_PHYVAR(nDeviceId,nInterfaceId);
	pstruBSMAC = DEVICE_MACVAR(nDeviceId,nInterfaceId);
	//Allocate memory for DS-MAP
	pstruBSMAC->pstruDSMAP = fnpAllocateMemory(1,sizeof(DSMAP));
	pstruBSMAC->pstruDSMAP->nManagementMessageType = MMM_DS_MAP%10;
	pstruBSMAC->pstruDSMAP->nPaddingBits = 0;

	//Loop through access buffer
	while(pstruBSMAC->pstruDSPacketList)
	{
		pstruPacket = pstruBSMAC->pstruDSPacketList;
		nSID = pstruBSMAC->anSIDFromDevId[pstruPacket->nDestinationId];
		if(!nSID && pstruPacket->nDestinationId)
		{
			//The packet is not for this BS
			pstruBSMAC->pstruDSPacketList = pstruBSMAC->pstruDSPacketList->pstruNextPacket;
			pstruPacket->pstruNextPacket = NULL;
			fn_NetSim_Packet_FreePacket(pstruPacket);
			continue;
		}
		//Set the receiver id
		pstruPacket->nReceiverId = pstruPacket->nDestinationId;
		pstruPacket->nTransmitterId = nDeviceId;
	
		nPacketSize = (unsigned int)fnGetPacketSize(pstruPacket);

		if(pstruBSPhy->nFrameNumber==0)// for considering SCH & preamble
				nDSBurst = fn_NetSim_CR_FillDSFrame(nPacketSize,pstruBSMAC->pstruDSBurst,pstruBSPhy->pstruSymbolParameter,4,&SlotRequire);
		else
				nDSBurst = fn_NetSim_CR_FillDSFrame(nPacketSize,pstruBSMAC->pstruDSBurst,pstruBSPhy->pstruSymbolParameter,2,&SlotRequire);
		
		if(nDSBurst)
		{
			//Allocated
			pstruBSMAC->pstruDSPacketList = pstruBSMAC->pstruDSPacketList->pstruNextPacket;
			pstruPacket->pstruNextPacket = NULL;
			fn_NetSim_CR_AddPacketToDSBurst(pstruBSMAC->pstruDSBurst[nDSBurst],pstruPacket);
			//Preparae the DS-MAP IE
			pstruTempDSMAPIE = fnpAllocateMemory(1,sizeof *pstruTempDSMAPIE);
			pstruTempDSMAPIE->pstruDSMAPIE = fnpAllocateMemory(1,sizeof *pstruTempDSMAPIE->pstruDSMAPIE);
			pstruTempDSMAPIE->pstruDSMAPIE->SID = nSID;
			pstruTempDSMAPIE->pstruDSMAPIE->length = SlotRequire;
			nDSIECount++;
			if(!pstruFirstDSMAPIE)
			{
				pstruFirstDSMAPIE = pstruTempDSMAPIE;
				pstruDSMAPIE = pstruFirstDSMAPIE;
			}
			else
			{
				pstruDSMAPIE->pstruNext = pstruTempDSMAPIE;
				pstruDSMAPIE = pstruTempDSMAPIE;
			}
		}
		else
			break; // No space available in DS-Frame
	}
	pstruBSMAC->pstruDSMAP->nIECount = nDSIECount;
	pstruBSMAC->pstruDSMAP->pstruIE = fnpAllocateMemory(nDSIECount,sizeof(DSMAP_IE*));
	nDSIECount = 0;
	while(pstruFirstDSMAPIE)
	{
		pstruBSMAC->pstruDSMAP->pstruIE[nDSIECount++] = pstruFirstDSMAPIE->pstruDSMAPIE;
		pstruTempDSMAPIE = pstruFirstDSMAPIE;
		pstruFirstDSMAPIE = pstruFirstDSMAPIE->pstruNext;
		fnpFreeMemory(pstruTempDSMAPIE);
	}
	return fn_NetSim_CR_GenerateBroadcastCtrlPacket(nDeviceId,nInterfaceId,MMM_DS_MAP);
}
int fn_NetSim_CR_CPE_ProcessDSMAP()
{
	return 1;
}
