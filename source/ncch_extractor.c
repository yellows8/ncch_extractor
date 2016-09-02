#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

void print_hexdump(unsigned char *buf, unsigned int bufsz)
{
	unsigned int pos, curpos, blocksz;

	blocksz = 0x10;
	for(pos=0; pos<bufsz; pos+=0x10)
	{
		if(bufsz - pos < 0x10)blocksz = bufsz - pos;

		printf("%08x: ", pos);
		for(curpos=0; curpos<0x10; curpos++)
		{
			if((curpos & 1) == 0 && curpos > 0)printf(" ");
			if(curpos<blocksz)
			{
				printf("%02x", buf[pos + curpos]);
			}
			else
			{
				printf("  ");
			}
		}
		printf("  ");

		for(curpos=0; curpos<blocksz; curpos++)
		{
			if(isprint(buf[pos + curpos]))
			{
				printf("%c", buf[pos + curpos]);
			}
			else
			{
				printf(".");
			}
		}

		printf("\n");
	}
	printf("\n");
}

int main(int argc, char **argv)
{
	FILE *finput, *fout;
	struct stat filestats;
	int argi;
	unsigned int inoff = 0, insz = 0, outsz = 0;
	unsigned int pos;
	int outdirlen;

	size_t readsz;
	unsigned char *inbuf;

	char ncch_magic[4] = {"NCCH"};
	unsigned char reserved[0x30];

	char ncch_productcode[0x11];
	char exhdr_name[9];

	char infn[256];
	char outdir[256];
	char outfn[256];
	char sys_str[256];

	printf("ncch_extractor by yellows8\n");

	memset(reserved, 0, 0x30);
	memset(ncch_productcode, 0, 0x11);
	memset(exhdr_name, 0, 9);

	memset(infn, 0, 256);
	memset(outdir, 0, 256);

	for(argi=1; argi<argc; argi++)
	{
		if(strncmp(argv[argi], "--infn=", 7)==0)strncpy(infn, &argv[argi][7], 255);
		if(strncmp(argv[argi], "--outdir=", 9)==0)strncpy(outdir, &argv[argi][9], 255);

		if(strncmp(argv[argi], "--inoff=", 8)==0)sscanf(&argv[argi][8], "%x", &inoff);
		if(strncmp(argv[argi], "--insz=", 7)==0)sscanf(&argv[argi][7], "%x", &insz);
	}

	outdirlen = strlen(outdir);
	if(outdir[0]!=0 && outdir[outdirlen-1] != '/')
	{
		outdir[outdirlen] = '/';
	}

	if(infn[0]==0)
	{
		printf("Usage:\n");
		printf("--infn=<fn> Input filename\n");
		printf("--outdir=<path> Directory path to write the extracted NCCHs\n");
		printf("--inoff=<hexoff> Input-file/shift-file offset (0 by default)\n");
		printf("--insz=<hexsz> Input-file/shift-file size (filesize by default)\n");
		return 0;
	}

	finput = fopen(infn, "rb");
	if(finput==NULL)
	{
		printf("failed to open input file\n");
		return 1;
	}

	stat(infn, &filestats);
	if(insz==0)insz = filestats.st_size - inoff;

	printf("inoff=%x, insz=%x\n", inoff, insz);

	inbuf = (unsigned char*)malloc(insz);
	if(inbuf==NULL)
	{
		printf("failed to alloc mem.\n");
		free(inbuf);
		fclose(finput);

		return 2;
	}

	memset(inbuf, 0, insz);

	if(fseek(finput, inoff, SEEK_SET)==-1)
	{
		printf("failed to seek to %x in input file.\n", inoff);
		free(inbuf);
		fclose(finput);

		return 3;
	}

	readsz = fread(inbuf, 1, insz, finput);
	fclose(finput);

	if(readsz!=insz)
	{
		printf("reading failed.\n");
		free(inbuf);

		return 3;
	}

	for(pos=0; pos<insz; pos++)
	{
		if(memcmp(&inbuf[pos], ncch_magic, 4)==0 && memcmp(&inbuf[pos+0x21], reserved, 0x2f)==0)
		{
			outsz = *((unsigned int*)&inbuf[pos+4]);
			if(inbuf[pos+0x12]!=1)outsz*= 0x200;//mediaunit size is 1 for ncch format version 1.
			memcpy(ncch_productcode, &inbuf[pos+0x50], 0x10);
			memcpy(exhdr_name, &inbuf[pos+0x100], 0x8);

			printf("found NCCH @ %x productcode %s exhdr name %s size %x\n", pos-0x100, ncch_productcode, exhdr_name, outsz);

			if(strncmp(ncch_productcode, "N/XG3i7PeiBR8oW", 0x10)==0)memset(ncch_productcode, 0, 0x10);

			if(ncch_productcode[0])snprintf(outfn, 255, "%s%s-%s.cxi", outdir, ncch_productcode, exhdr_name);
			if(ncch_productcode[0]==0)snprintf(outfn, 255, "%s%s.cxi", outdir, exhdr_name);
			printf("writing to %s\n", outfn);

			fout = fopen(outfn, "wb");
			if(fout==NULL)
			{
				printf("failed to open output file.\n");
			}
			else
			{
				fwrite(&inbuf[pos-0x100], 1, outsz, fout);
				fclose(fout);

				printf("running ctrtool...\n");
				memset(sys_str, 0, 256);
				snprintf(sys_str, 255, "ctrtool -v --verify \"%s\" > \"%s\".info", outfn, outfn);
				system(sys_str);
			}
		}
	}

	free(inbuf);

	return 0;
}

