#include <memory.h>
#include <stdio.h>

typedef struct fileMeta {
    unsigned int length;
    unsigned char nameLength;
    char* name;
} fileMeta;


void fputDword(unsigned int value, FILE* file) {

    fputc((value & 0xFF), file);
    fputc(((value >> 8) & 0xFF), file);
    fputc(((value >> 16) & 0xFF), file);
    fputc(((value >> 24) & 0xFF), file);
}


int main(int argc, char* argv[]){
        
        FILE *diskImage, *fileImage;
        unsigned int i, j, fileCount, offset; 
        int tempChar;
        fileMeta* file;
        
        if(argc < 3){
                printf("Usage: %s out.rd file1 [file2 file3 ...]\n", argv[0]);
                return 0;
        }

        /*
        steps: 
                1) Malloc the array of fileMetas 
                2) For each file listed
                    - Open the file
                    - Scan to find the file size, store in file->length
                    - Set file->name to the argval
                    - Scan to find the length of the argval, store in file->nameLength
                    - Close the file
                3) offset = 4 + (fileCount*9) //Length of count int plus each file descriptor less the size of the filename strings
                4) For i = 0 to fileCount
                    - offset += file[i].nameLength
                5) Open disk file
                6) Write the file count
                7) For each file entry:
                    - write offset 
                    - offset += file->length
                    - write file->length
                    - write file->nameLength
                    - write file->name
                8) For each file entry:
                    - open the file
                    - write the content of the file 
                    - close the file
                9) Close the disk file
        */
          
        fileCount = argc - 2;
        file = (unsigned int*)malloc(4*fileCount);
        
        for(i = 2; i < argc; i++){

                fileImage = fopen(argv[i], "rb");
                if(!fileImage){
                        printf("Error: Could not open driver image '%s'!\n", argv[i]);
                        free(driverSize);
                        return -1;
                }
        
                while((tempChar = fgetc(fileImage)) != EOF)
                        file[i-2].length++;

                file[i-2].name = argv[i];        
                        
                for(file[i-2].nameLength = 0; file[i-2].name[file[i-2].nameLength]; file[i-2].nameLength++);
                
                fclose(fileImage);
        }
        
        offset = 4 + (fileCount * 9);
        
        for(i = 0; i < fileCount; i++) 
            offset += file[i].nameLength;
        
        
        diskImage = fopen(argv[1], "wb");
        if(!diskImage){
                printf("Error: Couldn't open disk image '%s'!\n", argv[1]);
                free(driverSize);
                return -1;
        }   
               
        fputDword(fileCount, diskImage);
 
        for(i = 0; i < fileCount; i++) {
            
            fputDword(offset, diskImage);
            fputDword(file[i].length, diskImage);
            fputc(file[i].nameLength, diskImage);
            
            for(j = 0; j < file[i].nameLength; j++)
                fputc(file[i].name[j], diskImage);
                
            offset += file[i].length;
        }
 
        for(i = 0; i < fileCount; i++) {
        
            fileImage = fopen(file[i].name, "rb");
            if(!fileImage){
                printf("Error: Could not open driver image '%s'!\n", argv[i]);
                free(driverSize);
                fclose(kernelImage);
                return -1;
            }
            
            for(j = 0; j < file[i].length; j++) 
                fputc(fgetc(fileImage), kernelImage);
        }
 
        free(driverSize);
        fclose(kernelImage);
}
