#include <stdio.h>
#include <stdlib.h>
#include <time.h>


int main() {
	srand(time(NULL));
	int numA = 0;
	int numB = 0;
	int numC = 0;
	int total = 10000;
	for(int i = 0; i < total; ++i){
		int r = rand()%100;
		if(r < 10){
			++numA;
		}else if(r < 30){
			++numB;
		}else{
			++numC;
		}
	}
	printf("A=%d/%d, %.2f\n", numA, total, 100.0f*numA/total);
	printf("B=%d/%d, %.2f\n", numB, total, 100.0f*numB/total);
	printf("C=%d/%d, %.2f\n", numC, total, 100.0f*numC/total);
	return 0;
}


