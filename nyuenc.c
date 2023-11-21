#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#define DEFAULT_WB_SIZE 1024
#define ARR_COUNT(arr)  (sizeof(arr)/sizeof(arr[0]))

typedef struct RLE_Entry{//output, formatted
	char c;
	unsigned char count;
} __attribute__ ((packed)) RLE_Entry;//to pack count and char into 2byte block, instead of count being in next 4byte block(removing padding bytes) 

typedef struct MFILE{//upon receiving, we need file size. compacting input+size into struct, instead of 2 args
	char* mem;
	unsigned int size;
}MFILE;

void PrintRLE(void* buffer,unsigned int size){
	RLE_Entry* rle = buffer;
	unsigned int count = size / sizeof(RLE_Entry);
	for(unsigned int i = 0; i < count; i++){
		RLE_Entry e = rle[i];
		printf("%c %d ",e.c,(unsigned int)e.count);
	}
	printf("\n");
}

unsigned int RLE(MFILE file,void* out,unsigned int* o_size){
	struct RLE_Entry* cur = out;//writing out to array current output arr
	unsigned int out_size = *o_size;//size of output array

	for(unsigned int i = 1; i < file.size; i++){//i iterates through input array (file.mem)
		char c = file.mem[i];
		if(cur->c != c){//checking for new characters
			cur ++;//next index in output
			{
				char* end = (char*)cur;
				char* start = out;
				if ((end - start) >= out_size){//preventing allocation errors, ensuring enough memory
					out_size += DEFAULT_WB_SIZE;
					out = realloc(out, out_size);
				}
				assert((end - start) < out_size); 
			}
			cur->c = c;
			cur->count = 1;
		}
		else{
			cur->count ++;
		}
	}
	cur++;
	*o_size = out_size;
	return (char*)cur - (char*)out;//output buffer size
}


int main(int argc, char* argv[]){
	MFILE files[128] = {0};
	
	for (int i = 1 ; i < argc; i++){
		int fd = open(argv[i], O_RDONLY, S_IRUSR | S_IWUSR);
		if (fd == -1) perror("file descriptor failed.\n");
		// Get file size
		struct stat sb;
		if (fstat(fd, &sb) == -1) perror("couldn't get file size.\n");
		// Map file into memory
		char *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (addr == MAP_FAILED) perror("mapping failed.\n");
		
		files[i - 1].mem = addr;
		files[i - 1].size = sb.st_size;
	}
	unsigned int buffer_size = DEFAULT_WB_SIZE;//memory allocated for writing out
	void* write_buffer = malloc(buffer_size);//everything to write out
//thinking of write_buffer as array
	char* write_pos = write_buffer;//tracking index of write_buffer
	{
		RLE_Entry* cur = (RLE_Entry*)write_pos;//incrementing index of write_buffer, essentially
		cur->c = files[0].mem[0];//next char
		cur->count = 1;
	}

	for(int i = 0; i < argc - 1; i++){
		unsigned int written_size = RLE(files[i],write_pos,&buffer_size);
		write_pos += written_size;
		//stitching
		if(i + 1 < argc - 1){
			RLE_Entry* prev = (RLE_Entry*)(write_pos - sizeof(RLE_Entry));
			MFILE file = files[i + 1];
			if(prev->c == file.mem[0]){
				prev->count ++;
				write_pos = (char*)prev;
			}
			else{
				prev ++;
				prev->c = file.mem[0];
				prev->count = 1;
			}
		}
	}
	unsigned int write_size = write_pos - (char*)write_buffer;
	//PrintRLE(write_buffer,write_size);
	write(STDOUT_FILENO,write_buffer,write_size);

	return 0;
}
