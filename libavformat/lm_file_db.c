
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libavutil/log.h"
#include "lm_file_db.h"

static struct CustomMediaFileInfo * pMideaFileInfo = NULL;

static struct CustomMediaFileInfo * get_media_file_info_1(const char *filepath);

struct CustomMediaFileInfo * get_media_file_info(const char *vdata_db_path)
{
	if (1)
		return get_media_file_info_1(vdata_db_path);

	pMideaFileInfo = (struct CustomMediaFileInfo *)malloc(sizeof(struct CustomMediaFileInfo) + 3 * sizeof(struct MediaUnitData));
	memset(pMideaFileInfo, 0, sizeof(struct CustomMediaFileInfo)+ 3* sizeof(struct MediaUnitData));

	strcpy(pMideaFileInfo->filename,"01.one");
	pMideaFileInfo->nb_blocks = 3;

	strcpy(pMideaFileInfo->blocks_info[0].filename,"001.vcf");
	pMideaFileInfo->blocks_info[0].block_offset = 0;
	strcpy(pMideaFileInfo->blocks_info[1].filename,"002.vcf");
	pMideaFileInfo->blocks_info[1].block_offset = 518253;
	strcpy(pMideaFileInfo->blocks_info[2].filename,"003.vcf");
	pMideaFileInfo->blocks_info[2].block_offset = 518253+713751;

	return pMideaFileInfo;
}

static struct CustomMediaFileInfo * get_media_file_info_1(const char *filepath)
{
	FILE * file = fopen(filepath, "r");
	if (file == NULL)
	{
		av_log(NULL, AV_LOG_ERROR, "get_midea_file_info_1(): open file failed\n");
		return NULL;
	}

	char buf[100];
	char * pStr;
	int nb_blocks = 0;

	pStr = fgets(buf, sizeof(buf), file);
	if (pStr == NULL)
	{
		av_log(NULL, AV_LOG_ERROR, "get_midea_file_info_1(): fgets failed\n");
		fclose(file);
		return NULL;
	}

	if (strncmp(pStr, "N=", 2) == 0)
	{
		nb_blocks = atoi(pStr + 2);
	}
	else
	{
		av_log(NULL, AV_LOG_ERROR, "get_midea_file_info_1(): N is unknown\n");
		fclose(file);
		return NULL;
	}

	pMideaFileInfo = (struct CustomMediaFileInfo *)malloc(sizeof(struct CustomMediaFileInfo) + nb_blocks * sizeof(struct MediaUnitData));
	memset(pMideaFileInfo, 0, sizeof(struct CustomMediaFileInfo)+ nb_blocks * sizeof(struct MediaUnitData));

	//strcpy(pMideaFileInfo->filename, "vdata.one");
	strcpy(pMideaFileInfo->filename, "vdata.lmv");
	pMideaFileInfo->nb_blocks = nb_blocks;

	char *p1, *p2, *p3, *p4;

	char * filename;
	int num;
	int offset;
	int size;
	int next_block_offset;

	int iLoop = 0;
	while (1)
	{
		pStr = fgets(buf, sizeof(buf), file);
		if (pStr == NULL)
			break;

		if (strncmp(pStr, "file:", 5) == 0)
		{
			p1 = pStr + 5;
			p2 = strchr(p1, ',');
			p3 = strchr(p2 + 1, ',');
			p4 = strchr(p3 + 1, ',');

			// FIXME: atoi() ...
			filename = p2 + 1; *p3 = 0;
			num = atoi(p1);
			offset = atoi(p3 + 1);
			//size = atoi(p4 + 1);
			next_block_offset = atoi(p4 + 1);
			size = next_block_offset - offset;

			strcpy(pMideaFileInfo->blocks_info[iLoop].filename, filename);
			pMideaFileInfo->blocks_info[iLoop].block_num = num;
			pMideaFileInfo->blocks_info[iLoop].block_offset = offset;
			pMideaFileInfo->blocks_info[iLoop].block_size = size;
			pMideaFileInfo->blocks_info[iLoop].next_block_offset = next_block_offset;

			iLoop++;
			if (iLoop == nb_blocks)
				break;
		}

	}

	fclose(file);

	return pMideaFileInfo;
}
