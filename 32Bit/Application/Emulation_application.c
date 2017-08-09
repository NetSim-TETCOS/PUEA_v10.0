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

int fn_NetSim_Emulation_InitApplication(APP_INFO* appInfo);

int fn_NetSim_Application_ConfigureEmulationTraffic(APP_INFO* appInfo,void* xmlNetSimNode)
{
	char* szVal;
	APP_EMULATION_INFO* info=(APP_EMULATION_INFO*)calloc(1,sizeof* info);
	void* xmlChild;
	appInfo->appData=info;
	xmlChild=fn_NetSim_xmlGetChildElement(xmlNetSimNode,"EMULATION",0);
	if(xmlChild)
	{
		szVal=fn_NetSim_xmlConfig_GetVal(xmlChild,"SOURCE_REAL_IP",1);
		if(szVal && strcmp(szVal,"0.0.0.0"))
			info->realSourceIP=STR_TO_IP4(szVal);
		free(szVal);
		szVal=fn_NetSim_xmlConfig_GetVal(xmlChild,"DESTINATION_REAL_IP",1);
		if(szVal && strcmp(szVal,"0.0.0.0"))
			info->realDestIP=STR_TO_IP4(szVal);
		free(szVal);
		szVal=fn_NetSim_xmlConfig_GetVal(xmlChild,"SOURCE_PORT",0);
		if(szVal)
			info->nSourcePort=atoi(szVal);
		free(szVal);
		szVal=fn_NetSim_xmlConfig_GetVal(xmlChild,"DESTINATION_PORT",0);
		if(szVal)
			info->nDestinationPort=atoi(szVal);
		free(szVal);
	}
	else
		return 0;
	//Assign other value
	info->nDestinationId=appInfo->destList[0];
	info->nSourceId=appInfo->sourceList[0];
	info->simDestIP=fn_NetSim_Stack_GetIPAddressAsId(info->nDestinationId,1);
	info->simSourceIP=fn_NetSim_Stack_GetIPAddressAsId(info->nSourceId,1);
	fn_NetSim_Emulation_InitApplication(appInfo);
	//Add the emulation environment variable
	_putenv("NETSIM_EMULATOR=1");
	return 0;
}
int fn_NetSim_Emulation_InitApplication(APP_INFO* appInfo)
{
	unsigned int i,j;
	APP_EMULATION_INFO* info = appInfo->appData;

	if(appInfo->sourcePort==NULL)
		appInfo->sourcePort=calloc(appInfo->nSourceCount,sizeof* appInfo->sourcePort);
	if(appInfo->destPort==NULL)
		appInfo->destPort=calloc(appInfo->nDestCount,sizeof* appInfo->destPort);
	if(appInfo->appMetrics==NULL)
		appInfo->appMetrics=calloc(appInfo->nSourceCount,sizeof* appInfo->appMetrics);
	for(i=0;i<appInfo->nSourceCount;i++)
	{
		NETSIM_ID nSource;
		if(appInfo->appMetrics[i]==NULL)
			appInfo->appMetrics[i]=calloc(appInfo->nDestCount,sizeof* appInfo->appMetrics[i]);
		nSource=appInfo->sourceList[i];
		appInfo->sourcePort[i]=rand()*65500;
		for(j=0;j<appInfo->nDestCount;j++)
		{
			NETSIM_ID nDestination=appInfo->destList[j];
			if(appInfo->appMetrics[i][j]==NULL)
				appInfo->appMetrics[i][j]=calloc(1,sizeof* appInfo->appMetrics[i][j]);
			appInfo->destPort[j]=rand()*65500;
			appInfo->appMetrics[i][j]->nSourceId=nSource;
			appInfo->appMetrics[i][j]->nDestinationId=nDestination;
			appInfo->appMetrics[i][j]->nApplicationId=appInfo->nConfigId;
			//Create the socket buffer
			fnCreateSocketBuffer(appInfo,nSource,nDestination,appInfo->sourcePort[i],appInfo->destPort[j]);

		}
	}
	return 0;
}
