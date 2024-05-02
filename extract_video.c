/***************************************************************
 extract_video.c
Compile:
./cc
Test:
./extract_video -i /media/ipcam11/datadir0/ -o ./output -l -s 2014-10-13 -v
****************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
// #include <endian.h>

#if defined(__APPLE__)
// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#define __bswap_16(x) OSSwapInt16(x)
#define __bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#else
#include <byteswap.h>
#endif
// Index file data is saved little endian, so may need some conversion
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define htobe16(x) __bswap_16((uint16_t)(x))
#define htobe32(x) __bswap_32((uint32_t)(x))
#define htobe64(x) __bswap_64((uint64_t)(x))
#define htole16(x) ((uint16_t)(x))
#define htole32(x) ((uint32_t)(x))
#define htole64(x) ((uint64_t)(x))

#define be16toh(x) __bswap_16((uint16_t)(x))
#define be32toh(x) __bswap_32((uint32_t)(x))
#define be64toh(x) bswap64((uint64_t)(x))
#define le16toh(x) ((uint16_t)(x))
#define le32toh(x) ((uint32_t)(x))
#define le64toh(x) ((uint64_t)(x))
#else /* __BYTE_ORDER != __LITTLE_ENDIAN */
#define htobe16(x) ((uint16_t)(x))
#define htobe32(x) ((uint32_t)(x))
#define htobe64(x) ((uint64_t)(x))
#define htole16(x) __bswap_16((uint16_t)(x))
#define htole32(x) __bswap_32((uint32_t)(x))
#define htole64(x) __bswap_64((uint64_t)(x))

#define be16toh(x) ((uint16_t)(x))
#define be32toh(x) ((uint32_t)(x))
#define be64toh(x) ((uint64_t)(x))
#define le16toh(x) __bswap_16((uint16_t)(x))
#define le32toh(x) __bswap_32((uint32_t)(x))
#define le64toh(x) __bswap_64((uint64_t)(x))
#endif /* __BYTE_ORDER == __LITTLE_ENDIAN */

#define PROGRAM_VERSION "0.3"

struct FILE_IDX_HEADER
{
	uint64_t modifyTimes;	   // 8
	uint32_t version;		   // 4
	uint32_t avFiles;		   // 4
	uint32_t nextFileRecNo;	   // 4
	uint32_t lastFileRecNo;	   // 4
	uint8_t currFileRec[1176]; // 1276
	uint8_t res3[76];		   // 76
	uint32_t checksum;		   // 4
							   // length = 1280 bytes
};

struct FILE_IDX_RECORD
{
	uint32_t fileNo;  // 4
	uint16_t chan;	  // 2
	uint8_t res3[56]; // 56
	uint8_t res4[8];  // 56
	// 8
	uint16_t segRecNums;   // 2
	uint32_t startTime;	   // 4
	uint32_t endTime;	   // 4
	uint8_t status;		   // 1
	uint8_t res1;		   // 1
	uint16_t lockedSegNum; // 2
	uint8_t res2[4];	   // 4
	uint8_t infoTypes[8];  // 8
						   // length = 40 bytes
};

struct SEGMENT_IDX_RECORD
{
	uint8_t type;
	uint8_t status;
	uint8_t res1[2];	// 4
	uint32_t startTime; // 12
	uint8_t res11[4];
	uint32_t endTime; // 20
	uint8_t res12[4];
	uint32_t firstKeyFrame_absTime; // 28
	uint8_t res2[8];				// 32
	uint32_t firstKeyFrame_stdTime; // 36
	uint32_t startOffset;			// 46
	uint32_t endOffset;				// 50

	uint16_t startIndex; // 50
	uint8_t res21[2];			 // 50
	uint16_t endIndex; // 50
	uint8_t res22[2];			 // 50
	uint8_t infoNum[4];	 // 40

	uint32_t resolution;		// 44
	uint32_t lastFrame_stdTime; // 56
	uint8_t infoTypes[8];		// 64
	uint8_t infoStartTime[4];
	uint8_t infoEndTime[4];
};

typedef enum log_event
{
	LOG_DEBUG = 1,
	LOG_WARNING = 2,
	LOG_ERROR = 4,
	LOG_INFO = 8
} log_event;

FILE *_log_debug = NULL, *_log_warning = NULL, *_log_error = NULL, *_log_info = NULL;
void logger(log_event event, char *function, char *msg, ...);
char *timeformat(time_t time);
char *makefilename(char *path, char *name);
char *timefilename(char *prefix, char *postfix, uint16_t chan, time_t start, time_t end);

void FILE_IDX_HEADER_normalize(struct FILE_IDX_HEADER *f)
{
	f->modifyTimes = le64toh(f->modifyTimes);
	f->version = le32toh(f->version);
	f->avFiles = le32toh(f->avFiles);
	f->nextFileRecNo = le32toh(f->nextFileRecNo);
	f->lastFileRecNo = le32toh(f->lastFileRecNo);
	f->checksum = le32toh(f->checksum);
}

void FILE_IDX_RECORD_normalize(struct FILE_IDX_RECORD *f)
{
	f->fileNo = le32toh(f->fileNo);
	f->chan = le16toh(f->chan);
	f->segRecNums = le16toh(f->segRecNums);
	f->startTime = le32toh(f->startTime);
	f->endTime = le32toh(f->endTime);
	f->lockedSegNum = le16toh(f->lockedSegNum);
}

void SEGMENT_IDX_RECORD_normalize(struct SEGMENT_IDX_RECORD *f)
{
	f->startTime = le64toh(f->startTime);
	f->endTime = le64toh(f->endTime);
	f->firstKeyFrame_absTime = le64toh(f->firstKeyFrame_absTime);
	f->firstKeyFrame_stdTime = le32toh(f->firstKeyFrame_stdTime);
	f->lastFrame_stdTime = le32toh(f->lastFrame_stdTime);
	f->startOffset = le32toh(f->startOffset);
	f->endOffset = le32toh(f->endOffset);
}

int main(int argc, char **argv)
{
	struct FILE_IDX_HEADER header;
	struct FILE_IDX_RECORD file;
	struct SEGMENT_IDX_RECORD segment;

	char *inputPath = NULL, *outputPath = NULL, *matchString = NULL, buffer[8192];
	FILE *idxFile, *ivFile, *ovFile;

	unsigned int skip_existing = 0, verbose = 0, list_only = 0, totals_only = 0, pics_only = 0;

	unsigned long total_files = 0;
	unsigned long long total_filesize = 0;
	unsigned long total_time = 0;

	int err = 0, c, i, j, l, help = 0;

	_log_error = stderr;
	_log_info = stderr;

	// Parse options

	while (err == 0 && (c = getopt(argc, argv, "tvlkphi:o:s:")) != -1)
	{
		switch (c)
		{
		case 'i': // Format output
			logger(LOG_DEBUG, "main", "Format output using '%s'", optarg);
			inputPath = optarg;
			break;

		case 'o': // Server URL
			logger(LOG_DEBUG, "main", "Server URL set to '%s'", optarg);
			outputPath = optarg;
			break;

		case 's': // Regex
			logger(LOG_DEBUG, "main", "Match regex set to '%s'", optarg);
			matchString = optarg;
			break;

		case 'k': // Skip existing files in output dir
			skip_existing = 1;
			break;

		case 'l': // List only, don't extract
			list_only = 1;
			break;

		case 't': // Only show totals
			totals_only = 1;
			break;

		case 'p': // Create thumbs
			pics_only = 1;
			break;

		case 'v': // Verbose mode
			verbose = 1;
			break;

		case 'h':
		case '?':
			help = 1;
			printf("Read/Extract Hikvision Video File Storage\n");
			printf("Alexey Ozerov (c) 2014 - ver. %s\n\n", PROGRAM_VERSION);
			printf("Options\n");
			printf(" -? -h            Display this help\n");
			printf(" -i <path>        Input directory path\n");
			printf(" -o <path>        Output directory path\n");
			printf(" -s	<string>      List/extract only file names including string\n");
			printf(" -k				        Don't overwrite existing output files\n");
			printf(" -l				        List only, don't extract data\n");
			printf(" -t				        Only calculate and show totals\n");
			printf(" -v				        Verbose mode\n");
			printf(" -p				        Only create thumbnail pics\n");
		}
	}

	// Open index file

	if (!err && !help && inputPath != NULL)
	{
		idxFile = fopen(makefilename(inputPath, "index00.bin"), "rb");
		if (!idxFile)
		{
			logger(LOG_ERROR, "main", "Can't open file %s", makefilename(inputPath, "index00.bin"));
			err++;
			return 1;
		}
		else
		{

			// Read header

			if (fread(&header, sizeof(header), 1, idxFile) != 1)
			{
				logger(LOG_ERROR, "main", "Can't read %d header bytes", sizeof(header));
				err++;
				return 1;
			}

			FILE_IDX_HEADER_normalize(&header);
			/*
						int test=2;
						for (i = 0; i < sizeof(test); i++) { printf("%02x ", ((const unsigned char *)&test)[i]); }
			*/

			if (verbose)
			{
				printf("HEADER =================\n");
				printf("version: %u\n", header.version);
				printf("avFiles: %u\n", header.avFiles);
				printf("nextFileRecNo: %u\n", header.nextFileRecNo);
				printf("lastFileRecNo: %u\n", header.lastFileRecNo);
			}

			// Read files
			struct FILE_IDX_RECORD files[header.avFiles];

			for (i = 0; i < header.avFiles; i++)
			{

				if (fread(&file, sizeof(file), 1, idxFile) != 1)
				{
					logger(LOG_ERROR, "main", "Can't read %d bytes for file index", sizeof(file));
					err++;
					return 1;
				}

				FILE_IDX_RECORD_normalize(&file);

				if (file.chan != 0xFFFF) // Skip empty file records
				{
					files[i] = file;
					if (verbose)
					{
						printf("FILE %u =================\n", i);
						printf("fileNo: %u\n", file.fileNo);
						printf("chan: %u\n", file.chan);
						printf("startTime: %s\n", timeformat(file.startTime));
						printf("endTime: %s\n", timeformat(file.endTime));
					}
				}
			}

			// Read segments
			// Motion >> type: 3 status: 0

			for (i = 0; i < header.avFiles; i++)
			{
				for (j = 0; j < 256; j++)
				{
					if (fread(&segment, sizeof(segment), 1, idxFile) != 1)
					{
						logger(LOG_ERROR, "main", "Can't read %d bytes for segment index", sizeof(segment));
						err++;
						return 1;
					}

					SEGMENT_IDX_RECORD_normalize(&segment);

					if (segment.type != 0x00 && segment.startTime != 0 && segment.endTime != 0) // Skip empty segment records
					{
						if (files[i].startTime <= segment.startTime && files[i].endTime >= segment.endTime) {
							if (verbose)
							{
								printf("FILE %u SEG %u ============\n", i, j);
								printf("type: %u\n", segment.type);
								printf("status: %u\n", segment.status);
								printf("startTime: %s\n", timeformat(segment.startTime));
								printf("endTime: %s\n", timeformat(segment.endTime));
								printf("firstKeyFrame_absTime: %s\n", timeformat(segment.firstKeyFrame_absTime));
								printf("firstKeyFrame_stdTime: %u\n", segment.firstKeyFrame_stdTime);
								printf("lastFrame_stdTime: %u\n", segment.lastFrame_stdTime);
								printf("startOffset: %u\n", segment.startOffset);
								printf("endOffset: %u\n", segment.endOffset);
							}
						} else {
							if (verbose)
							{
								printf("FILE %u SEG %u ============\n", i, j);
								printf("skipping invalid file....\n");
								printf("FILE startTime: %s\n", timeformat(files[i].startTime));
								printf("SEG startTime: %s\n", timeformat(segment.startTime));
								printf("SEG endTime: %s\n", timeformat(segment.endTime));
								printf("FILE endTime: %s\n", timeformat(files[i].endTime));
							}
							continue;
						}

						// Extract from video file

						if (outputPath != NULL) // if (i==7) // Extract only from one file pls
						{
							size_t filesize = segment.endOffset - segment.startOffset;
							size_t l1, l2;
							int file_exists = 0;
							char *extStr = pics_only ? ".jpg" : ".mp4";
							char *ovFilename = makefilename(outputPath, timefilename("hikvideo", extStr, files[i].chan, segment.startTime, segment.endTime));

							if (matchString != NULL && strstr(ovFilename, matchString) == NULL)
								continue;


							if (pics_only && filesize > 1000000) {
								// Extract 1MB of footage. Assumed this will be enough to get the first frame
								filesize = 1000000;
							}

							if (!totals_only)
							{
								printf("File name: %s\n", ovFilename);
								printf("File size: %zu bytes\n", filesize);
								printf("Play time: %u sec\n", segment.endTime - segment.startTime);
							}

							total_files++;
							total_filesize += filesize;
							total_time += (segment.endTime - segment.startTime);

							// Check output file duplicates

							if (!list_only && !totals_only)
							{
								ovFile = fopen(ovFilename, "r");
								if (ovFile)
								{
									if (skip_existing)
										printf("File exists... Skipped!\n");
									else
										printf("File exists... Overwriting...\n");
									file_exists = 1;
									fclose(ovFile);
								}
							}

							// Open input file and copy data

							if ((!file_exists || !skip_existing) && !list_only && !totals_only)
							{
								char ovFilenameFinal[1000] = "";
								char ovFilenameTmp[1000] = "";
								
								if (pics_only) {
									strcpy(ovFilenameFinal, ovFilename);
									strcpy(ovFilenameTmp, ovFilename);
									strcat(ovFilenameTmp, ".tmp");
									
									ovFile = fopen(ovFilenameTmp, "wb");
								} else {
									ovFile = fopen(ovFilename, "wb");
								}
								if (!ovFile)
								{
									logger(LOG_ERROR, "main", "Can't open file %s", ovFilename);
									err++;
									return 1;
								}

								sprintf(buffer, "hiv%05u.mp4", i);
								ivFile = fopen(makefilename(inputPath, buffer), "rb");
								if (!ivFile)
								{
									logger(LOG_ERROR, "main", "Can't open file %s", makefilename(inputPath, buffer));
									err++;
									return 1;
								}
								if (fseek(ivFile, segment.startOffset, SEEK_SET) != 0)
								{
									logger(LOG_ERROR, "main", "Can't seek in file %s to position %u", buffer, segment.startOffset);
									err++;
									return 1;
								}

								while ((l1 = fread(buffer, 1, filesize > sizeof(buffer) ? sizeof(buffer) : filesize, ivFile)) > 0 && filesize > 0)
								{
									if (l1 < (filesize > sizeof(buffer) ? sizeof(buffer) : filesize))
									{
										logger(LOG_ERROR, "main", "Input file failure");
										err++;
										return 1;
									}

									l2 = fwrite(buffer, 1, l1, ovFile);
									if (l2 < l1)
									{
										logger(LOG_ERROR, "main", "Output file failure");
										err++;
										return 1;
									}

									filesize -= l1;
								}
								if (filesize > 0)
								{
									logger(LOG_ERROR, "main", "Input file truncated?");
									err++;
									return 1;
								}

								fclose(ovFile);
								fclose(ivFile);

								// create thumb from extracted ov
								if (pics_only) {
									char cmd[1000] = "";
									unlink(ovFilenameFinal);
									sprintf(cmd, "ffmpeg -i %s -vframes 1 -an %s >/dev/null 2>&1", ovFilenameTmp, ovFilenameFinal);
									if (verbose)
									{
										printf("creating thumbnail for %s....\n", ovFilenameFinal);
										printf("executing cmd: %s\n", cmd);
									}
									system(cmd);
									unlink(ovFilenameTmp);
								}
								
							}
						}
						else
						{
							logger(LOG_ERROR, "main", "Output directory path not specified");
							err++;
							return 1;
						}
					}
				}
			}

			printf("Total files: %lu\n", total_files);
			printf("Total file size: %llu bytes (=%llu MB)\n", total_filesize, total_filesize / 1024 / 1024);
			printf("Total play time: %lu sec (=%lu min)\n", total_time, total_time / 60);

			fclose(idxFile);
		}
	}
}

//***************************************************************
// Common functions
//***************************************************************

char timebuffer[100];

char *timeformat(time_t time)
{
	struct tm *tmptr = gmtime(&time);
	if (tmptr != NULL && strftime(timebuffer, sizeof(timebuffer), "%Y-%m-%d %H:%M:%S", tmptr))
		return timebuffer;
	else
		return "Error converting time";
}

char *timefilename(char *prefix, char *postfix, uint16_t chan, time_t start, time_t end)
{
	char buffer1[20], buffer2[20], buffer3[20] = "";
	struct tm *tmptr;

	tmptr = gmtime(&start);
	if (tmptr == NULL || !strftime(buffer1, sizeof(buffer1), "%Y-%m-%d_%H.%M.%S", tmptr))
		return "Error converting time";
	tmptr = gmtime(&end);
	if (tmptr == NULL || !strftime(buffer2, sizeof(buffer2), "%H.%M.%S", tmptr))
		return "Error converting time";
	sprintf(buffer3, "%u", chan);

	strcpy(timebuffer, prefix);
	strcat(timebuffer, "_ch");
	strcat(timebuffer, buffer3);
	strcat(timebuffer, "_");
	strcat(timebuffer, buffer1);
	strcat(timebuffer, "_to_");
	strcat(timebuffer, buffer2);
	strcat(timebuffer, postfix);
	return timebuffer;
}

//***************************************************************

char *filebuffer = NULL;
char *makefilename(char *path, char *name)
{
	if (filebuffer != NULL)
	{
		free(filebuffer);
		filebuffer = NULL;
	}

	filebuffer = malloc(strlen(path) + strlen(name) + 2);
	if (filebuffer)
	{
		strcpy(filebuffer, path);
		if (strlen(filebuffer) > 0 && filebuffer[strlen(filebuffer) - 1] != '/')
			strcat(filebuffer, "/");
		strcat(filebuffer, name);
	}
	return filebuffer;
}

//***************************************************************

void logger(log_event event, char *function, char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	switch (event)
	{
	case LOG_DEBUG:
		if (_log_debug)
		{
			fprintf(_log_debug, "message: logger.%s - ", function);
			vfprintf(_log_debug, msg, args);
			fprintf(_log_debug, "\n");
		}
		break;

	case LOG_WARNING:
		if (_log_warning)
		{
			fprintf(_log_warning, "warning: logger.%s - ", function);
			vfprintf(_log_warning, msg, args);
			fprintf(_log_warning, "\n");
		}
		break;

	case LOG_ERROR:
		if (_log_error)
		{
			fprintf(_log_error, "error: logger.%s - ", function);
			vfprintf(_log_error, msg, args);
			fprintf(_log_error, "\n");
		}
		break;

	case LOG_INFO:
		if (_log_info)
		{
			fprintf(_log_info, "info: logger.%s - ", function);
			vfprintf(_log_info, msg, args);
			fprintf(_log_info, "\n");
		}
		break;
	}
	va_end(args);
}
