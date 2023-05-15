/*
	RT-11 Adapter Package for CP/M

	Modification history (most recent first)

	Rev. 1.2  -- August 1983 added Copy command to copy all files
			from an RT-11 disk to a CP/M disk.
						Tony Nicholson

	Rev. 1.1c -- June 1983 fixed r50toa routine so it translates a
			'Z' correctly.		Tony Nicholson

	Rev. 1.1b -- December 1982 fixed initial disk selection of the
			RT11 disk drive by modifying the BIOS(SEL_DSK)
			call to include an extra parameter to force
			disk type selection.	Tony Nicholson

	Rev. 1.1a -- November 1982 added conditional for Godbout DISK1
			DMA Floppy disk controller in the header file
			so that sector numbering is correct.
						Tony Nicholson

	Rev. 1.1 -- March 1981 consisting of adding a valid system date
			word to all files placed on the RT-11 disk and
			putting the volume ID on a disk when the directory
			is initialized.  This will keep RT-11 versions
			later than V02C from choking.

	Rev. 1.0 -- July 1980

	copyright (c) 1980, William C. Colley, III

The main-line routine is here, as are the individual command processors.
*/

#include "RT11.H"

/*
Main program..........
*/

main()
{
	char temp[20], t, d, m, y;
	puts("\nRT-11 File I/O Package -- Rev. 1.2 -- August 1983\n");
	puts("     copyright (c) 1980, William C. Colley, III\N");
#ifdef DISK1
	puts("\nConfigured for Godbout DISK1 DMA Floppy disk controller.\n");
#endif
	puts("\nPut RT-11 disk into drive B, and CP/M disk into drive A.\n");
	puts("THIS IS IMPORTANT.  Hit CR when you're ready.");
	while(1)
	{
		gets(temp);
		if (temp[0] == '\0') break;
		puts("Are you sure?");
	}
	puts("\nOK.\n");
	sel_flag = 0;
	bdos(INIT_BDOS);

/* Get system date word for later RT-11 versions. */

	do
	{
		puts("Today's date (dd/mm/yy)? ");
		scanf("%d/%d/%d",&d,&m,&y);
	}
	while (m == 0 || m > 12 || d == 0 || d > 31 || y < 72 || y > 99);
	sysdate = (m << 10) + (d << 5) + (y - 0110);

	while((t = getcom()) != 'Q')
	{
		switch (t)
		{
			case 'C':	copy_files();		break;
			case 'D':	list_dir();		break;
			case 'E':	erase_file();		break;
			case 'G':	get_file();		break;
			case 'I':	init_disk();		break;
			case 'P':	put_file();		break;
			case 'R':	rename_file();		break;
			case 'T':	type_file();		break;
			default:
				puts("\nCommands available are:\n\n");
				puts("(C)opy all RT-11 files to CP/M disk\n");
				puts("(D)irectory list\n");
				puts("(E)rase file on RT-11 disk\n");
				puts("(G)et file from RT-11 disk\n");
				puts("(I)initialize RT-11 disk\n");
				puts("(P)ut file onto RT-11 disk\n");
				puts("(R)ename file on RT-11 disk\n");
				puts("(Q)uit program\n");
				puts("(T)ype file on RT-11 disk\n");
				putchar('\n');
				break;
		}
	}
	puts("\nReinsert system disk and type any key.");
	getchar();
	putchar('\n');
}

/*
Routine to erase a file on the RT-11 disk.
*/

erase_file()
{
	int file_name[3];
	if (!get_RT_name("File to erase? ",file_name)) return;
	if (!delete(file_name)) puts("Error -- file does not exist\n");
}

/*
Routine to rename a file on the RT-11 disk.
*/

rename_file()
{
	int old_name[3], new_name[3];
	if (!get_RT_name("Old name of file? ",old_name)) return;
	if (!get_RT_name("New name of file? ",new_name)) return;
	if (!rename(old_name,new_name)) puts("Error -- file does not exist\n");
}

/*
Routine to list the directory on the RT-11 disk.
*/

list_dir()
{
	int t;
	char s[4], volume_id[512], temp[13];
	temp[12] = '\0';
	getmon(sysdate >> 10,s);
	printf(" %2d-%s-%2d",(sysdate >> 5) & 037, s,
		(sysdate & 037) + 0110);
	emt_375(READ,1,1,volume_id);
	puts("\n Volume ID: ");
	putstr(temp,&volume_id[236*2]);
	puts(temp);
	puts("\n Owner    : ");
	putstr(temp,&volume_id[242*2]);
	puts(temp);
	usrcom();
	puts("\nName  .Ext   Blks    Date     Start    Extras\n");
	do
	{
		while(*(dir_pointer + 1) != ENDSEG)
		{
			if (*(dir_pointer + 1) == PERM)
			{
				print_name(dir_pointer + 2);
				printf("   %3d   ",_getword(dir_pointer,4));
				if (t = _getword(dir_pointer,6) & 0x7fff)
				{
					getmon(t >> 10,s);
					printf("%2d-%s-%2d   ",(t >> 5) & 037,
						s,(t & 037) + 0110);
				}
				else puts("            ");
				printf("%03o   ",file_start);
				for (t = 0; t < directory.extra_bytes; t++)
					printf("  %03o",*(dir_pointer+t+14));
			}
			else printf("< unused >   %3d               %03o",
				_getword(dir_pointer,4),file_start);
			incr1();
			putchar('\n');
		}
	}
	while (nxblk());
}

/*
Function to copy all files on an RT-11 disk to a CP/M disk.
*/

copy_files()
{
	char temp[20], CP_drive, CP_file[20], xfer_buffer[512];
	int t, RT_file[3], fd, RT_file_count, i, j, k;
	struct dir {
		int name[3];
	} RT_dir[128];
	puts("CP/M disk drive name? ");
	gets(temp);
	if (temp[0] == 0) return;
	if ((CP_drive = toupper(temp[0])) == 'B')
		{
		puts("Error -- Drive B: is an RT-11 disk\n");
		return;
		}
	if ((CP_drive < 'A') || (CP_drive > 'P'))
		{
		puts("Error -- Illegal CP/M drive\n");
		return;
		}
	/* read RT-11 directory, recording permanent filenames in table */
	RT_file_count = 0;
	usrcom();
	do
	{
		while(*(dir_pointer + 1) != ENDSEG)
		{
			if (*(dir_pointer + 1) == PERM)
				{
				copy_RTname(dir_pointer + 2, RT_dir[RT_file_count]);
				RT_file_count++;
				}
			incr1();
		}
	}
	while (nxblk() && (RT_file_count < 128));
	/* now copy the files */
	puts("Copying\n");
	for ( i = 0; i<RT_file_count ; i++ )
		{
		copy_RTname(RT_dir[i],RT_file);
		print_name(RT_file);
		sprint_name(RT_file,temp);
		CP_file[0] = CP_drive;
		CP_file[1] = ':';
		k = 2;
		for ( j = 0; j<10 ; j++)
			if (temp[j] != ' ') CP_file[k++] = temp[j];
		CP_file[k] = '\0';
		lookup(RT_file);
		if ((fd = creat(CP_file)) == -1)
			{
			puts(" Error -- could not create destination file\n");
			return;
			}
		for (t = _getword(dir_pointer,4); t > 0; t--)
			{
			emt_375(READ,file_start++,1,xfer_buffer);
			if (write(fd,xfer_buffer,4) == -1)
				{
				puts(" Error -- CP/M disk full\n");
				close(fd);
				return;
				}
			}
		close(fd);
		puts(" to ");puts(CP_file);puts("\n");
		}
}


/*
Function to get a file from the RT-11 disk onto the CP/M disk.
*/

get_file()
{
	char CP_file[20], xfer_buffer[512];
	int t, RT_file[3], fd;
	if (!get_RT_name("RT-11 file name? ",RT_file)) return;
	puts("CP/M file name? ");
	gets(CP_file);
	if (CP_file[0] == '\0') sprint_name(RT_file,CP_file);
	if (!lookup(RT_file))
	{
		puts("Error -- no source file\n");
		return;
	}
	if ((fd = creat(CP_file)) == -1)
	{
		puts("Error -- could not create destination file\n");
		return;
	}
	for (t = _getword(dir_pointer,4); t > 0; t--)
	{
		emt_375(READ,file_start++,1,xfer_buffer);
		if (write(fd,xfer_buffer,4) == -1)
		{
			puts("Error -- CP/M disk full\n");
			close(fd);
			return;
		}
	}
	close(fd);
}

/*
Routine to type an RT-11 file on the console.
*/

type_file()
{
	char c, xfer_buffer[512];
	int file_name[3], i, j;
	if (!get_RT_name("File to type? ",file_name)) return;
	if (!lookup(file_name))
	{
		puts("Error -- file does not exist\n");
		return;
	}
	for (i = _getword(dir_pointer,4); i > 0; i--)
	{
		emt_375(READ,file_start++,1,xfer_buffer);
		for (j = 0; j < 512; j++)
		{
			c = xfer_buffer[j];
			if ((c == '\0') || c == ('Z' - 64))
				return;
			putchar(c);
		}
	}
}

/*
Routine to put a file onto the RT-11 disk.
*/

put_file()
{
	int RT_file[3], fd, size, j;
	char CP_file[20], xfer_buffer[512];
	puts("CP/M file name? ");
	gets(CP_file);
	if (CP_file[0] == '\0') return;
	if (!get_RT_name("RT-11 file name? ",RT_file)) return;
	if ((fd = open(CP_file,0)) == -1)
	{
		puts("Error -- could not open source file\n");
		return;
	}
	size = filesize(CP_file);
	if ((file_start = enter(RT_file,size)) == 0) return;
	while (--size >= 0)
	{
		for (j = 0; j < 512; xfer_buffer[j++] = '\0');
		if (read(fd,xfer_buffer,4) == -1)
		{
			puts("Error -- disk read on CP/M disk\n");
			close(fd);
			return;
		}
		emt_375(WRITE,file_start++,1,xfer_buffer);
	}
	close(fd);
	klose(RT_file);
}

/*
Routine to initialize the directory on an RT-11 disk.
*/

init_disk()
{
	char temp[50], *gets();
	unsigned volume_id[256];
	puts("Are you sure you want to do this (Y/N)? ");
	gets(temp);
	if (temp[0] != 'Y')
	{
		puts("\t.....Aborted\n");
		return;
	}
	setmem(&directory,1024,0);
	puts("Number of directory segments (1 to 31)? ");
	while (1)
	{
		scanf("%d",&directory.total_segments);
		if (directory.total_segments > 0 &&
			directory.total_segments < 32) break;
		puts("Say again? ");
	}
	puts("Extra bytes per directory entry? ");
	scanf("%d",&directory.extra_bytes);
	directory.highest_segment = 1;
	directory.first_block = (directory.highest_segment + 1) * 2 + 4;
	directory.entries[1] = EMPTY;
	_putword(directory.entries,4,488 - 2 * directory.total_segments);
	directory.entries[15 + directory.extra_bytes] = ENDSEG;
	segrw(WRITE,1);
	setmem(volume_id,512,' ');
	volume_id[0] = 0;
	volume_id[233] = 1;
	volume_id[234] = 6;
	volume_id[235] = 0107251;
	puts("Volume ID (CR for default)? ");
	putstr(&volume_id[236], *gets(temp)=='\0' ? "RT11A" : temp);
	puts("Owner name (CR for default)? ");
	if (*gets(temp) != '\0') putstr(&volume_id[242], temp);
	putstr(&volume_id[248], "DECRT11A");
	emt_375(WRITE,1,1,volume_id);
	puts("\t.....Done\n");
}

/*
Internal function in initialize directory routine to put up to 12 characters
from a string into the core image of the volume ID block (block 1 on the disk).
Note that this routine is an addition at Ver 1.0.
*/

putstr(buffer,string)
char *buffer, *string;
{
	char i;
	for (i = 12; i > 0 && *string != '\0'; i--)
		*buffer++ = toupper(*string++);
}

