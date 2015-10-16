#ifndef AVFORMAT_LM_FILE_DB_H
#define AVFORMAT_LM_FILE_DB_H

#include <stdint.h>

struct MediaUnitData
{
	char filename[12];
	int block_num;

//	int64_t block_offset;
//	int64_t block_size;
//	int64_t block_duration;
//
//	int64_t next_block_offset;

	// FIXME: 2G文件大小限制 ...
	int block_offset;
	int block_size;
	int block_duration;

	int next_block_offset;
};

struct CustomMediaFileInfo
{
	int fd;

	char filename[1024];
	int64_t duration;
	int nb_streams;
	int nb_blocks;
	struct MediaUnitData blocks_info[0];
};

struct CustomMediaFileInfo * get_media_file_info(const char *vdata_db_path);

#endif // AVFORMAT_LM_FILE_DB_H
