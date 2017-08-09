#include "main.h"
#include "802_22.h"
/** This function is used to check whether the destination is present or not.
If it presents,it broadcast the data packet */
int fn_NetSim_CR_BS_ForwardDataPacket()
{
	NetSim_BUFFER* pstruBuffer;
	NETSIM_ID nDeviceId = pstruEventDetails->nDeviceId;
	NETSIM_ID nInterfaceId = pstruEventDetails->nInterfaceId;
	NetSim_PACKET* pstruPacket = pstruEventDetails->pPacket;
	BS_MAC* pstruBSMac = (BS_MAC*)DEVICE_MACVAR(nDeviceId,nInterfaceId);
	//Check the destination
	if(pstruPacket->nDestinationId == 0)
	{
		//Broadcase packet
		NETSIM_ID nloop;
		pstruPacket = fn_NetSim_CR_BS_PackPacket(pstruBSMac,pstruPacket);
		if(!pstruPacket)
			return 1;
		for(nloop = 0;nloop<NETWORK->ppstruDeviceList[nDeviceId-1]->nNumOfInterface;nloop++)
		{
			if(!DEVICE_MAC_NW_INTERFACE(nDeviceId,nloop+1))
				continue;
			if(nloop+1==nInterfaceId)
			{
				NetSim_PACKET* list = pstruBSMac->pstruDSPacketList;
				if(!list)
				{
					pstruBSMac->pstruDSPacketList = fn_NetSim_Packet_CopyPacket(pstruPacket);
				}
				else
				{
					while(list->pstruNextPacket)
						list = list->pstruNextPacket;
					list->pstruNextPacket = fn_NetSim_Packet_CopyPacket(pstruPacket);
				}
				continue;
			}
			//Place the packet to access interface
			pstruBuffer = DEVICE_MAC_NW_INTERFACE(nDeviceId,nloop+1)->pstruAccessBuffer;
			if(!fn_NetSim_GetBufferStatus(pstruBuffer))
			{
				//Add the MAC out event
				pstruEventDetails->nEventType = MAC_OUT_EVENT;
				pstruEventDetails->nInterfaceId = nloop+1;
				pstruEventDetails->nProtocolId = fn_NetSim_Stack_GetMacProtocol(nDeviceId,nloop+1);
				pstruEventDetails->nSubEventType = 0;
				pstruEventDetails->pPacket = NULL;
				fnpAddEvent(pstruEventDetails);
			}
			fn_NetSim_Packet_AddPacketToList(pstruBuffer,fn_NetSim_Packet_CopyPacket(pstruPacket),0);
		}
		fn_NetSim_Packet_FreePacket(pstruPacket);
	}
	else
	{
		//Point to point packet
		int nSID = pstruBSMac->anSIDFromDevId[pstruPacket->nDestinationId];
		if(nSID)
		{
			NetSim_PACKET* list = pstruBSMac->pstruDSPacketList;
			if(!list)
			{
				pstruBSMac->pstruDSPacketList = pstruPacket;
			}
			else
			{
				while(list->pstruNextPacket)
					list = list->pstruNextPacket;
				list->pstruNextPacket = pstruPacket;
			}
			pstruPacket->nTransmitterId = pstruPacket->nReceiverId;
			pstruPacket->nReceiverId = pstruPacket->nDestinationId;
		}
		else
		{
			//The packet is not for this network. Broadcasting to other port
			NETSIM_ID nloop;
			pstruPacket = fn_NetSim_CR_BS_PackPacket(pstruBSMac,pstruPacket);
			if(!pstruPacket)
				return 1;
			for(nloop = 0;nloop<NETWORK->ppstruDeviceList[nDeviceId-1]->nNumOfInterface;nloop++)
			{
				if(nloop == nInterfaceId-1)
					continue;
				//Place the packet to access interface
				pstruBuffer = DEVICE_MAC_NW_INTERFACE(nDeviceId,nloop+1)->pstruAccessBuffer;
				if(!fn_NetSim_GetBufferStatus(pstruBuffer))
				{
					//Add the MAC out event
					pstruEventDetails->nEventType = MAC_OUT_EVENT;
					pstruEventDetails->nInterfaceId = nloop+1;
					pstruEventDetails->nProtocolId = fn_NetSim_Stack_GetMacProtocol(nDeviceId,nloop+1);
					pstruEventDetails->nSubEventType = 0;
					pstruEventDetails->pPacket = NULL;
					fnpAddEvent(pstruEventDetails);
				}
				fn_NetSim_CR_FreePacket_F(pstruPacket);
				fn_NetSim_Packet_AddPacketToList(pstruBuffer,pstruPacket,0);
			}
		}
	}
	return 0;
}
