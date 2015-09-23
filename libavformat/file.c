/*
 * buffered file I/O
 * Copyright (c) 2001 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avstring.h"
#include "libavutil/internal.h"
#include "libavutil/opt.h"
#include "avformat.h"
#include <fcntl.h>
#if HAVE_IO_H
#include <io.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <stdlib.h>
#include "os_support.h"
#include "url.h"
#include "lm_file_db.h"

/* Some systems may not have S_ISFIFO */
#ifndef S_ISFIFO
#  ifdef S_IFIFO
#    define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#  else
#    define S_ISFIFO(m) 0
#  endif
#endif

/* standard file protocol */

typedef struct FileContext {
    const AVClass *class;
    int fd;
    int trunc;
    int blocksize;
} FileContext;

static const AVOption file_options[] = {
    { "truncate", "truncate existing files on write", offsetof(FileContext, trunc), AV_OPT_TYPE_INT, { .i64 = 1 }, 0, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { "blocksize", "set I/O operation maximum block size", offsetof(FileContext, blocksize), AV_OPT_TYPE_INT, { .i64 = INT_MAX }, 1, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { NULL }
};

static const AVOption pipe_options[] = {
    { "blocksize", "set I/O operation maximum block size", offsetof(FileContext, blocksize), AV_OPT_TYPE_INT, { .i64 = INT_MAX }, 1, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { NULL }
};

static const AVClass file_class = {
    .class_name = "file",
    .item_name  = av_default_item_name,
    .option     = file_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const AVClass pipe_class = {
    .class_name = "pipe",
    .item_name  = av_default_item_name,
    .option     = pipe_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

/**
 * 注意：这里的filename是带有一个结构体指针的。
 *
 * vcf (Virtual Concat File) 文件名格式：
 * 001.vcf_%p
 * 002.vcf_%p
 * 003.vcf_%p
 */
static int is_vcf_file(const char *filename)
{
	if (av_strncasecmp(filename + 3, ".vcf", 4) == 0)
		return 1;
	else
		return 0;
}

static void get_filename_and_ptr(char *oldFilename, char *newFilename, void **ptr)
{
	strncpy(newFilename, oldFilename, 7);
	newFilename[7] = 0;
	*ptr = Str_2_Ptr(oldFilename + 8);
}

/**
 * 根据文件名得到对应的数组索引。
 * e.g:
 *    "001.vcf" --> 0
 */
static int filename_2_index(const char *filename)
{
	if (!is_vcf_file(filename))
		return -1;

	char *p = NULL;
	int num = 0;

	if (av_strncasecmp(filename, "00", 2) == 0)
	{
		p = filename + 2;
	}
	else if (av_strncasecmp(filename, "0", 1) == 0)
	{
		p = filename + 1;
	}
	else
	{
		p = filename;
	}

	num = atoi(p);

	return num - 1;
}

static int file_read(URLContext *h, unsigned char *buf, int size)
{
    FileContext *c = h->priv_data;
    int r;
    size = FFMIN(size, c->blocksize);
    r = read(c->fd, buf, size);
    return (-1 == r)?AVERROR(errno):r;
}

static int file_read_2(URLContext *h, unsigned char *buf, int size)
{
    FileContext *c = h->priv_data;
    int r;
    size = FFMIN(size, c->blocksize);
    r = read(c->fd, buf, size); // XXX:这里可以加个验证
    return (-1 == r)?AVERROR(errno):r;
}

static int file_write(URLContext *h, const unsigned char *buf, int size)
{
    FileContext *c = h->priv_data;
    int r;
    size = FFMIN(size, c->blocksize);
    r = write(c->fd, buf, size);
    return (-1 == r)?AVERROR(errno):r;
}

static int file_write_2(URLContext *h, const unsigned char *buf, int size)
{
	if (!is_vcf_file(h->filename))
		return file_write(h, buf, size);

    return AVERROR(EACCES);
}

static int file_get_handle(URLContext *h)
{
    FileContext *c = h->priv_data;
    return c->fd;
}

static int file_get_handle_2(URLContext *h)
{
    FileContext *c = h->priv_data;
    return c->fd;
}

static int file_check(URLContext *h, int mask)
{
    int ret = 0;
    const char *filename = h->filename;
    av_strstart(filename, "file:", &filename);

    {
#if HAVE_ACCESS && defined(R_OK)
    if (access(filename, F_OK) < 0)
        return AVERROR(errno);
    if (mask&AVIO_FLAG_READ)
        if (access(filename, R_OK) >= 0)
            ret |= AVIO_FLAG_READ;
    if (mask&AVIO_FLAG_WRITE)
        if (access(filename, W_OK) >= 0)
            ret |= AVIO_FLAG_WRITE;
#else
    struct stat st;
    ret = stat(filename, &st);
    if (ret < 0)
        return AVERROR(errno);

    ret |= st.st_mode&S_IRUSR ? mask&AVIO_FLAG_READ  : 0;
    ret |= st.st_mode&S_IWUSR ? mask&AVIO_FLAG_WRITE : 0;
#endif
    }
    return ret;
}

static int file_check_2(URLContext *h, int mask)
{
	if (!is_vcf_file(h->filename))
		return file_check(h, mask);

    return AVIO_FLAG_READ;
}

#if CONFIG_FILE_PROTOCOL

static int file_open(URLContext *h, const char *filename, int flags)
{
    FileContext *c = h->priv_data;
    int access;
    int fd;
    struct stat st;

    av_strstart(filename, "file:", &filename);

    if (flags & AVIO_FLAG_WRITE && flags & AVIO_FLAG_READ) {
        access = O_CREAT | O_RDWR;
        if (c->trunc)
            access |= O_TRUNC;
    } else if (flags & AVIO_FLAG_WRITE) {
        access = O_CREAT | O_WRONLY;
        if (c->trunc)
            access |= O_TRUNC;
    } else {
        access = O_RDONLY;
    }
#ifdef O_BINARY
    access |= O_BINARY;
#endif
    fd = avpriv_open(filename, access, 0666);
    if (fd == -1)
        return AVERROR(errno);
    c->fd = fd;

    h->is_streamed = !fstat(fd, &st) && S_ISFIFO(st.st_mode);

    return 0;
}

//static int s_one_fd = -1;
//
//static struct CustomMediaFileInfo * pFileInfo = NULL;

static int file_open_2(URLContext *h, const char *filename, int flags)
{
	if (!is_vcf_file(h->filename))
		return file_open(h, filename, flags);

	char newFilename[10];
	void *newPtr = NULL;
	get_filename_and_ptr(h->filename, newFilename, &newPtr);
	av_log(NULL, AV_LOG_ERROR, "get_filename_and_ptr(): newFilename = %s newPtr = %p\n", newFilename, newPtr);

    FileContext *c = h->priv_data;
    //int fd;
    int ret;

    struct CustomMediaFileInfo *pFileInfo = (struct CustomMediaFileInfo *) newPtr;

//    if (pFileInfo == NULL)
//    	pFileInfo = get_media_file_info();
//
//    if (pFileInfo == NULL)
//    {
//        av_log(h, AV_LOG_ERROR, "Failed to get media file info\n");
//    	return -1;
//    }

    for (int i = 0; i < pFileInfo->nb_blocks; i++)
    {
    	if (av_strncasecmp(newFilename, pFileInfo->blocks_info[i].filename, 7) == 0)
    	{
    		if (i == 0)
    		{
//    			fd = avpriv_open(pFileInfo->filename, O_RDONLY | O_BINARY, 0666);
//    			if (fd == -1)
//    			{
//    				av_log(h, AV_LOG_ERROR, "open the media file failed\n");
//    				return AVERROR(errno);
//    			}
    			c->fd = pFileInfo->fd;
    			//s_one_fd = fd;
    			ret = lseek(c->fd, 0, SEEK_SET);

    			return 0;
    		}
    		else
    		{
    			//c->fd = s_one_fd;
    			c->fd = pFileInfo->fd;
    			h->is_streamed = 0;

    		    ret = lseek(c->fd, pFileInfo->blocks_info[i].block_offset, SEEK_SET);

    		    return 0;//ret < 0 ? AVERROR(errno) : ret;
    		}

    		break;
    	}

    }
	/*if (av_strcasecmp(filename, "001.vcf") == 0)
	{
		fd = avpriv_open("01.one", O_RDONLY | O_BINARY, 0666);
		//fd = avpriv_open("01.mp4", O_RDONLY | O_BINARY, 0666);
	    if (fd == -1)
	        return AVERROR(errno);

		c->fd = fd;
		h->is_streamed = 0;

		s_one_fd = fd;

		ret = lseek(c->fd, 0, SEEK_SET);

		return 0;
	}
	else if (av_strcasecmp(filename, "002.vcf") == 0)
	{
		c->fd = s_one_fd;
		h->is_streamed = 0;

	    ret = lseek(c->fd, 518253, SEEK_SET);

	    return 0;//ret < 0 ? AVERROR(errno) : ret;
	}
	else if (av_strcasecmp(filename, "003.vcf") == 0)
	{
		c->fd = s_one_fd;
		h->is_streamed = 0;

	    ret = lseek(c->fd, 518253+713751, SEEK_SET);

	    return 0;//ret < 0 ? AVERROR(errno) : ret;
	}*/

    return 0;
}

/* XXX: use llseek */
static int64_t file_seek(URLContext *h, int64_t pos, int whence)
{
    FileContext *c = h->priv_data;
    int64_t ret;

    if (whence == AVSEEK_SIZE) {
        struct stat st;
        ret = fstat(c->fd, &st);
        return ret < 0 ? AVERROR(errno) : (S_ISFIFO(st.st_mode) ? 0 : st.st_size);
    }

    ret = lseek(c->fd, pos, whence);

    return ret < 0 ? AVERROR(errno) : ret;
}

static int64_t file_seek_2(URLContext *h, int64_t pos, int whence)
{
	if (!is_vcf_file(h->filename))
		return file_seek(h, pos, whence);

	char newFilename[10];
	void *newPtr = NULL;
	get_filename_and_ptr(h->filename, newFilename, &newPtr);
	av_log(NULL, AV_LOG_ERROR, "get_filename_and_ptr(): newFilename = %s newPtr = \n", newFilename, newPtr);

    FileContext *c = h->priv_data;
    int64_t ret;

    int index = filename_2_index(newFilename);
    //struct MediaUnitData * pBlockInfo = &pFileInfo->blocks_info[index];
    struct CustomMediaFileInfo *pFileInfo = (struct CustomMediaFileInfo *) newPtr;
    struct MediaUnitData *pBlockInfo = &pFileInfo->blocks_info[index];

    // 通过 AVSEEK_SIZE 获取文件大小
    if (whence == AVSEEK_SIZE) {
    	ret = pBlockInfo->block_size;
    	return ret;

//    	if (av_strcasecmp(h->filename, "001.vcf") == 0)
//    	{
//    		ret = 518253;
//    		return ret;
//    	}
//    	else if (av_strcasecmp(h->filename, "002.vcf") == 0)
//    	{
//    		ret = 713751;
//    		return ret;
//    	}
//    	else if (av_strcasecmp(h->filename, "003.vcf") == 0)
//    	{
//    		ret = 836689;
//    		return ret;
//    	}
    }

    switch (whence)
    {
    case SEEK_SET: // FIXME!!!
		ret = lseek(c->fd, pBlockInfo->block_offset, SEEK_SET);
		ret = lseek(c->fd, pos, SEEK_CUR);
		return pos;

//    	if (av_strcasecmp(h->filename, "001.vcf") == 0)
//    	{
//    		ret = lseek(c->fd, 0, SEEK_SET);
//    		ret = lseek(c->fd, pos, SEEK_CUR);
//    		return pos;
//    	}
//    	else if (av_strcasecmp(h->filename, "002.vcf") == 0)
//    	{
//    		ret = lseek(c->fd, 518253, SEEK_SET);
//    		ret = lseek(c->fd, pos, SEEK_CUR);
//    		return pos;
//    	}
//    	else if (av_strcasecmp(h->filename, "003.vcf") == 0)
//    	{
//    		ret = lseek(c->fd, 518253 + 713751, SEEK_SET);
//    		ret = lseek(c->fd, pos, SEEK_CUR);
//    		return pos;
//    	}
    	break;
    case SEEK_CUR:
    	ret = lseek(c->fd, pos, whence);
    	break;
    case SEEK_END: // FIXME!!!
    	ret = -1;
    	break;
    }

    return ret < 0 ? AVERROR(errno) : ret;
}

static int file_close(URLContext *h)
{
    FileContext *c = h->priv_data;
    return close(c->fd);
}

static int file_close_2(URLContext *h)
{
	if (!is_vcf_file(h->filename))
		return file_close(h);

//    FileContext *c = h->priv_data;
//    return close(c->fd);
	return 0;
}

#define LIMAO_IJK_PLAYER 1

#ifdef LIMAO_IJK_PLAYER

URLProtocol ff_file_protocol = {
    .name                = "file",
    .url_open            = file_open_2, // ok
    .url_read            = file_read_2,
    .url_write           = file_write_2, // ok
    .url_seek            = file_seek_2,
    .url_close           = file_close_2, // ok
    .url_get_file_handle = file_get_handle_2, // ok
    .url_check           = file_check_2, // ok
    .priv_data_size      = sizeof(FileContext),
    .priv_data_class     = &file_class,
};

#else

URLProtocol ff_file_protocol = {
    .name                = "file",
    .url_open            = file_open,
    .url_read            = file_read,
    .url_write           = file_write,
    .url_seek            = file_seek,
    .url_close           = file_close,
    .url_get_file_handle = file_get_handle,
    .url_check           = file_check,
    .priv_data_size      = sizeof(FileContext),
    .priv_data_class     = &file_class,
};

#endif

#endif /* CONFIG_FILE_PROTOCOL */

#if CONFIG_PIPE_PROTOCOL

static int pipe_open(URLContext *h, const char *filename, int flags)
{
    FileContext *c = h->priv_data;
    int fd;
    char *final;
    av_strstart(filename, "pipe:", &filename);

    fd = strtol(filename, &final, 10);
    if((filename == final) || *final ) {/* No digits found, or something like 10ab */
        if (flags & AVIO_FLAG_WRITE) {
            fd = 1;
        } else {
            fd = 0;
        }
    }
#if HAVE_SETMODE
    setmode(fd, O_BINARY);
#endif
    c->fd = fd;
    h->is_streamed = 1;
    return 0;
}

URLProtocol ff_pipe_protocol = {
    .name                = "pipe",
    .url_open            = pipe_open,
    .url_read            = file_read,
    .url_write           = file_write,
    .url_get_file_handle = file_get_handle,
    .url_check           = file_check,
    .priv_data_size      = sizeof(FileContext),
    .priv_data_class     = &pipe_class,
};

#endif /* CONFIG_PIPE_PROTOCOL */
