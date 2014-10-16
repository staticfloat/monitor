// Compile me with `clang -dynamiclib libblocktest.mm -o libblocktest.dylib`
#include <stdio.h>

extern "C" {
	void runBlock( int (^theBlock)() );
	void runBlockWithArgs( int (^theBlock)(float x, float y) );
}

void runBlock( int (^theBlock)() ) {
    printf("About to run theBlock\n");
    if( theBlock != NULL ) {
        int z = theBlock();
        printf("Just ran theBlock with result: %d\n", z);
    }
}

void runBlockWithArgs( int (^theBlock)(float x, float y) ) {
	printf("About to run theBlock(1.0, 2.5)\n");
	if( theBlock != NULL ) {
		int z = theBlock(1.0f, 2.5f);
		printf("Just ran theBlock with result: %d\n", z);
	}
}