#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

int main(int argc, char *argv[]){ 
	
	int set = 0; 
	FILE *fp = stdin;
	/*if command arguments are present in argv[1], exit accordingly*/
	if((set = getopt(argc,argv,argv[1])) != -1 ) {
		printf("wordle: invalid command line\n");
      		exit(1);
	}

	if(argc != 3) {
		printf("wordle: invalid number of args\n");
		exit(1);
	}
      	
	fp = fopen(argv[1],"r");
	if (fp == NULL) {
		printf("wordle: cannot open file\n");
		exit(1);
	}
	

	char *word = argv[2];

	if(word == NULL) {
		exit(1);
	}

	char *buffer = malloc(256);
	while (fgets(buffer, 256, fp) != NULL) { 
		char *x = calloc(sizeof(char),strlen(buffer));
		int z = 0;
		int b = 0;
		/* this for loop removes nonalpha characters */
		for ( int i = 0; i < strlen(buffer);i++){ 
			if(isalpha(buffer[i]) == 0){
				z++;
				continue;
			}
			
			else if(strchr(word,buffer[i])){
				b++;
				break;
			}
			else{
				x[i-z] = buffer[i];
			}	
		}

		if(b != 0 ) {
			free(x);
			continue;
		}

		int result = strncasecmp(x,argv[argc-1],strlen(argv[argc-1]));
			if (result == 0 && strlen(x) == 5 ){
				free(x);
				continue;
			}
	       		else if (result != 0 && strlen(x) == 5) {
				printf("%s\n",x);
				free(x);
				continue;
			}
			else {
				free(x);
				continue;
			}
	}
	free(buffer);
	fclose(fp);	
	return 0;
}

