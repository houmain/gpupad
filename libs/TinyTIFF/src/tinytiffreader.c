/*
    Copyright (c) 2008-2020 Jan W. Krieger (<jan@jkrieger.de>), German Cancer Research Center (DKFZ) & IWR, University of Heidelberg

    This software is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License (LGPL) as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


*/
#include "tinytiffreader.h"
#include "tiff_definitions_internal.h"
#include "tinytiff_version.h"
//#define DEBUG_IFDTIMING
#ifdef DEBUG_IFDTIMING
#include "highrestimer.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

/** \defgroup tinytiffreader_internal TinyTIFFReader: Internal functions
 *  \ingroup tinytiffreader */

#ifndef __WINDOWS__
# if defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32)
#  define __WINDOWS__
# endif
#endif

#ifndef __LINUX__
# if defined(linux)
#  define __LINUX__
# endif
#endif

#define __USE_LIBC_FOR_TIFFReader__

#ifdef __WINDOWS__
#  ifndef __USE_LIBC_FOR_TIFFReader__
#    define TINYTIFF_USE_WINAPI_FOR_FILEIO
#  endif
#endif // __WINDOWS__

#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
#  include <windows.h>
#  warning COMPILING TinyTIFFReader with WinAPI
#  define TinyTIFFReader_POSTYPE DWORD
#else
#  define TinyTIFFReader_POSTYPE fpos_t
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO



/** \brief maximum length of error messages in bytes \internal */
#define TIFF_LAST_ERROR_SIZE 1024





int TIFFReader_get_byteorder() {
    union {
        long l;
        char c[4];
    } test;
    test.l = 1;
    if( test.c[3] && !test.c[2] && !test.c[1] && !test.c[0] )
        return TIFF_ORDER_BIGENDIAN;

    if( !test.c[3] && !test.c[2] && !test.c[1] && test.c[0] )
        return TIFF_ORDER_LITTLEENDIAN;

    return TIFF_ORDER_UNKNOWN;
}

typedef struct TinyTIFFReaderFrame {
    uint32_t width;
    uint32_t height;
    uint16_t compression;

    uint32_t rowsperstrip;
    uint32_t* stripoffsets;
    uint32_t* stripbytecounts;
    uint32_t stripcount;
    uint16_t samplesperpixel;
    uint32_t bitspersample;
    uint16_t planarconfiguration;
    uint16_t sampleformat;
    uint32_t imagelength;
    uint8_t orientation;
    uint8_t fillorder;
    uint32_t photometric_interpretation;
    uint8_t isTiled;

    float xresolution;
    float yresolution;
    uint16_t resolutionunit;
	
	char* description;
} TinyTIFFReaderFrame;

static TinyTIFFReaderFrame TinyTIFFReader_getEmptyFrame() {
    TinyTIFFReaderFrame d;
    d.width=0;
    d.height=0;
    d.stripcount=0;
    d.compression=TIFF_COMPRESSION_NONE;
    d.rowsperstrip=0;
    d.stripoffsets=0;
    d.stripbytecounts=0;
    d.samplesperpixel=1;
    d.bitspersample=0;
    d.planarconfiguration=TIFF_PLANARCONFIG_CHUNKY;
    d.sampleformat=TINYTIFF_SAMPLEFORMAT_UINT;
    d.imagelength=0;
	d.description=0;
    d.orientation=TIFF_ORIENTATION_STANDARD;
    d.fillorder=TIFF_FILLORDER_DEFAULT;
    d.photometric_interpretation=TIFF_PHOTOMETRICINTERPRETATION_BLACKISZERO;
    d.isTiled=TINYTIFF_FALSE;
    d.xresolution=1.0;
    d.yresolution=1.0;
    d.resolutionunit=1;
    return d;
}

static void TinyTIFFReader_freeEmptyFrame(TinyTIFFReaderFrame f) {
    if (f.stripoffsets) free(f.stripoffsets);
    f.stripoffsets=NULL;
    if (f.stripbytecounts) free(f.stripbytecounts);
    f.stripbytecounts=NULL;
	if (f.description) free(f.description);
	f.description=NULL;
}


struct TinyTIFFReaderFile {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
    HANDLE hFile;
#else
    FILE* file;
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO

    char lastError[TIFF_LAST_ERROR_SIZE];
    int wasError;

    uint8_t systembyteorder;
    uint8_t filebyteorder;

    uint32_t firstrecord_offset;
    uint32_t nextifd_offset;

    uint64_t filesize;
	
    TinyTIFFReaderFrame currentFrame;
};

unsigned long TinyTIFFReader_min(unsigned long a, unsigned long b) {
    if (a<b) return a;
    else return b;
}

unsigned long TinyTIFFReader_max(unsigned long a, unsigned long b) {
    if (a>b) return a;
    else return b;
}

unsigned long TinyTIFFReader_doRangesOverlap(unsigned long xstart, unsigned long xend, unsigned long ystart, unsigned long yend, unsigned long* overlap_start, unsigned long* overlap_end) {
    // see: https://stackoverflow.com/questions/36035074/how-can-i-find-an-overlap-between-two-given-ranges

    unsigned long totalRange = TinyTIFFReader_max(xend, yend) - TinyTIFFReader_min(xstart, ystart);
    unsigned long sumOfRanges = (xend - xstart) + (yend - ystart);

    if (sumOfRanges > totalRange) { // means they overlap
        if (overlap_end!=NULL) *overlap_end = TinyTIFFReader_min(xend, yend);
        if (overlap_start!=NULL) *overlap_start = TinyTIFFReader_max(xstart, ystart);
        return TINYTIFF_TRUE;
    }

    return TINYTIFF_FALSE;
}

int TinyTIFFReader_memcpy_s( void * dest, unsigned long destsz, const void * src, unsigned long count ) {
#ifdef HAVE_MEMCPY_S
    return memcpy_s(dest, destsz, src, count);
#else
    memcpy(dest,  src, count);
    return 0;
#endif
}

void TinyTIFFReader_fopen(TinyTIFFReaderFile* tiff, const char* filename) {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
    tiff->hFile = CreateFile(filename,               // name of the write
                       GENERIC_READ,          // open for writing
                       FILE_SHARE_READ,
                       NULL,                   // default security
                       OPEN_EXISTING,             // create new file only
                       FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,  // normal file
                       NULL);                  // no attr. template
#else
#  ifdef HAVE_FOPEN_S
    fopen_s(&(tiff->file), filename, "rb");
#  else
    tiff->file=fopen(filename, "rb");
#  endif
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO
}

int TinyTIFFReader_fclose(TinyTIFFReaderFile* tiff) {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
    CloseHandle(tiff->hFile);
    return 0;
#else
    int r=fclose(tiff->file);
    tiff->file=NULL;
    return r;
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO
}

int TinyTIFFReader_fOK(const TinyTIFFReaderFile* tiff)  {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
   if (tiff->hFile == INVALID_HANDLE_VALUE) return TINYTIFF_FALSE;
   else return TINYTIFF_TRUE;
#else
    if (tiff->file) {
        return TINYTIFF_TRUE;
    }
    return TINYTIFF_FALSE;
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO
}

int TinyTIFFReader_fseek_set(TinyTIFFReaderFile* tiff, unsigned long offset) {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
   DWORD res = SetFilePointer (tiff->hFile,
                                offset,
                                NULL,
                                FILE_BEGIN);


   return res;
#else
    return fseek(tiff->file, (long)offset, SEEK_SET);
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO
}

int TinyTIFFReader_fseek_cur(TinyTIFFReaderFile* tiff, unsigned long offset) {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
   DWORD res = SetFilePointer (tiff->hFile,
                                offset,
                                NULL,
                                FILE_CURRENT);


   return res;
#else
    return fseek(tiff->file, (long)offset, SEEK_CUR);
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO
}

unsigned long TinyTIFFReader_fread(void * ptr, unsigned long ptrsize, unsigned long size, unsigned long count, TinyTIFFReaderFile* tiff) {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
    DWORD  dwBytesRead = 0;
    if(!ReadFile(tiff->hFile, ptr, size*count, &dwBytesRead, NULL)) {
        return 0;
    }
    return dwBytesRead;
#else
#ifdef  HAVE_FREAD_S
    return fread_s(ptr, ptrsize, size, count, tiff->file);
#  else
    return fread(ptr, size, count, tiff->file);
#  endif
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO
}


static long int TinyTIFFReader_ftell ( TinyTIFFReaderFile * tiff ) {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
DWORD dwPtr = SetFilePointer( tiff->hFile,
                                0,
                                NULL,
                                FILE_CURRENT );
    return dwPtr;
#else
    return ftell(tiff->file);
#endif
}

int TinyTIFFReader_fgetpos(TinyTIFFReaderFile* tiff, TinyTIFFReader_POSTYPE* pos) {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
    *pos= SetFilePointer( tiff->hFile,
                                0,
                                NULL,
                                FILE_CURRENT );
    return 0;
#else
    return fgetpos(tiff->file, pos);
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO
}

int TinyTIFFReader_fsetpos(TinyTIFFReaderFile* tiff, const TinyTIFFReader_POSTYPE* pos) {
#ifdef TINYTIFF_USE_WINAPI_FOR_FILEIO
    SetFilePointer( tiff->hFile,
                                *pos,
                                NULL,
                                FILE_BEGIN );
    return 0;
#else
    return fsetpos(tiff->file, pos);
#endif // TINYTIFF_USE_WINAPI_FOR_FILEIO
}




const char* TinyTIFFReader_getLastError(TinyTIFFReaderFile* tiff) {
    if (tiff) return tiff->lastError;
    return NULL;
}

int TinyTIFFReader_wasError(TinyTIFFReaderFile* tiff) {
    if (tiff) return tiff->wasError;
    return TINYTIFF_TRUE;
}

int TinyTIFFReader_success(TinyTIFFReaderFile* tiff) {
    if (tiff) return !tiff->wasError;
    return TINYTIFF_FALSE;
}



static uint64_t TinyTIFFReader_Byteswap64(uint64_t nLongNumber)
{
    return ( ((nLongNumber&0x00000000000000FFULL)<<56)
            +((nLongNumber&0x000000000000FF00ULL)<<40)
            +((nLongNumber&0x0000000000FF0000ULL)<<24)
            +((nLongNumber&0x00000000FF000000ULL)<<8)
            +((nLongNumber&0x000000FF00000000ULL)>>8)
            +((nLongNumber&0x0000FF0000000000ULL)>>24)
            +((nLongNumber&0x00FF000000000000ULL)>>40)
            +((nLongNumber&0xFF00000000000000ULL)>>56));
}


static uint32_t TinyTIFFReader_Byteswap32(uint32_t nLongNumber)
{
    return (((nLongNumber&0x000000FF)<<24)+((nLongNumber&0x0000FF00)<<8)+
            ((nLongNumber&0x00FF0000)>>8)+((nLongNumber&0xFF000000)>>24));
}

static uint16_t TinyTIFFReader_Byteswap16(uint16_t nValue)
{
   return (((nValue>> 8)) | (nValue << 8));
}

static uint32_t TinyTIFFReader_readuint32(TinyTIFFReaderFile* tiff) {
    uint32_t res=0;
    //fread(&res, 4,1,tiff->file);
    TinyTIFFReader_fread(&res, sizeof(res), 4,1,tiff);
    if (tiff->systembyteorder!=tiff->filebyteorder) {
        res=TinyTIFFReader_Byteswap32(res);
    }
    return res;
}


static uint16_t TinyTIFFReader_readuint16(TinyTIFFReaderFile* tiff) {
    uint16_t res=0;
    //fread(&res, 2,1,tiff->file);
    TinyTIFFReader_fread(&res, sizeof(res), 2,1,tiff);
    if (tiff->systembyteorder!=tiff->filebyteorder) {
        res=TinyTIFFReader_Byteswap16(res);
    }
    return res;
}

static uint8_t TinyTIFFReader_readuint8(TinyTIFFReaderFile* tiff) {
    uint8_t res=0;
    //fread(&res, 1,1,tiff->file);
    TinyTIFFReader_fread(&res, sizeof(res), 1,1,tiff);
    return res;
}


typedef struct TinyTIFFReader_IFD {
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value;
    uint32_t value2;

    uint32_t* pvalue;
    uint32_t* pvalue2;
} TinyTIFFReader_IFD;

static void TinyTIFFReader_freeIFD(TinyTIFFReader_IFD d) {
    if (d.pvalue /*&& d.count>1*/) { free(d.pvalue);  d.pvalue=NULL; }
    if (d.pvalue2 /*&& d.count>1*/) { free(d.pvalue2);  d.pvalue2=NULL; }
}

static TinyTIFFReader_IFD TinyTIFFReader_readIFD(TinyTIFFReaderFile* tiff) {
    TinyTIFFReader_IFD d;

    d.value=0;
    d.value2=0;

    d.pvalue=0;
    d.pvalue2=0;

    d.tag=TinyTIFFReader_readuint16(tiff);
    d.type=TinyTIFFReader_readuint16(tiff);
    d.count=TinyTIFFReader_readuint32(tiff);
    //uint32_t val=TinyTIFFReader_readuint32(tiff);
    TinyTIFFReader_POSTYPE pos;
    //fgetpos(tiff->file, &pos);
    TinyTIFFReader_fgetpos(tiff, &pos);
    int changedpos=TINYTIFF_FALSE;
    //printf("    - pos=0x%X   tag=%d type=%d count=%u \n",pos, d.tag, d.type, d.count);
    switch(d.type) {
        case TIFF_TYPE_BYTE:
        case TIFF_TYPE_ASCII:
            if (d.count>0) {
              d.pvalue=(uint32_t*)calloc(d.count, sizeof(uint32_t));
                if (d.count<=4) {
                unsigned int i;
                for (i=0; i<4; i++) {
                  uint32_t v=TinyTIFFReader_readuint8(tiff);
                  if (i<d.count) d.pvalue[i]=v;
                }
              } else {
                changedpos=TINYTIFF_TRUE;
                uint32_t offset=TinyTIFFReader_readuint32(tiff);

                if (offset+d.count*1<=tiff->filesize) {
                  //fseek(tiff->file, offset, SEEK_SET);
                  TinyTIFFReader_fseek_set(tiff, offset);
                  unsigned int i;
                  for (i=0; i<d.count; i++) {
                    d.pvalue[i]=TinyTIFFReader_readuint8(tiff);
                  }
                }
              }
            }
            d.pvalue2=NULL;
            //printf("    - BYTE/CHAR: tag=%d count=%u   val[0]=%u\n",d.tag,d.count, d.pvalue[0]);
            break;
        case TIFF_TYPE_SHORT:
            d.pvalue=(uint32_t*)calloc(d.count, sizeof(uint32_t));
            if (d.count<=2) {
                unsigned int i;
                for (i=0; i<2; i++) {
                    uint32_t v=TinyTIFFReader_readuint16(tiff);
                    if (i<d.count) d.pvalue[i]=v;
                }
            } else {
                changedpos=TINYTIFF_TRUE;
                uint32_t offset=TinyTIFFReader_readuint32(tiff);
                if (offset+d.count*2<=tiff->filesize) {
                    //fseek(tiff->file, offset, SEEK_SET);
                    TinyTIFFReader_fseek_set(tiff, offset);
                    unsigned int i;
                    for (i=0; i<d.count; i++) {
                        d.pvalue[i]=TinyTIFFReader_readuint16(tiff);
                    }
                }
            }
            d.pvalue2=NULL;
            //printf("    - SHORT: tag=%d count=%u   val[0]=%u\n",d.tag,d.count, d.pvalue[0]);
            break;

        case TIFF_TYPE_LONG:
            d.pvalue=(uint32_t*)calloc(d.count, sizeof(uint32_t));
            if (d.count<=1) {
                d.pvalue[0]=TinyTIFFReader_readuint32(tiff);
            } else {
                changedpos=TINYTIFF_TRUE;
                uint32_t offset=TinyTIFFReader_readuint32(tiff);
                if (offset+d.count*4<=tiff->filesize) {
                    //fseek(tiff->file, offset, SEEK_SET);
                    TinyTIFFReader_fseek_set(tiff, offset);
                    uint32_t i;
                    for (i=0; i<d.count; i++) {
                        d.pvalue[i]=TinyTIFFReader_readuint32(tiff);
                    }
                }
                //printf("    - LONG: pos=0x%X   offset=0x%X   tag=%d count=%u   val[0]=%u\n",pos, offset,d.tag,d.count, d.pvalue[0]);
            }
            d.pvalue2=NULL;
            //printf("    - LONG: tag=%d count=%u   val[0]=%u\n",d.tag,d.count, d.pvalue[0]);
            break;
        case TIFF_TYPE_RATIONAL: {
            d.pvalue=(uint32_t*)calloc(d.count, sizeof(uint32_t));
            d.pvalue2=(uint32_t*)calloc(d.count, sizeof(uint32_t));

            changedpos=TINYTIFF_TRUE;
            uint32_t offset=TinyTIFFReader_readuint32(tiff);
            if (offset+d.count*4<=tiff->filesize) {
                //fseek(tiff->file, offset, SEEK_SET);
                TinyTIFFReader_fseek_set(tiff, offset);
                uint32_t i;
                for (i=0; i<d.count; i++) {
                    d.pvalue[i]=TinyTIFFReader_readuint32(tiff);
                    d.pvalue2[i]=TinyTIFFReader_readuint32(tiff);
                }
            }
            //printf("    - RATIONAL: pos=0x%X   offset=0x%X   tag=%d count=%u   val[0]=%u/%u\n",pos, offset,d.tag,d.count, d.pvalue[0], d.pvalue[1]);
            } break;

       default: d.value=TinyTIFFReader_readuint32(tiff); break;
    }
    if (d.pvalue) d.value=d.pvalue[0];
    if (d.pvalue2) d.value2=d.pvalue2[0];

    if (changedpos) {
        //fsetpos(tiff->file, &pos);
        TinyTIFFReader_fsetpos(tiff, &pos);
        //fseek(tiff->file, 4, SEEK_CUR);
        TinyTIFFReader_fseek_cur(tiff, 4);
    }
    return d;
}


static void TinyTIFFReader_readNextFrame(TinyTIFFReaderFile* tiff) {

    TinyTIFFReader_freeEmptyFrame(tiff->currentFrame);
    tiff->currentFrame=TinyTIFFReader_getEmptyFrame();
    #ifdef DEBUG_IFDTIMING
    HighResTimer timer;
    timer.start();
    #endif
    if (tiff->nextifd_offset!=0 && tiff->nextifd_offset+2<tiff->filesize) {
        //printf("    - seeking=0x%X\n", tiff->nextifd_offset);
        //fseek(tiff->file, tiff->nextifd_offset, SEEK_SET);
        TinyTIFFReader_fseek_set(tiff, tiff->nextifd_offset);
        uint16_t ifd_count=TinyTIFFReader_readuint16(tiff);
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
        printf("    - tag_count=%u\n", ifd_count);
#endif
        uint16_t i;
        for ( i=0; i<ifd_count; i++) {
    #ifdef DEBUG_IFDTIMING
            timer.start();
    #endif
            TinyTIFFReader_IFD ifd=TinyTIFFReader_readIFD(tiff);
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
    #ifdef DEBUG_IFDTIMING
            printf("    - readIFD %d (tag: %u, type: %u, count: %u): %lf us\n", i, ifd.tag, ifd.type, ifd.count, timer.get_time());
    #else
            printf("    - readIFD %d (tag: %u, type: %u, count: %u)\n", i, ifd.tag, ifd.type, ifd.count);
    #endif
#endif
            switch(ifd.tag) {
                case TIFF_FIELD_IMAGEWIDTH: tiff->currentFrame.width=ifd.value;  break;
                case TIFF_FIELD_IMAGELENGTH: tiff->currentFrame.imagelength=ifd.value;  break;
                case TIFF_FIELD_BITSPERSAMPLE: {
                        tiff->currentFrame.bitspersample=ifd.value;
                        if (ifd.count>0) {
                            tiff->currentFrame.bitspersample=ifd.pvalue[0];
                            int ok=TINYTIFF_TRUE;
                            uint32_t ii;
                            for (ii=1; ii<ifd.count; ii++) {
                                if (ifd.pvalue[ii]!=ifd.pvalue[0]) {
                                    ok=TINYTIFF_FALSE;
                                }
                            }
                            if (ok==TINYTIFF_FALSE) {
                                tiff->wasError=TINYTIFF_TRUE;
                                TINYTIFF_SET_LAST_ERROR(tiff, "this library does not support different sample sizes in a single frame\0");
                            }
                        }

                     } break;
                case TIFF_FIELD_COMPRESSION: tiff->currentFrame.compression=ifd.value; break;
                case TIFF_FIELD_STRIPOFFSETS: {
                        tiff->currentFrame.stripcount=ifd.count;
                        tiff->currentFrame.stripoffsets=(uint32_t*)calloc(ifd.count, sizeof(uint32_t));
                        TinyTIFFReader_memcpy_s(tiff->currentFrame.stripoffsets, ifd.count*sizeof(uint32_t), ifd.pvalue, ifd.count*sizeof(uint32_t));
                    } break;
                case TIFF_FIELD_SAMPLESPERPIXEL: tiff->currentFrame.samplesperpixel=ifd.value; break;
                case TIFF_FIELD_ROWSPERSTRIP: tiff->currentFrame.rowsperstrip=ifd.value; break;
                case TIFF_FIELD_SAMPLEFORMAT: tiff->currentFrame.sampleformat=ifd.value; break;
                case TIFF_FIELD_IMAGEDESCRIPTION: {
                        //printf("TIFF_FIELD_IMAGEDESCRIPTION: (tag: %u, type: %u, count: %u)\n", ifd.tag, ifd.type, ifd.count);
				        if (ifd.count>0) {
				            if (tiff->currentFrame.description) free(tiff->currentFrame.description);
							tiff->currentFrame.description=(char*)calloc(ifd.count+1, sizeof(char));
                            memset(tiff->currentFrame.description, 0, ifd.count+1);
							for (uint32_t ji=0; ji<ifd.count; ji++) {
							    tiff->currentFrame.description[ji]=(char)ifd.pvalue[ji];
								//printf(" %d[%d]", int(tiff->currentFrame.description[ji]), int(ifd.pvalue[ji]));
							}
							//printf("\n  %s\n", tiff->currentFrame.description);
					    }
                    } break;
                case TIFF_FIELD_STRIPBYTECOUNTS: {
                        tiff->currentFrame.stripcount=ifd.count;
                        tiff->currentFrame.stripbytecounts=(uint32_t*)calloc(ifd.count, sizeof(uint32_t));
                        TinyTIFFReader_memcpy_s(tiff->currentFrame.stripbytecounts, ifd.count*sizeof(uint32_t), ifd.pvalue, ifd.count*sizeof(uint32_t));
                    } break;
                case TIFF_FIELD_PLANARCONFIG: tiff->currentFrame.planarconfiguration=ifd.value; break;
                case TIFF_FIELD_ORIENTATION: tiff->currentFrame.orientation=ifd.value; break;
                case TIFF_FIELD_PHOTOMETRICINTERPRETATION: tiff->currentFrame.photometric_interpretation=ifd.value; break;
                case TIFF_FIELD_FILLORDER: tiff->currentFrame.fillorder=ifd.value; break;
                case TIFF_FIELD_TILE_BYTECOUNTS:
                case TIFF_FIELD_TILE_LENGTH:
                case TIFF_FIELD_TILE_OFFSETS:
                case TIFF_FIELD_TILE_WIDTH :
                    tiff->currentFrame.isTiled=TINYTIFF_TRUE;
                    break;
                case TIFF_FIELD_XRESOLUTION:{
                    tiff->currentFrame.xresolution= ((float)ifd.value)/((float) ifd.value2);
                } break;
                case TIFF_FIELD_YRESOLUTION: tiff->currentFrame.yresolution = ((float)ifd.value)/((float) ifd.value2);break;
                case TIFF_FIELD_RESOLUTIONUNIT: tiff->currentFrame.resolutionunit = ifd.value;break;
                default:
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                    printf("      --> unhandled FIELD %d\n", (int)ifd.tag);
#endif
                    break;
            }
            TinyTIFFReader_freeIFD(ifd);
            //printf("    - tag=%u\n", ifd.tag);
        }
        tiff->currentFrame.height=tiff->currentFrame.imagelength;
        //printf("      - width=%u\n", tiff->currentFrame.width);
        //printf("      - height=%u\n", tiff->currentFrame.height);
        //fseek(tiff->file, tiff->nextifd_offset+2+12*ifd_count, SEEK_SET);
        TinyTIFFReader_fseek_set(tiff, tiff->nextifd_offset+2+12*ifd_count);
        tiff->nextifd_offset=TinyTIFFReader_readuint32(tiff);
        //printf("      - nextifd_offset=%lu\n", tiff->nextifd_offset);
    } else {
        tiff->wasError=TINYTIFF_TRUE;
        TINYTIFF_SET_LAST_ERROR(tiff, "no more images in TIF file\0");
    }
}

int TinyTIFFReader_getSampleData(TinyTIFFReaderFile* tiff, void* buffer, uint16_t sample) {
    if (tiff) {
        if (tiff->currentFrame.compression!=TIFF_COMPRESSION_NONE) {
            tiff->wasError=TINYTIFF_TRUE;
            TINYTIFF_SET_LAST_ERROR(tiff, "the compression of the file is not supported by this library\0");
            return TINYTIFF_FALSE;
        }
        if (tiff->currentFrame.isTiled!=TINYTIFF_FALSE) {
            tiff->wasError=TINYTIFF_TRUE;
            TINYTIFF_SET_LAST_ERROR(tiff, "tiled images are not supported by this library\0");
            return TINYTIFF_FALSE;
        }
        if (tiff->currentFrame.orientation!=TIFF_ORIENTATION_STANDARD) {
            tiff->wasError=TINYTIFF_TRUE;
            TINYTIFF_SET_LAST_ERROR(tiff, "only standard TIFF orientations are supported by this library\0");
            return TINYTIFF_FALSE;
        }
        if (tiff->currentFrame.photometric_interpretation==TIFF_PHOTOMETRICINTERPRETATION_PALETTE) {
            tiff->wasError=TINYTIFF_TRUE;
            TINYTIFF_SET_LAST_ERROR(tiff, "palette-colored TIFF images are supported by this library\0");
            return TINYTIFF_FALSE;
        }
        if (tiff->currentFrame.width==0 || tiff->currentFrame.height==0 ) {
            tiff->wasError=TINYTIFF_TRUE;
            TINYTIFF_SET_LAST_ERROR(tiff, "the current frame does not contain images\0");
            return TINYTIFF_FALSE;
        }
        if (tiff->currentFrame.bitspersample!=8 && tiff->currentFrame.bitspersample!=16 && tiff->currentFrame.bitspersample!=32 && tiff->currentFrame.bitspersample!=64) {
            tiff->wasError=TINYTIFF_TRUE;
            TINYTIFF_SET_LAST_ERROR(tiff, "this library only support 8,16,32 and 64 bits per sample\0");
            return TINYTIFF_FALSE;
        }
        TinyTIFFReader_POSTYPE pos;
        //fgetpos(tiff->file, &pos);
        TinyTIFFReader_fgetpos(tiff, &pos);
        tiff->wasError=TINYTIFF_FALSE;

#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
        printf("    - stripcount=%lu\n", (unsigned long)tiff->currentFrame.stripcount);
#endif
        if (tiff->currentFrame.stripcount>0 && tiff->currentFrame.stripbytecounts && tiff->currentFrame.stripoffsets) {
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
            printf("    - bitspersample=%lu\n", (unsigned long)tiff->currentFrame.bitspersample);
#endif

            if (tiff->currentFrame.samplesperpixel==1 || tiff->currentFrame.planarconfiguration==TIFF_PLANARCONFIG_PLANAR) {
                // we assume the set of strips form a continuous memory-range. The actual strip offsets are only taken into account, when actually seeking in the file
                //
                //  strip 0                          strip 1                          strip 2                          strip 3                          strip 4
                //  +--------------------------------+--------------------------------+--------------------------------+--------------------------------+--------------------------------+
                //  |                                |                                |                                |                                |                                |
                //  +--------------------------------+--------------------------------+--------------------------------+--------------------------------+--------------------------------+
                //  0                                stripsize0                       stripsize0+1                     stripsize0+1+2                   stripsize0+1+2+3                 stripsize0+1+2+3+4
                //
                //                                   ^
                //                                   |
                //                                   fileimageidx_bytes: points to start of strip in continuous strip space
                //                                   |
                //                                   v
                //
                //                          image to read (length=sample_image_size_bytes)
                //                       +--------------------------------------------------------------------------------------------------+
                //                       |                                                                                                  |
                //                       +--------------------------------------------------------------------------------------------------+
                //                       sample_start_bytes=sample*sample_image_size_bytes                                                  sample_end_bytes=sample_start_bytes+sample_image_size_bytes
                //
                //              READ:    |===========|================================|================================|====================|
                //                                   |                                |
                //                                   +--------------------------------+
                //                                        read segment in strip 1
                //                                   bytes_to_read_start              bytes_to_read_end (0=start of strip, stripsize_bytes-1=endofstrip)
                //
                //                                   ^
                //                                   |
                //                                   outputimageidx_bytes: points to where to start writing in output array

                const unsigned long sample_image_size_bytes=tiff->currentFrame.width*tiff->currentFrame.height*tiff->currentFrame.bitspersample/8;
                const unsigned long sample_start_bytes=sample*sample_image_size_bytes;
                const unsigned long sample_end_bytes=sample_start_bytes+sample_image_size_bytes;
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                printf("    - sample_image_size_bytes=%lu\n", (unsigned long)sample_image_size_bytes);
                printf("    - sample_start_bytes=%lu\n", (unsigned long)sample_start_bytes);
                printf("    - sample_end_bytes=%lu\n", (unsigned long)sample_end_bytes);
#endif
                uint32_t strip;
                unsigned long fileimageidx_bytes=0;
                unsigned long outputimageidx_bytes=0;
                for (strip=0; strip<tiff->currentFrame.stripcount; strip++) {
                    const unsigned long stripsize_bytes=tiff->currentFrame.stripbytecounts[strip];
                    const unsigned long strip_offset_bytes=tiff->currentFrame.stripoffsets[strip];
                    unsigned long bytes_to_read_start=0, bytes_to_read_end=0;
                    const int hasToReadFromStrip=TinyTIFFReader_doRangesOverlap(sample_start_bytes, sample_end_bytes, fileimageidx_bytes, fileimageidx_bytes+stripsize_bytes, &bytes_to_read_start, &bytes_to_read_end);
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                    printf("    - strip %2lu, stripoffset=%8lubytes, stripsize=%8lubytes, fileimageidx=%8lubytes, outputimageidx_bytes=%8lubytes\n", (unsigned long)strip, (unsigned long)strip_offset_bytes,(unsigned long)stripsize_bytes,(unsigned long)fileimageidx_bytes, (unsigned long)outputimageidx_bytes);
#endif

                    if (hasToReadFromStrip!=TINYTIFF_FALSE) {
                        const unsigned long count_bytes_to_read=bytes_to_read_end-bytes_to_read_start;
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                        printf("      - bytes_to_read_start=%8lu, bytes_to_read_end=%8lu, count_bytes_to_read=%8lu\n", (unsigned long)bytes_to_read_start-fileimageidx_bytes, (unsigned long)bytes_to_read_end-fileimageidx_bytes, (unsigned long)count_bytes_to_read);
                        printf("      - READ -> Writing to %8lu...%8lu / %8lu\n", (unsigned long)outputimageidx_bytes, (unsigned long)outputimageidx_bytes+count_bytes_to_read, (unsigned long)sample_image_size_bytes);
#endif
                        TinyTIFFReader_fseek_set(tiff, strip_offset_bytes+(bytes_to_read_start-fileimageidx_bytes));
                        unsigned long readbytes=TinyTIFFReader_fread(&(((uint8_t*)buffer)[outputimageidx_bytes]), sample_image_size_bytes, 1, count_bytes_to_read, tiff);
                        if(readbytes!=count_bytes_to_read) {
                            tiff->wasError=TINYTIFF_TRUE;
                            TINYTIFF_SET_LAST_ERROR(tiff, "TINYTIFFReader was unable to read all necessary data from the strip!\0");
                        }
                        outputimageidx_bytes+=count_bytes_to_read;
                    } else if (fileimageidx_bytes>sample_end_bytes) {
                        // we have read all data successfully
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                        printf("      - FINISHED READING, fileimageidx_bytes=%lu > %lu=sample_end_bytes\n", (unsigned long)fileimageidx_bytes,(unsigned long)sample_end_bytes);
#endif
                        break;
                    }

                    fileimageidx_bytes+=stripsize_bytes;
                }
            } else if (tiff->currentFrame.samplesperpixel>1 && tiff->currentFrame.planarconfiguration==TIFF_PLANARCONFIG_CHUNKY) {
                uint32_t strip;
                unsigned long fileimageidx_bytes=0;
                unsigned long outputimageidx_bytes=0;
                uint8_t* stripdata=NULL;
                unsigned long last_stripsize_bytes=0;
                for (strip=0; strip<tiff->currentFrame.stripcount; strip++) {
                    const unsigned long stripsize_bytes=tiff->currentFrame.stripbytecounts[strip];
                    if (stripsize_bytes>last_stripsize_bytes) {
                        if (stripdata) free(stripdata);
                        stripdata=(uint8_t*)malloc(stripsize_bytes);
                        last_stripsize_bytes=stripsize_bytes;
                    }

                    const unsigned long strip_offset_bytes=tiff->currentFrame.stripoffsets[strip];
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                    printf("    - strip %2lu, stripoffset=%8lubytes, stripsize=%8lubytes, fileimageidx=%8lubytes, outputimageidx_bytes=%8lubytes\n", (unsigned long)strip, (unsigned long)strip_offset_bytes,(unsigned long)stripsize_bytes,(unsigned long)fileimageidx_bytes, (unsigned long)outputimageidx_bytes);
#endif
                    TinyTIFFReader_fseek_set(tiff, strip_offset_bytes);
                    unsigned long readbytes=TinyTIFFReader_fread(stripdata, last_stripsize_bytes, 1, stripsize_bytes, tiff);
                    if(readbytes!=stripsize_bytes) {
                        tiff->wasError=TINYTIFF_TRUE;
                        TINYTIFF_SET_LAST_ERROR(tiff, "TINYTIFFReader was unable to read all necessary data from the strip!\0");
                    }
                    unsigned long stripi=0;
                    for (stripi=sample*tiff->currentFrame.bitspersample/8; stripi<stripsize_bytes; stripi+=tiff->currentFrame.bitspersample/8*tiff->currentFrame.samplesperpixel) {
                        memcpy(&(((uint8_t*)buffer)[outputimageidx_bytes]), &(stripdata[stripi]), tiff->currentFrame.bitspersample/8);
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                        if (stripi<sample*tiff->currentFrame.bitspersample/8+10*tiff->currentFrame.bitspersample/8*tiff->currentFrame.samplesperpixel) printf("      - memcpy(buffer[%8lu], stripdata[%8lu], %lu)\n", (unsigned long)outputimageidx_bytes, (unsigned long)stripi, (unsigned long)(tiff->currentFrame.bitspersample/8));
                        else if (stripi==sample*tiff->currentFrame.bitspersample/8+10*tiff->currentFrame.bitspersample/8*tiff->currentFrame.samplesperpixel) printf("      - memcpy(...)\n");
#endif
                        outputimageidx_bytes+=tiff->currentFrame.bitspersample/8;
                    }

                    fileimageidx_bytes+=stripsize_bytes;

                }
                if (stripdata) free(stripdata);

            }



            if (tiff->systembyteorder!=tiff->filebyteorder) {
                if (tiff->currentFrame.bitspersample==8) {
                    // we're done no little/big-endian correction necessary for 1-byte data
                } else if (tiff->currentFrame.bitspersample==16) {
                    // little/big-endian correction necessary for 2-byte data
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                    printf("        - correcting 16-bit little-big-endian\n");
#endif
                    unsigned long x=0;
                    for (x=0; x<tiff->currentFrame.width*tiff->currentFrame.height; x++) {
                        ((uint16_t*)buffer)[x]=TinyTIFFReader_Byteswap16(((uint16_t*)buffer)[x]);
                    }
                } else if (tiff->currentFrame.bitspersample==32) {
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                    printf("        - correcting 32-bit little-big-endian\n");
#endif

                    // little/big-endian correction necessary for 4-byte data
                    unsigned long x=0;
                    for (x=0; x<tiff->currentFrame.width*tiff->currentFrame.height; x++) {
                        ((uint32_t*)buffer)[x]=TinyTIFFReader_Byteswap32(((uint32_t*)buffer)[x]);
                    }
                } else if (tiff->currentFrame.bitspersample==64) {
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
                    printf("        - correcting 32-bit little-big-endian\n");
#endif

                    // little/big-endian correction necessary for 8-byte data
                    unsigned long x=0;
                    for (x=0; x<tiff->currentFrame.width*tiff->currentFrame.height; x++) {
                        ((uint64_t*)buffer)[x]=TinyTIFFReader_Byteswap64(((uint64_t*)buffer)[x]);
                    }
                } else {
                    tiff->wasError=TINYTIFF_TRUE;
                    TINYTIFF_SET_LAST_ERROR(tiff, "TINYTIFFReader does not support the bitsPerSample, given in teh file (only 8,16,32bit are supported)\0");
                }
            }


        } else {
            tiff->wasError=TINYTIFF_TRUE;
            TINYTIFF_SET_LAST_ERROR(tiff, "TIFF format not recognized\0");
        }

        //fsetpos(tiff->file, &pos);
        TinyTIFFReader_fsetpos(tiff, &pos);
        return !tiff->wasError;
    }
    return TINYTIFF_FALSE;
}
















TinyTIFFReaderFile* TinyTIFFReader_open(const char* filename) {
    TinyTIFFReaderFile* tiff=(TinyTIFFReaderFile*)malloc(sizeof(TinyTIFFReaderFile));

    tiff->filesize=0;
    struct stat file;
    if(stat(filename,&file)==0) {
         tiff->filesize=file.st_size;
    }
    tiff->currentFrame=TinyTIFFReader_getEmptyFrame();


    //tiff->file=v(filename, "rb");
    TinyTIFFReader_fopen(tiff, filename);
    tiff->systembyteorder=TIFFReader_get_byteorder();
    memset(tiff->lastError, 0, TIFF_LAST_ERROR_SIZE);
    tiff->wasError=TINYTIFF_FALSE;
    if (TinyTIFFReader_fOK(tiff) && tiff->filesize>0) {
        const unsigned long tiffidsize=3;
        uint8_t tiffid[3]={0,0,0};
        //fread(tiffid, 1,2,tiff->file);
        TinyTIFFReader_fread(tiffid, tiffidsize, 1,2,tiff);
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
        printf("      - head=%s\n", tiffid);
#endif
        if (tiffid[0]=='I' && tiffid[1]=='I') tiff->filebyteorder=TIFF_ORDER_LITTLEENDIAN;
        else if (tiffid[0]=='M' && tiffid[1]=='M') tiff->filebyteorder=TIFF_ORDER_BIGENDIAN;
        else {
            free(tiff);
            return NULL;
        }
        uint16_t magic=TinyTIFFReader_readuint16(tiff);
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
        printf("      - magic=%u\n", magic);
#endif
        if (magic!=42) {
            free(tiff);
            return NULL;
        }
        tiff->firstrecord_offset=TinyTIFFReader_readuint32(tiff);
        tiff->nextifd_offset=tiff->firstrecord_offset;
#ifdef TINYTIFF_ADDITIONAL_DEBUG_MESSAGES
        printf("      - filesize=%lu\n", tiff->filesize);
        printf("      - firstrecord_offset=%4X\n", tiff->firstrecord_offset);
#endif
        TinyTIFFReader_readNextFrame(tiff);
    } else {
        free(tiff);
        return NULL;
    }

    return tiff;
}

void TinyTIFFReader_close(TinyTIFFReaderFile* tiff) {
    if (tiff) {
        TinyTIFFReader_freeEmptyFrame(tiff->currentFrame);
        //fclose(tiff->file);
        TinyTIFFReader_fclose(tiff);
        free(tiff);
    }
}

int TinyTIFFReader_hasNext(TinyTIFFReaderFile* tiff) {
    if (tiff) {
        if (tiff->nextifd_offset>0 && tiff->nextifd_offset<tiff->filesize) return TINYTIFF_TRUE;
        else return TINYTIFF_FALSE;
    } else {
        return TINYTIFF_FALSE;
    }
}

int TinyTIFFReader_readNext(TinyTIFFReaderFile* tiff) {
    int hasNext=TinyTIFFReader_hasNext(tiff);
    if (hasNext) {
        TinyTIFFReader_readNextFrame(tiff);
    }
    return hasNext;
}

uint32_t TinyTIFFReader_getWidth(TinyTIFFReaderFile* tiff) {
    if (tiff) {
        return tiff->currentFrame.width;
    }
    return 0;
}

uint32_t TinyTIFFReader_getHeight(TinyTIFFReaderFile* tiff) {
    if (tiff) {
        return tiff->currentFrame.height;
    }
    return 0;
}

const char* TinyTIFFReader_getImageDescription(TinyTIFFReaderFile* tiff) {
    if (tiff) {
        if (tiff->currentFrame.description) return tiff->currentFrame.description;
    }
    static const char* nothing = "";
    return nothing;
}

uint16_t TinyTIFFReader_getSampleFormat(TinyTIFFReaderFile* tiff) {
    if (tiff) {
        return tiff->currentFrame.sampleformat;
    }
    return 0;
}

uint16_t TinyTIFFReader_getBitsPerSample(TinyTIFFReaderFile* tiff, int sample) {
    if (tiff) {
        return tiff->currentFrame.bitspersample;
    }
    return 0;
}

uint16_t TinyTIFFReader_getSamplesPerPixel(TinyTIFFReaderFile* tiff) {
    if (tiff) {
        return tiff->currentFrame.samplesperpixel;
    }
    return 0;
}



uint32_t TinyTIFFReader_countFrames(TinyTIFFReaderFile* tiff) {

    if (tiff) {
        uint32_t frames=0;
        TinyTIFFReader_POSTYPE pos;
        //printf("    -> countFrames: pos before %ld\n", ftell(tiff->file));
        //fgetpos(tiff->file, &pos);
        TinyTIFFReader_fgetpos(tiff, &pos);

        uint32_t nextOffset=tiff->firstrecord_offset;
        while (nextOffset>0) {
            //fseek(tiff->file, nextOffset, SEEK_SET);
            TinyTIFFReader_fseek_set(tiff, nextOffset);
            uint16_t count=TinyTIFFReader_readuint16(tiff);
            //fseek(tiff->file, count*12, SEEK_CUR);
            TinyTIFFReader_fseek_cur(tiff, count*12);
            nextOffset=TinyTIFFReader_readuint32(tiff);
            frames++;
        }


        //fsetpos(tiff->file, &pos);
        TinyTIFFReader_fsetpos(tiff, &pos);
        //printf("    -> countFrames: pos after %ld\n", ftell(tiff->file));
        return frames;
    }
    return 0;
}
float TinyTIFFReader_getXResolution(TinyTIFFReaderFile* tiff){
    if(tiff){
        return tiff->currentFrame.xresolution;
    }
    return 0.0f;
}

float TinyTIFFReader_getYResolution(TinyTIFFReaderFile* tiff){
    if(tiff){
        return tiff->currentFrame.yresolution;
    }
    return 0.0f;
}

uint16_t TinyTIFFReader_getResolutionUnit(TinyTIFFReaderFile* tiff){
    if(tiff){
        return tiff->currentFrame.resolutionunit;
    }
    return TIFF_RESOLUTION_UNIT_INCH;// default
}

const char *TinyTIFFReader_getVersion()
{
    static char tmp[]=TINYTIFF_FULLVERSION;
    return tmp;
}

