/*
Copyright 2019 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#include <zip.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include "write_zip.h"

static int str_is_ascii(char const *str)
    {
    while(*str != 0)
        {
        if(!isascii(*str))
            return 0;

        ++str;
        }

    return 1;
    }

static void *z_open(void *opaque, void const *filename, int mode)
    {
    FILE *file = NULL;
    const char *mode_fopen = NULL;

    if(opaque != NULL)
        return (FILE *)opaque;

    if((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";

    else if(mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";

    else if(mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if(mode_fopen != NULL && filename != NULL)
        file = fopen((const char*)filename, mode_fopen);

    return file;
    }

static uLong z_read(void *opaque, void *stream, void *buf, uLong size)
    {
    uLong ret;

    (void)opaque; /* Unused */

    ret = (uLong)fread(buf, 1, (size_t)size, (FILE *)stream);
    return ret;
    }

static uLong z_write(void *opaque, void *stream, const void *buf, uLong size)
    {
    uLong ret;

    (void)opaque; /* Unused */

    ret = (uLong)fwrite(buf, 1, (size_t)size, (FILE *)stream);
    return ret;
    }

static ZPOS64_T z_tell64(void *opaque, void *stream)
    {
    ZPOS64_T ret;

    (void)opaque; /* Unused */

    ret = ftello((FILE *)stream);
    return ret;
    }

static long z_seek64(void *opaque, void *stream, ZPOS64_T offset, int origin)
{
    int fseek_origin=0;
    long ret = 0;

    (void)opaque; /* Unused */

    switch(origin)
        {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = SEEK_SET;
        break;
    default: return -1;
        }

    if(fseeko((FILE *)stream, offset, fseek_origin) != 0)
        ret = -1;

    return ret;
    }

static int z_close(void *opaque, void *stream)
    {
    int ret;

    if(opaque)
        ret = fflush((FILE *)stream);
    else
        ret = fclose((FILE *)stream);

    return ret;
    }

static int z_error(void *opaque, void *stream)
    {
    int ret;

    (void)opaque; /* Unused */

    ret = ferror((FILE *)stream);
    return ret;
    }

static void fill_z_func(zlib_filefunc64_def *zff)
    {
    zff->zopen64_file = &z_open;
    zff->zread_file = &z_read;
    zff->zwrite_file = &z_write;
    zff->ztell64_file = &z_tell64;
    zff->zseek64_file = &z_seek64;
    zff->zclose_file = &z_close;
    zff->zerror_file = &z_error;
    zff->opaque = NULL;
    }

int write_zip(FILE *fout, char const *data_name, unsigned char const *data, size_t data_size)
    {
    zlib_filefunc64_def zff;
    zipFile out;
    zip_fileinfo zfi;
    time_t utime;
    struct tm *ltime;
    uLong flag;
    int zip64;

    fill_z_func(&zff);
    zff.opaque = fout;

    out = zipOpen2_64(NULL, APPEND_STATUS_CREATE, NULL, &zff);

    if(out == NULL)
        {
        fprintf(stderr, "ERROR: Can't create output ZIP file!\n");
        perror("ERROR");
        return 0;
        }

    zfi.tmz_date.tm_sec = 0;
    zfi.tmz_date.tm_min = 0;
    zfi.tmz_date.tm_hour = 0;
    zfi.tmz_date.tm_mday = 0;
    zfi.tmz_date.tm_mon = 0;
    zfi.tmz_date.tm_year = 0;
    zfi.dosDate = 0;
    zfi.internal_fa = 0;
    zfi.external_fa = 0;

    utime = time(NULL);
    ltime = localtime(&utime);

    if(ltime == NULL)
        {
        fprintf(stderr, "WARNING: Can't get current time.\n");
        perror("WARNING");
        }
    else
        {
        zfi.tmz_date.tm_sec = ltime->tm_sec;
        zfi.tmz_date.tm_min = ltime->tm_min;
        zfi.tmz_date.tm_hour = ltime->tm_hour;
        zfi.tmz_date.tm_mday = ltime->tm_mday;
        zfi.tmz_date.tm_mon = ltime->tm_mon;
        zfi.tmz_date.tm_year = ltime->tm_year + 1900;
        }

    if(str_is_ascii(data_name))
        flag = 0;
    else
        flag = 1 << 11; /* utf-8 */

    if(data_size >= 0xffffffff)
        zip64 = 1;
    else
        zip64 = 0;

    if(zipOpenNewFileInZip4_64(
        out,                /*zipFile file,*/
        data_name,          /*const char* filename,*/
        &zfi,               /*const zip_fileinfo* zipfi,*/
        NULL,               /*const void* extrafield_local,*/
        0,                  /*uInt size_extrafield_local,*/
        NULL,               /*const void* extrafield_global,*/
        0,                  /*uInt size_extrafield_global,*/
        NULL,               /*const char* comment,*/
        Z_DEFLATED,         /*int method,*/
        9,                  /*int level,*/
        0,                  /*int raw,*/
        15,                 /*int windowBits,*/
        9,                  /*int memLevel,*/
        Z_DEFAULT_STRATEGY, /*int strategy,*/
        NULL,               /*const char* password,*/
        0,                  /*uLong crcForCrypting,*/
        0,                  /*uLong versionMadeBy,*/
        flag,               /*uLong flagBase,*/
        zip64               /*int zip64*/
        ) != Z_OK)
        {
        fprintf(stderr, "ERROR: Can't create element in ZIP file: %s\n", data_name);
        return 0;
        }

    while(data_size > 0)
        {
        unsigned write_size = INT_MAX;

        if(write_size > data_size)
            write_size = data_size;

        if(zipWriteInFileInZip(out, data, write_size) != Z_OK)
            {
            fprintf(stderr, "ERROR: Write failed!\n");
            perror("ERROR");
            return 0;
            }

        data += write_size;
        data_size -= write_size;
        }

    if(zipCloseFileInZip(out) != Z_OK || zipClose(out, NULL) != Z_OK)
            {
            fprintf(stderr, "ERROR: Write failed!\n");
            perror("ERROR");
            return 0;
            }

    return 1;
    }
