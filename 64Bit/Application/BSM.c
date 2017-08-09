/************************************************************************************
* Copyright (C) 2016                                                               *
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
#include "Application.h"

int fn_NetSim_Application_BSM(PAPP_BSM_INFO info,
							  double* fSize,
							  double* ldArrival,
							  unsigned long* uSeed,
							  unsigned long* uSeed1,
							  unsigned long* uSeed2,
							  unsigned long* uSeed3)
{
	double time = 0.0;
	do
	{
		fnDistribution(info->packetSizeDistribution, fSize, uSeed, uSeed1, &(info->dPacketSize));
	} while (*fSize <= 1.0);
	//Call the Distribution DLL to generate inter arrival time
	do
	{
		fnDistribution(info->IATDistribution, &time, uSeed, uSeed1, &(info->dIAT));
	} while (time <= 0.0);
	*ldArrival = *ldArrival + time;

	//generate random time in range of[-5,5]
	time = NETSIM_RAND_RN(info->dRandomWaitTime*-1, info->dRandomWaitTime);
	*ldArrival += time;
	return 1;
}

/** This function is used to start the Database, FTP and Custom applications */
int fn_NetSim_Application_StartBSM(APP_INFO* appInfo, double time, NETSIM_ID nSourceId, NETSIM_ID nDestId)
{
	unsigned int i, j;
	PAPP_BSM_INFO info = (PAPP_BSM_INFO)appInfo->appData;

	if (appInfo->dEndTime <= time)
		return 0;

	if (appInfo->sourcePort == NULL)
		appInfo->sourcePort = (unsigned int*)calloc(appInfo->nSourceCount, sizeof* appInfo->sourcePort);

	if (appInfo->destPort == NULL)
		appInfo->destPort = (unsigned int*)calloc(appInfo->nDestCount, sizeof* appInfo->destPort);

	if (appInfo->appMetrics == NULL)
		appInfo->appMetrics = (struct stru_Application_Metrics***)calloc(appInfo->nSourceCount, sizeof* appInfo->appMetrics);

	for (i = 0; i < appInfo->nSourceCount; i++)
	{
		NETSIM_ID nSource;
		if (appInfo->appMetrics[i] == NULL)
		{
			if (appInfo->nTransmissionType == UNICAST)
				appInfo->appMetrics[i] = (struct stru_Application_Metrics**)calloc(appInfo->nDestCount, sizeof* appInfo->appMetrics[i]);
			else if (appInfo->nTransmissionType == BROADCAST)
				appInfo->appMetrics[i] = (struct stru_Application_Metrics**)calloc(NETWORK->nDeviceCount, sizeof* appInfo->appMetrics[i]);
		}
		nSource = appInfo->sourceList[i];
		if (nSourceId && nSourceId != nSource)
			continue;
		appInfo->sourcePort[i] = rand() * 65535 / RAND_MAX;
		for (j = 0; j < appInfo->nDestCount; j++)
		{
			NETSIM_ID nDestination = appInfo->destList[j];
			double arrivalTime = 0;
			double packetSize = 0;
			if (nDestId && nDestId != nDestination)
				continue;

			appInfo->destPort[j] = rand()*MAX_PORT / RAND_MAX;

			if (appInfo->appMetrics[i][j] == NULL)
			{
				if (nDestination)
				{
					appInfo->appMetrics[i][j] = (struct stru_Application_Metrics*)calloc(1, sizeof* appInfo->appMetrics[i][j]);
					appInfo->appMetrics[i][j]->nSourceId = nSource;
					appInfo->appMetrics[i][j]->nDestinationId = nDestination;
					appInfo->appMetrics[i][j]->nApplicationId = appInfo->nConfigId;
				}
				else
				{
					NETSIM_ID k;
					for (k = 0; k < NETWORK->nDeviceCount; k++)
					{
						appInfo->appMetrics[i][k] = (struct stru_Application_Metrics*)calloc(1, sizeof* appInfo->appMetrics[i][k]);
						appInfo->appMetrics[i][k]->nSourceId = nSource;
						appInfo->appMetrics[i][k]->nDestinationId = k + 1;
						appInfo->appMetrics[i][k]->nApplicationId = appInfo->nConfigId;
					}
				}
			}

			//Create the socket buffer
			fnCreateSocketBuffer(appInfo, nSource, nDestination, appInfo->sourcePort[i], appInfo->destPort[j]);

			//Generate the app start event
			fn_NetSim_Application_BSM((PAPP_BSM_INFO)appInfo->appData,
									  &packetSize,
									  &arrivalTime,
									  &(NETWORK->ppstruDeviceList[nSource - 1]->ulSeed[0]),
									  &(NETWORK->ppstruDeviceList[nSource - 1]->ulSeed[1]),
									  &(NETWORK->ppstruDeviceList[nSource - 1]->ulSeed[0]),
									  &(NETWORK->ppstruDeviceList[nSource - 1]->ulSeed[1]));
			pstruEventDetails->dEventTime = time + arrivalTime;
			pstruEventDetails->dPacketSize = packetSize;
			pstruEventDetails->nApplicationId = appInfo->id;
			pstruEventDetails->nDeviceId = nSource;
			pstruEventDetails->nDeviceType = DEVICE_TYPE(nSource);
			pstruEventDetails->nEventType = TIMER_EVENT;
			pstruEventDetails->nInterfaceId = 0;
			pstruEventDetails->nPacketId = 0;
			pstruEventDetails->nProtocolId = PROTOCOL_APPLICATION;
			pstruEventDetails->nSegmentId = 0;
			pstruEventDetails->nSubEventType = event_APP_START;
			pstruEventDetails->pPacket = fn_NetSim_Application_GeneratePacket(pstruEventDetails->dEventTime,
																			  nSource,
																			  nDestination,
																			  ++appInfo->nPacketId,
																			  appInfo->nAppType,
																			  Priority_High,
																			  QOS_BE,
																			  appInfo->sourcePort[i],
																			  appInfo->destPort[j]);
			pstruEventDetails->szOtherDetails = appInfo;
			fnpAddEvent(pstruEventDetails);
		}
	}
	return 1;
}

/* Below function are out of scope for NetSim.
 * An User can modify these function to implement Vanet packet type.
 */

bool add_sae_j2735_payload(NetSim_PACKET* packet, APP_INFO* info)
{
	// Add the payload based on SAE J2735 or any other standard
	// return true after adding.
	return false;
}

void process_saej2735_packet(NetSim_PACKET* packet)
{
	//Add the code to process the SAE J2735 packet.
	//This function is called in Application_IN event.
}