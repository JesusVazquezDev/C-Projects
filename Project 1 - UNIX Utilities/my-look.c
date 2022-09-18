#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

int main(int argc, char *argv[]){ 
	
	int set = 0; 
	FILE *fp = stdin;

	while ((set = getopt(argc,argv,"Vhf")) != -1 ) {
    		switch (set) {
      			case 'V':
				printf("my-look from CS537 Spring 2022\n");
				exit(0);	
      			case 'h':
				printf("HELPFUL INFORMATION ABOUT THE PROGRAM HERE");
				exit(0);
      			case 'f':
      				fp = fopen(argv[2],"r");
				if (fp == NULL ) {
					printf("my-look: cannot open file\n");
					exit(1);
				}
				break;
			case '?':
				printf("my-look: invalid command line\n");
        			exit(1);
			default:
				printf("my-look: default invalid command line\n");
				exit(1);
		}
	}


	char *buffer = malloc(256);
	while (fgets(buffer, 256, fp ) != NULL ) { 
		char *x = calloc(sizeof(char),strlen(buffer));
		int z = 0;

		for ( int i = 0; i < strlen(buffer);i++){ 
			if(isalpha(buffer[i]) == 0){
				z++;
				continue;
			}
			else{
				x[i-z] = buffer[i];
			}
		}
		int result = strncasecmp(x,argv[argc-1],strlen(argv[argc-1]));
		free(x);
		  if (result == 0){
			  printf("%s",buffer);
		  }
		  else if (result < 0){
			  continue;
		  }
		  else{
			  continue;
		  }
	}
	free(buffer);
	fclose(fp);	
	return 0;
}

