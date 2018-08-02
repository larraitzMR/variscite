/*
 * reader_params.c
 *
 *  Created on: March 22, 2018
 *      Author: Larraitz
 */

#include <inttypes.h>
#include "m6e/tmr_utils.h"
#include "reader_params.h"

static const char *gen2LinkFrequencyNames[] = { "LINK250KHZ", "LINK300KHZ",	"LINK320KHZ", "LINK40KHZ", "LINK640KHZ" };
static const char *sessionNames[] = { "S0", "S1", "S2", "S3" };
static const char *targetNames[] = { "A", "B", "AB", "BA" };
static const char *tagEncodingNames[] = {"FM0", "M2", "M4", "M8"};
static const char *tariNames[] = { "TARI_25US", "TARI_12_5US", "TARI_6_25US" };
static const char *regions[] = {"UNSPEC", "NA", "EU", "KR", "IN", "JP", "PRC", "EU2", "EU3", "KR2", "PRC2", "AU", "NZ", "NA2", "NA3", "IS"};


void printU8List(TMR_uint8List *list, int *ports) {
	int i;

	putchar('[');
	for (i = 0; i < list->len && i < list->max; i++) {
		printf("%u%s", list->list[i], ((i + 1) == list->len) ? "" : ",");
		ports[i] = list->list[i];
	}
	if (list->len > list->max) {
		printf("...");
	}
	putchar(']');
	printf("\n");
}

static bool
strcasecmpcount(const char *a, const char *b, int *matches)
{
  int i = 0;

  while (*a && *b &&
    (tolower((unsigned char)*a) == tolower((unsigned char)*b)))
  {
    i++;
    a++;
    b++;
  }
  if (matches)
  {
    *matches = i;
  }
  return (*a == *b);
}

int listid(const char *list[], int listlen, const char *name)
{
  int i, len, match, bestmatch, bestmatchindex;

  bestmatch = 0;
  bestmatchindex = -1;
  len = (int)strlen(name);
  for (i = 0; i < listlen; i++)
  {
    if (NULL != list[i])
    {
      if (strcasecmpcount(name, list[i], &match))
      {
        return i; /* Exact match - return immediately */
      }
      if (match == len)
      {
        if (bestmatch == 0)
        {
          /* Prefix match - return if nothing else conflicts */
          bestmatch = match;
          bestmatchindex = i;
        }
        else
        {
          /* More than one prefix match of the same length - ambiguous */
          bestmatchindex = -1;
        }
      }
    }
  }
  return bestmatchindex;
}

char *regionName(TMR_Region region)
{
  if (region == TMR_REGION_OPEN)
  {
    return "OPEN";
  }
  return listname(regions, numberof(regions), region);
}

void getRegionNames(TMR_Reader *rp, int *reg)
{
	printf("GET REGION NAMES\n");
	TMR_Status ret;
	TMR_RegionList value;
	TMR_Region valueList[32];
	int i;

	value.max = numberof(valueList);
	value.list = valueList;

	ret = TMR_paramGet(rp, TMR_PARAM_REGION_SUPPORTEDREGIONS, &value);

	printf("[");
	for (i = 0; i < value.len && i < value.max; i++) {

		printf("%s%s", regionName(value.list[i]), ((i + 1) == value.len) ? "" : ",");
		//strcpy(reg[i],regionName(value.list[i]));
		//printf("%s", reg[i]);
		reg[i] = value.list[i];
	}
	if (value.len > value.max){
		printf("...");
	}
	printf("]");
	printf("\n");
}


int getRegionNumber(char * name)
{
	int len = sizeof(regions)/sizeof(regions[0]);
	int i;

	for(i = 0; i < len; ++i)
	{
	    if(strcmp(regions[i], name)==0){
	    	printf("Numero Region: %d\n", i);
	    	return i;
	    }
	}
}

int getID(char * name, char * lista)
{
	if(strcmp(lista, "Tari")==0){
		return listid(tariNames, sizeof(tariNames), name);
	} else if(strcmp(lista, "M")==0){
		return listid(tagEncodingNames, sizeof(tagEncodingNames), name);
	} else if(strcmp(lista, "Target")==0){
		return listid(targetNames, sizeof(targetNames), name);
	}
}

void getAntennaList(char *lchar, TMR_uint8List *list) {
	int i = 0;
	char* p;
	list->len = 0;
	list->max = 4;
	list->list = malloc(sizeof(list->list[0]) * list->max);
	putchar('[');
	for (p = strtok(lchar + 1, " "); p; p = strtok(NULL, " ")) {
		int a = atoi(p);
		if (a == 0){}
		else {
			list->list[list->len] = a;
			list->len++;
		}
		printf("%u%s", list->list[i], ((i + 1) == list->len) ? "" : ",");
		i++;
	}
	putchar(']');
	printf("\n");
}

void getConnectedAntennaPorts(TMR_Reader *rp, int *puertos) {
	TMR_Status ret;
	TMR_uint8List value;
	uint8_t valueList[64];
	value.max = numberof(valueList);
	value.list = valueList;
	ret = TMR_paramGet(rp, TMR_PARAM_ANTENNA_CONNECTEDPORTLIST, &value);
	printU8List(&value, puertos);
}

void getAntennaPorts(TMR_Reader *rp, int *p) {
	TMR_Status ret;
	TMR_uint8List value;
	uint8_t valueList[64];
	value.max = numberof(valueList);
	value.list = valueList;
	ret = TMR_paramGet(rp, TMR_PARAM_ANTENNA_PORTLIST, &value);
	printU8List(&value, p);
	for (int var = 0; var < 4; ++var) {
		printf("Puertos: %d ", p[var]);
	}
	printf("\n");
}

void getSelectedAntennas(TMR_Reader *rp, int *p) {
	TMR_Status ret;
	//TMR_uint8List value;
	TMR_ReadPlan plan;
	ret = TMR_paramGet(rp, TMR_PARAM_READ_PLAN, &plan);
	printU8List(&plan.u.simple.antennas, p);
	for (int var = 0; var < 4; ++var) {
		printf("Puertos: %d ", p[var]);
	}
	printf("\n");
}

void getReaderInfo(TMR_Reader *rp, TMR_Param key, char *inf) {
	TMR_String info;
	TMR_Status ret;
	//char inf[1];
	char stringValue[256];
	info.max = sizeof(stringValue);
	info.value = stringValue;

	ret = TMR_paramGet(rp, key, &info);
	if (TMR_SUCCESS != ret) {
		printf("Error getting Reader Info: %s\n", ret);
	}
	strcpy(inf, info.value);
}

void **getParam(TMR_Reader *rp, TMR_Param key) {
	TMR_Status ret;

	if (TMR_PARAM_RADIO_POWERMAX == key) {
		uint16_t value;
		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		//printf("%u\n", value);
		return value;
	} else if (TMR_PARAM_RADIO_POWERMIN == key) {
		uint16_t value;
		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		//printf("%u\n", value);
		return value;
	} else if (TMR_PARAM_ANTENNA_CHECKPORT == key) {
		bool value;
		uint16_t v;
		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		printf("%s\n", value ? "true" : "false");
		v = value;
		//printf("V: %d\n", v);
		return v;
	} else if (TMR_PARAM_RADIO_READPOWER == key) {
		uint32_t value;
		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		printf("Power: %u\n", value);
		return value;
	} else if (TMR_PARAM_GEN2_SESSION == key) {
		TMR_GEN2_Session value;
		char *s;

		ret = TMR_paramGet(rp, key, &value);
		//printf("SESSION: %d\n", value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		s = listname(sessionNames, numberof(sessionNames), value);
		//printf("SESSION: %s\n", s);
		return s;
	} else if (TMR_PARAM_GEN2_TARGET == key) {
		TMR_GEN2_Target value;
		char *s;

		ret = TMR_paramGet(rp, key, &value);
		printf("TARGET: %d\n", value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		s = listname(targetNames, numberof(targetNames), value);
		//printf("TARGET: %s\n", s);
		return s;
	} else if (TMR_PARAM_GEN2_TARI == key) {
		TMR_GEN2_Tari value;
		char *s;

		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		printf("TARI: %d\n", value);
		s = listname(tariNames, numberof(tariNames), value);
		return s;
	} else if (TMR_PARAM_GEN2_BLF == key) {
		TMR_GEN2_LinkFrequency value;
		char *s;

		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS == ret) {
			int temp = value;
			switch (temp) {
			case 250:
				value = 0;
				break;
			case 640:
				value = 4;
				break;
			case 320:
				value = 2;
				break;
			default:;
			}
			s = listname(gen2LinkFrequencyNames, numberof(gen2LinkFrequencyNames), value);
		}
		printf("BLF: %d\n", value);
		return s;
	} else if (TMR_PARAM_GEN2_TAGENCODING == key) {
		TMR_GEN2_TagEncoding value;
		char *s;

		ret = TMR_paramGet(rp, key, &value);
		if (TMR_SUCCESS != ret) {
			goto out;
		}
		s = listname(tagEncodingNames, numberof(tagEncodingNames), value);
		printf("ENCODING: %d\n", value);
		return s;
	} else if (TMR_PARAM_GEN2_Q == key) {

		TMR_SR_GEN2_Q value;
		char *s = "";

		ret = TMR_paramGet(rp, key, &value);
		//printf("Q: %d\n", value);
		if (TMR_SUCCESS == ret)
		{
		  if (value.type == TMR_SR_GEN2_Q_DYNAMIC)
		  {
			  s = "DynamicQ";
		  }
		  else if (value.type == TMR_SR_GEN2_Q_STATIC)
		  {
			  s = "StaticQ";
			  s += value.u.staticQ.initialQ;
		  }
		}
		return s;
	} else if (TMR_PARAM_REGION_ID == key) {

		TMR_Region value;
		char *s = "";

		ret = TMR_paramGet(rp, key, &value);
		 if (value == TMR_REGION_OPEN)
		  {
		    return "OPEN";
		  }
		s =  listname(regions, numberof(regions), value);
		return s;
	}
	out: if (TMR_SUCCESS != ret) {
		printf("Error retrieving value: %s\n", TMR_strerr(rp, ret));
	}
}

