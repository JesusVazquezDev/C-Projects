#include "types.h"
#include "user.h"
#include "iostat.h"

int
main(int argc, char *argv[]){

	if ( argc != 3){
		printf(1, " Please give proper amount of args \n");
		exit();
	}

	// Setting read/write count to 0
	struct iostat x = {0,0};

	for( int i = 0; i < atoi(argv[1]); i++){
		read(-1,"jesus" ,6);
	}


	for (int r = 0; r < atoi(argv[2]); r++){
		write(-1,"jesus",6);
	}


	int io = getiocounts(&x);

	int read = x.readcount;

	int write = x.writecount;

	printf(1,"%d %d %d\n", io, read, write);
	exit();
}
