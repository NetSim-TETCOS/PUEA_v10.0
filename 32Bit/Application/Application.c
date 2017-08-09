/************************************************************************************
* Copyright (C) 2014                                                               *
* TETCOS, Bangalore. India                                                         *
*                                                                                  *
* Tetcos owns the intellectual property rights in the Product and its content.     *
* The copying, redistribution, reselling or publication of any or all of the       *
* Product or its content without express prior written consent of Tetcos is        *
* prohibited. Ownership and / or any other right relating to the software and all  *
* intellectual property rights therein shall remain at all times with Tetcos.      *
*                                                                                  *
* Author:    Shashi Kant Suman                                                     *
*                                                                                  *
* ---------------------------------------------------------------------------------*/
#include "main.h"
#include "Application.h"
/** 
This function is used to initialize the parameter for all the application based on 
the traffic type 
*/
_declspec(dllexport) int fn_NetSim_Application_Init(struct stru_NetSim_Network *NETWORK_Formal,
													NetSim_EVENTDETAILS *pstruEventDetails_Formal,
													char *pszAppPath_Formal,
													char *pszWritePath_Formal,
													int nVersion_Type,
													void **fnPointer)
{
	return fn_NetSim_Application_Init_F(NETWORK_Formal,
		pstruEventDetails_Formal,
		pszAppPath_Formal,
		pszWritePath_Formal,
		nVersion_Type,
		fnPointer);
}
/** This function is used to configure the applications like application id, source count, source id, destination count etc. */
_declspec(dllexport) int fn_NetSim_Application_Configure(void** var)
{
	return fn_NetSim_Application_Configure_F(var);
}
/**	
This function is called by NetworkStack.dll, whenever the event gets triggered	
inside the NetworkStack.dll for applications. This is the main function for the application.
It processes APPLICATION_OUT, APPLICATION_IN and TIMER events.
 */
 uint64_t em_current_time(void);
_declspec (dllexport) int fn_NetSim_Application_Run()
{
	switch(pstruEventDetails->nEventType)
	{
	case APPLICATION_OUT_EVENT:
		{
			unsigned int nSocketId;
			int nSegmentCount=0;
			double ldEventTime=pstruEventDetails->dEventTime;
			APP_INFO* appInfo=(APP_INFO*)pstruEventDetails->szOtherDetails;
			NETSIM_ID nDeviceId = pstruEventDetails->nDeviceId;
			NetSim_PACKET* pstruPacket=pstruEventDetails->pPacket;
			if(pstruPacket)
			{
				APPLICATION_TYPE nappType=pstruPacket->pstruAppData->nAppType;
				nSocketId=fnGetSocketId(pstruEventDetails->nApplicationId,
					pstruPacket->nSourceId,
					pstruPacket->nDestinationId,
					pstruPacket->pstruTransportData->nSourcePort,
					pstruPacket->pstruTransportData->nDestinationPort);
				//Initialize the application data
				pstruPacket->pstruAppData->dArrivalTime = ldEventTime;
				pstruPacket->pstruAppData->dEndTime = ldEventTime;
				if(pstruPacket->nPacketType!=PacketType_Control)
				{
					pstruPacket->pstruAppData->dOverhead = 0;
					pstruPacket->pstruAppData->dPayload = pstruEventDetails->dPacketSize;
				}
				else
				{
					pstruPacket->pstruAppData->dPayload = 0;
					pstruPacket->pstruAppData->dOverhead = pstruEventDetails->dPacketSize;
				}
				pstruPacket->pstruAppData->dPacketSize = pstruPacket->pstruAppData->dOverhead+pstruPacket->pstruAppData->dPayload;
				pstruPacket->pstruAppData->dStartTime = ldEventTime;
				pstruPacket->pstruAppData->nApplicationId = pstruEventDetails->nApplicationId;
				if(!NETWORK->ppstruDeviceList[nDeviceId-1]->pstruSocketInterface->pstruSocketBuffer[nSocketId]->pstruPacketlist)
				{
					//Socket buffer is NULL
					//Create event for transport out
					pstruEventDetails->dEventTime = ldEventTime;
					pstruEventDetails->nEventType = TRANSPORT_OUT_EVENT;
					pstruEventDetails->pPacket = NULL;
					pstruEventDetails->nProtocolId = fn_NetSim_Stack_GetTrnspProtocol(nDeviceId,pstruPacket);
					pstruEventDetails->szOtherDetails=(void*)nSocketId;
					//Add Transport out event
					fnpAddEvent(pstruEventDetails);
				}
				//Fragment the packet
				nSegmentCount=fn_NetSim_Stack_FragmentPacket(pstruPacket,(int)fn_NetSim_Stack_GetMSS(pstruPacket));
				if(nappType==TRAFFIC_FTP || nappType==TRAFFIC_DATABASE)
				{
					NetSim_PACKET* packet=pstruPacket;
					while(packet->pstruNextPacket)
						packet=packet->pstruNextPacket;
					packet->pstruAppData->nAppEndFlag=1;
					fn_NetSim_Application_StartDataAPP(appInfo,pstruEventDetails->dEventTime,pstruPacket->nSourceId,pstruPacket->nDestinationId);
				}
				else if(nappType==TRAFFIC_HTTP)
				{
					//Do nothing
					NetSim_PACKET* packet=pstruPacket;
					if(pstruPacket->pstruAppData->nAppEndFlag)
					{
						while(packet->pstruNextPacket)
						{
							packet->pstruAppData->nAppEndFlag=0;
							packet=packet->pstruNextPacket;
						}
						packet->pstruAppData->nAppEndFlag=1;
					}
				}
				else if(nappType==TRAFFIC_EMAIL)
				{
					appInfo = fn_NetSim_Application_Email_GenerateNextPacket((DETAIL*)appInfo,pstruPacket->nSourceId,pstruPacket->nDestinationId,pstruEventDetails->dEventTime);
				}
				else if(nappType==TRAFFIC_PEER_TO_PEER)
				{
					NetSim_PACKET* packet=pstruPacket;
					while(packet->pstruNextPacket)
						packet=packet->pstruNextPacket;
					packet->pstruAppData->nAppEndFlag=1;
				}
				else if(nappType == TRAFFIC_EMULATION)
				{
					//do nothing
				}
				else
					fn_NetSim_Application_GenerateNextPacket(appInfo,pstruPacket->nSourceId,pstruPacket->nDestinationId,pstruEventDetails->dEventTime);
				//Add the dummy payload to packet
				fn_NetSim_Add_DummyPayload(pstruPacket,appInfo);
				//Place the packet to socket buffer
				fn_NetSim_Packet_AddPacketToList(NETWORK->ppstruDeviceList[nDeviceId-1]->pstruSocketInterface->pstruSocketBuffer[nSocketId],pstruPacket,2);
				fn_NetSim_Application_Metrics_Source(appInfo,pstruPacket);
			}
		}
		break;
	case APPLICATION_IN_EVENT:
		{
			NetSim_PACKET* pstruPacket=pstruEventDetails->pPacket;
			if(pstruPacket->nPacketType != PacketType_Control && pstruPacket->pstruAppData->nApplicationId && 
				pstruPacket->nControlDataType/100 != PROTOCOL_APPLICATION)
			{
				APP_INFO* pstruappinfo;
				fnValidatePacket(pstruPacket);
				pstruappinfo=appInfo[pstruPacket->pstruAppData->nApplicationId-1];
				pstruPacket->pstruAppData->dEndTime = pstruEventDetails->dEventTime;
				fn_NetSim_Application_PlotPlot(pstruPacket);
				fn_NetSim_Application_Metrics_Destination(pstruappinfo,pstruPacket);
				if(pstruappinfo->nAppType==TRAFFIC_PEER_TO_PEER && pstruPacket->pstruAppData->nAppEndFlag==1)
				{
					fn_NetSim_Application_P2P_MarkReceivedPacket(pstruappinfo,pstruPacket);
					fn_NetSim_Application_P2P_SendNextPiece(pstruappinfo,pstruPacket->nDestinationId,pstruEventDetails->dEventTime);
				}
				if(pstruappinfo->nAppType == TRAFFIC_EMULATION && pstruPacket->szPayload)
				{
					fn_NetSim_Dispatch_to_emulator(pstruPacket);
				}
				if (pstruappinfo->nAppType == TRAFFIC_BSM_APP)
				{
					process_saej2735_packet(pstruPacket);
				}
				//Delete the packet
				fn_NetSim_Packet_FreePacket(pstruPacket);
			}
			else if(pstruPacket->nControlDataType == packet_HTTP_REQUEST)
			{
				APP_INFO* pstruappinfo;
				fnValidatePacket(pstruPacket);
				pstruappinfo=appInfo[pstruPacket->pstruAppData->nApplicationId-1];
				pstruPacket->pstruAppData->dEndTime = pstruEventDetails->dEventTime;
				fn_NetSim_Application_Metrics_Destination(pstruappinfo,pstruPacket);
				fn_NetSim_Application_HTTP_ProcessRequest(pstruappinfo,pstruPacket);
			}
		}
		break;
	case TIMER_EVENT:
		{
			switch(pstruEventDetails->nSubEventType)
			{
			case event_APP_START:
				//fn_NetSim_Call_LowerLayer(); //Not implemented now
				switch(pstruEventDetails->pPacket->pstruAppData->nAppType)
				{
				case TRAFFIC_DATABASE:
				case TRAFFIC_FTP:
				case TRAFFIC_CUSTOM:
				case TRAFFIC_DATA:
				case TRAFFIC_VOICE:
				case TRAFFIC_VIDEO:
				case TRAFFIC_HTTP:
				case TRAFFIC_EMAIL:
				case TRAFFIC_CBR:
				case TRAFFIC_SENSING:
				case TRAFFIC_BSM_APP:
					pstruEventDetails->nEventType=APPLICATION_OUT_EVENT;
					pstruEventDetails->nSubEventType=0;
					fnpAddEvent(pstruEventDetails);
					break;
				case TRAFFIC_ERLANG_CALL:
					{
						APP_INFO* appInfo=(APP_INFO*)pstruEventDetails->szOtherDetails;
						pstruEventDetails->nEventType=APPLICATION_OUT_EVENT;
						pstruEventDetails->nSubEventType=0;
						fnpAddEvent(pstruEventDetails);
						pstruEventDetails->szOtherDetails=appInfo;
						fn_NetSim_Application_ErlangCall_StartCall(pstruEventDetails);
					}
					break;
				default:
					fnNetSimError("Unknown application type %d in application_run.\n",pstruEventDetails->pPacket->pstruAppData->nAppType);
					break;
				}
				break;
			case event_APP_RESTART:
				fn_NetSim_App_RestartApplication();
				break;
			case event_APP_END:
				switch(appInfo[pstruEventDetails->nApplicationId-1]->nAppType)
				{
				case TRAFFIC_ERLANG_CALL:
					fn_NetSim_Application_ErlangCall_EndCall(pstruEventDetails);
					break;
				}
				break;
			}
		}
		break;
	default:
		fnNetSimError("Unknown event type for Application");
		break;
	}
	return 1;
}
/**
This function is called by NetworkStack.dll, while writing the event trace 
to get the sub event as a string.
*/
_declspec (dllexport) char *fn_NetSim_Application_Trace(int nSubEvent)
{
	return "";
}
/**
This function is called by NetworkStack.dll, to free the application information from a packets.
*/
_declspec(dllexport) int fn_NetSim_Application_FreePacket(NetSim_PACKET* pstruPacket)
{
	return 1;
}
/**
This function is called by NetworkStack.dll, to copy the application
related information to a new packet 
*/
_declspec(dllexport) int fn_NetSim_Application_CopyPacket(NetSim_PACKET* pstruDestPacket,NetSim_PACKET* pstruSrcPacket)
{
	return 1;
}
/**
This function writes the Application metrics in Metrics.txt	
*/
_declspec(dllexport) int fn_NetSim_Application_Metrics(PMETRICSWRITER metricsWriter)
{
	return fn_NetSim_Application_Metrics_F(metricsWriter);
}

/**
This function is used to configure the packet trace
*/
_declspec(dllexport) char* fn_NetSim_Application_ConfigPacketTrace()
{
	return "";
}
/**
This function is used to write the packet trace																									
*/
_declspec(dllexport) int fn_NetSim_Application_WritePacketTrace(NetSim_PACKET* pstruPacket, char** ppszTrace)
{
	return 1;
}
/**
	This function is called by NetworkStack.dll, once simulation ends, to free the 
	allocated memory.	
*/
_declspec(dllexport) int fn_NetSim_Application_Finish()
{
	unsigned int loop,i,j;
	for(loop=0;loop<nApplicationCount;loop++)
	{
		free(appInfo[loop]->sourceList);
		free(appInfo[loop]->destList);
		free((char*)(appInfo[loop]->appData));
		for(i=0;i<NETWORK->nDeviceCount;i++)
			free(appInfo[loop]->nAppState[i]);
		free(appInfo[loop]->nAppState);
		free(appInfo[loop]->sourcePort);
		free(appInfo[loop]->destPort);
		if(appInfo[loop]->appMetrics)
		{
			for(i=0;i<appInfo[loop]->nSourceCount;i++)
			{
				if(appInfo[loop]->appMetrics[i])
				{
					for(j=0;j<appInfo[loop]->nDestCount;j++)
					{
						if(appInfo[loop]->appMetrics[i][j])
						{
							while(appInfo[loop]->appMetrics[i][j]->receivedInfo)
								LIST_FREE((void**)&appInfo[loop]->appMetrics[i][j]->receivedInfo,appInfo[loop]->appMetrics[i][j]->receivedInfo);
							free(appInfo[loop]->appMetrics[i][j]);
						}
					}
					free(appInfo[loop]->appMetrics[i]);
				}
			}
			free(appInfo[loop]->appMetrics);
		}

		while(appInfo[loop]->socketInfo)
			LIST_FREE((void**)&appInfo[loop]->socketInfo,appInfo[loop]->socketInfo);
		free(appInfo[loop]);
	}
	
	free(appInfo);
	return 1;
}
/** 
This function is used to generate the packets for application 
*/
NetSim_PACKET* fn_NetSim_Application_GeneratePacket(double ldArrivalTime,
													NETSIM_ID nSourceId,
													NETSIM_ID nDestinationId,
													unsigned long long int nPacketId,
													APPLICATION_TYPE nAppType,
													PACKET_PRIORITY nPacketPriority,
													QUALITY_OF_SERVICE nQOS,
													unsigned int sourcePort,
													unsigned int destPort)
{
	NetSim_PACKET* pstruPacket;
	pstruPacket= fn_NetSim_Packet_CreatePacket(5);
	pstruPacket->dEventTime = ldArrivalTime;
	pstruPacket->nDestinationId = nDestinationId;
	pstruPacket->nPacketId = nPacketId;
	pstruPacket->nPacketStatus = PacketStatus_NoError;
	pstruPacket->nPacketType = fn_NetSim_Stack_GetPacketTypeBasedOnApplicationType(nAppType);
	pstruPacket->pstruAppData->nAppType = nAppType;
	pstruPacket->nPacketPriority = nPacketPriority;
	pstruPacket->nQOS = nQOS;
	pstruPacket->nSourceId = nSourceId;
	pstruPacket->pstruTransportData->nSourcePort = sourcePort;
	pstruPacket->pstruTransportData->nDestinationPort = destPort;

	if(NETWORK->ppstruDeviceList[nSourceId-1]->pstruNetworkLayer && NETWORK->ppstruDeviceList[nSourceId-1]->pstruNetworkLayer->ipVar)
	{
		//Add the source ip address
		pstruPacket->pstruNetworkData->szSourceIP = IP_COPY(fn_NetSim_Stack_GetFirstIPAddressAsId(nSourceId,0));
		//Add the destination IP address
		if(nDestinationId)
			pstruPacket->pstruNetworkData->szDestIP = IP_COPY(DNS_QUERY(nSourceId,pstruPacket->nDestinationId));
		else
		{
			NETSIM_IPAddress ip = pstruPacket->pstruNetworkData->szSourceIP;
			if(ip->type==4)
				pstruPacket->pstruNetworkData->szDestIP = STR_TO_IP("255.255.255.255",4);
			else
				pstruPacket->pstruNetworkData->szDestIP = STR_TO_IP("FF00:0:0:0:0:0:0:0",6);
		}
	}
	//Set TTL value
	pstruPacket->pstruNetworkData->nTTL = MAX_TTL;
	return pstruPacket;
}
/** 
This function is used to the get the index of the source for an application, if the source is not present in the 
source list then it returns -1 
*/
int fnGetSourceIndex(APP_INFO* appInfo,NETSIM_ID source)
{
	unsigned int i;
	for(i=0;i<appInfo->nSourceCount;i++)
		if(appInfo->sourceList[i]==source)
			return i;
	return -1;
}
/** 
This function is used to the get the index of the destination for an application, if the destination is 
not present in the destination list then it returns -1 
*/
int fnGetDestIndex(APP_INFO* appInfo,NETSIM_ID dest)
{
	unsigned int i;
	for(i=0;i<appInfo->nDestCount;i++)
		if(appInfo->destList[i]==dest)
			return i;
	return -1;
}
/**
This function is used to generate the next packet
*/
int fn_NetSim_Application_GenerateNextPacket(APP_INFO* appInfo,NETSIM_ID nSource,NETSIM_ID nDestination,double time)
{
	if(appInfo->dEndTime<=time)
		return 0;
	if(nDestination && appInfo->nAppState[nSource-1][nDestination-1] != appState_Started)
		return 0;
	switch(appInfo->nAppType)
	{
	case TRAFFIC_FTP:
	case TRAFFIC_CUSTOM:
	case TRAFFIC_DATA:
	case TRAFFIC_CBR:
	case TRAFFIC_SENSING:
		{
			int i=fnGetSourceIndex(appInfo,nSource);
			int j=fnGetDestIndex(appInfo,nDestination);
			double arrivalTime=0;
			double packetSize=0;
			fn_NetSim_TrafficGenerator_Custom((APP_DATA_INFO*)appInfo->appData,&packetSize,&arrivalTime,
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[0]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[1]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[0]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[1]));

			pstruEventDetails->dEventTime=time+arrivalTime;
			pstruEventDetails->dPacketSize=(double)((int)packetSize);
			pstruEventDetails->nApplicationId=appInfo->id;
			pstruEventDetails->nDeviceId=nSource;
			pstruEventDetails->nDeviceType=DEVICE_TYPE(nSource);
			pstruEventDetails->nEventType=APPLICATION_OUT_EVENT;
			pstruEventDetails->nInterfaceId=0;
			pstruEventDetails->nProtocolId=PROTOCOL_APPLICATION;
			pstruEventDetails->nSegmentId=0;
			pstruEventDetails->nSubEventType=0;
			pstruEventDetails->pPacket=fn_NetSim_Application_GeneratePacket(pstruEventDetails->dEventTime,
				nSource,nDestination,++appInfo->nPacketId,appInfo->nAppType,Priority_Low,QOS_BE,appInfo->sourcePort[i],appInfo->destPort[j]);
			pstruEventDetails->nPacketId=appInfo->nPacketId;
			pstruEventDetails->szOtherDetails=appInfo;
			fnpAddEvent(pstruEventDetails);
		}
		break;
	case TRAFFIC_DATABASE:
		break;
	case TRAFFIC_HTTP:
		break;
	case TRAFFIC_VIDEO:
		{
			int i=fnGetSourceIndex(appInfo,nSource);
			int j=fnGetDestIndex(appInfo,nDestination);
			double arrivalTime=0;
			double packetSize=0;
			fn_NetSim_TrafficGenerator_Video((APP_VIDEO_INFO*)appInfo->appData,&packetSize,&arrivalTime,
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[0]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[1]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[0]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[1]));
			pstruEventDetails->dEventTime=time+arrivalTime;
			pstruEventDetails->dPacketSize=(double)((int)packetSize);
			pstruEventDetails->nApplicationId=appInfo->id;
			pstruEventDetails->nDeviceId=nSource;
			pstruEventDetails->nDeviceType=DEVICE_TYPE(nSource);
			pstruEventDetails->nEventType=APPLICATION_OUT_EVENT;
			pstruEventDetails->nInterfaceId=0;
			pstruEventDetails->nProtocolId=PROTOCOL_APPLICATION;
			pstruEventDetails->nSegmentId=0;
			pstruEventDetails->nSubEventType=0;
			pstruEventDetails->pPacket=fn_NetSim_Application_GeneratePacket(pstruEventDetails->dEventTime,
				nSource,nDestination,++appInfo->nPacketId,appInfo->nAppType,Priority_Medium,QOS_nrtPS,appInfo->sourcePort[i],appInfo->destPort[j]);
			pstruEventDetails->nPacketId=appInfo->nPacketId;
			pstruEventDetails->szOtherDetails=appInfo;
			fnpAddEvent(pstruEventDetails);
		}
		break;
	case TRAFFIC_VOICE:
		{
			int i=fnGetSourceIndex(appInfo,nSource);
			int j=fnGetDestIndex(appInfo,nDestination);
			double arrivalTime=0;
			double packetSize=0;
			fn_NetSim_TrafficGenerator_Voice((APP_VOICE_INFO*)appInfo->appData,&packetSize,&arrivalTime,
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[0]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[1]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[0]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[1]));
			pstruEventDetails->dEventTime=time+arrivalTime;
			pstruEventDetails->dPacketSize=(double)((int)packetSize);
			pstruEventDetails->nApplicationId=appInfo->id;
			pstruEventDetails->nDeviceId=nSource;
			pstruEventDetails->nDeviceType=DEVICE_TYPE(nSource);
			pstruEventDetails->nEventType=APPLICATION_OUT_EVENT;
			pstruEventDetails->nInterfaceId=0;
			pstruEventDetails->nProtocolId=PROTOCOL_APPLICATION;
			pstruEventDetails->nSegmentId=0;
			pstruEventDetails->nSubEventType=0;
			pstruEventDetails->pPacket=fn_NetSim_Application_GeneratePacket(pstruEventDetails->dEventTime,
				nSource,nDestination,++appInfo->nPacketId,appInfo->nAppType,Priority_High,QOS_rtPS,appInfo->sourcePort[i],appInfo->destPort[j]);
			pstruEventDetails->nPacketId=appInfo->nPacketId;
			pstruEventDetails->szOtherDetails=appInfo;
			fnpAddEvent(pstruEventDetails);
		}
		break;
	case TRAFFIC_ERLANG_CALL:
		{
			APP_CALL_INFO* info=(APP_CALL_INFO*)appInfo->appData;
			int i=fnGetSourceIndex(appInfo,nSource);
			int j=fnGetDestIndex(appInfo,nDestination);
			NetSim_PACKET* packet=info->nextPacket[nSource-1][nDestination-1];
			double arrivalTime=0;
			double packetSize=0;
			if(info->nCallStatus[nSource-1][nDestination-1] ==0 || packet==NULL)
				return 0; //Call is ended
			pstruEventDetails->dEventTime=packet->dEventTime;
			pstruEventDetails->dPacketSize=fnGetPacketSize(packet);
			pstruEventDetails->nApplicationId=appInfo->id;
			pstruEventDetails->nDeviceId=nSource;
			pstruEventDetails->nDeviceType=DEVICE_TYPE(nSource);
			pstruEventDetails->nEventType=APPLICATION_OUT_EVENT;
			pstruEventDetails->nInterfaceId=0;
			pstruEventDetails->nProtocolId=PROTOCOL_APPLICATION;
			pstruEventDetails->nSegmentId=0;
			pstruEventDetails->nSubEventType=0;
			pstruEventDetails->pPacket=packet;
			pstruEventDetails->nPacketId=packet->nPacketId;
			pstruEventDetails->szOtherDetails=appInfo;
			if(info->nAppoutevent==NULL)
			{
				NETSIM_ID i;
				info->nAppoutevent=(unsigned long long**)calloc(NETWORK->nDeviceCount,sizeof* info->nAppoutevent);
				for(i=0;i<NETWORK->nDeviceCount;i++)
					info->nAppoutevent[i]=(unsigned long long*)calloc(NETWORK->nDeviceCount,sizeof* info->nAppoutevent[i]);
			}
			info->nAppoutevent[nSource-1][nDestination-1]=fnpAddEvent(pstruEventDetails);
			//Generate next packet
			fn_NetSim_TrafficGenerator_Voice(&(info->VoiceInfo),&packetSize,&arrivalTime,
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[0]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[1]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[0]),
				&(NETWORK->ppstruDeviceList[nSource-1]->ulSeed[1]));
			if(info->dCallEndTime[nSource-1][nDestination-1] <=time+arrivalTime)
			{
				info->nextPacket[nSource-1][nDestination-1]=NULL;
				packet->pstruAppData->nAppEndFlag=1;//last packet
				return 0;
			}
			info->nextPacket[nSource-1][nDestination-1]=fn_NetSim_Application_GeneratePacket(pstruEventDetails->dEventTime+arrivalTime,
				nSource,nDestination,++appInfo->nPacketId,appInfo->nAppType,Priority_High,QOS_UGS,appInfo->sourcePort[i],appInfo->destPort[j]);
			info->nextPacket[nSource-1][nDestination-1]->pstruAppData->dPacketSize=packetSize;
			info->nextPacket[nSource-1][nDestination-1]->pstruAppData->dPayload=packetSize;
		}
		break;
	case TRAFFIC_BSM_APP:
	{
		int i = fnGetSourceIndex(appInfo, nSource);
		int j = fnGetDestIndex(appInfo, nDestination);
		double arrivalTime = 0;
		double packetSize = 0;
		fn_NetSim_Application_BSM((PAPP_BSM_INFO)appInfo->appData,
								  &packetSize,
								  &arrivalTime,
								  &(NETWORK->ppstruDeviceList[nSource - 1]->ulSeed[0]),
								  &(NETWORK->ppstruDeviceList[nSource - 1]->ulSeed[1]),
								  &(NETWORK->ppstruDeviceList[nSource - 1]->ulSeed[0]),
								  &(NETWORK->ppstruDeviceList[nSource - 1]->ulSeed[1]));

		pstruEventDetails->dEventTime = time + arrivalTime;
		pstruEventDetails->dPacketSize = (double)((int)packetSize);
		pstruEventDetails->nApplicationId = appInfo->id;
		pstruEventDetails->nDeviceId = nSource;
		pstruEventDetails->nDeviceType = DEVICE_TYPE(nSource);
		pstruEventDetails->nEventType = APPLICATION_OUT_EVENT;
		pstruEventDetails->nInterfaceId = 0;
		pstruEventDetails->nProtocolId = PROTOCOL_APPLICATION;
		pstruEventDetails->nSegmentId = 0;
		pstruEventDetails->nSubEventType = 0;
		pstruEventDetails->pPacket = fn_NetSim_Application_GeneratePacket(pstruEventDetails->dEventTime,
																		  nSource, nDestination, ++appInfo->nPacketId, appInfo->nAppType, Priority_Low, QOS_BE, appInfo->sourcePort[i], appInfo->destPort[j]);
		pstruEventDetails->nPacketId = appInfo->nPacketId;
		pstruEventDetails->szOtherDetails = appInfo;
		fnpAddEvent(pstruEventDetails);
	}
	break;
	default:
		fnNetSimError("Unknown application type %d in Generate_next_packet.\n",appInfo->nAppType);
		break;
	}
	return 1;
}

void copy_payload(UINT8 real[],NetSim_PACKET* packet,unsigned int* payload,APP_INFO* info)
{
	u_short i;
	uint32_t key = 16;
	if (payload)
	{
		for (i = 0; i < *payload; i++)
		{
			if (info->encryption == Encryption_XOR)
				real[i] = xor_encrypt('a' + i % 26, 16);
			else
				real[i] = 'a' + i % 26;
		}
		if (info->encryption == Encryption_TEA)
			encryptBlock(real, payload, &key);
		else if (info->encryption == Encryption_AES)
			aes256(real,payload);
		else if(info->encryption==Encryption_DES)
			des(real,payload);
	}
}

int fn_NetSim_Add_DummyPayload(NetSim_PACKET* packet,APP_INFO* info)
{
	if (add_sae_j2735_payload(packet,info))
		return 0; // Payload added by SAE J2735 Module

	while(packet)
	{
		if(!packet->szPayload && packet->pstruAppData && packet->pstruAppData->dPacketSize && wireshark_flag)
		{
			unsigned int size=(unsigned int)packet->pstruAppData->dPacketSize;
			packet->szPayload = (PPACKET_INFO)calloc(1,sizeof* packet->szPayload);
			copy_payload(packet->szPayload->packet,packet,&size,info);
			packet->szPayload->packet_len = size;
			packet->pstruAppData->dPacketSize = (double)size;
		}
		packet=packet->pstruNextPacket;
	}
	return 0;
}

double get_random_startupdelay()
{
	return RANDOM_STARTUP_DELAY*NETSIM_RAND_01();
}
