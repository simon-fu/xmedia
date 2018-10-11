#include <stdio.h>
#include <iostream>

int main(){
	int m;
	int i,j;
	int a[4][4]={{1,2,3,4},{5,6,7,8},{9,10,11,12},{13,14,15,16}};

        int N = 2*(4-1);
        int cursor = 0;
        for(int i = 0; i <= N; i++) {
	   cursor = i % 4;

	  if(i < 4) {
           for(int j = 0; j <= cursor; j++) {
               std::cout<<a[j][i-j] << std::endl;
           }
	  }else {
	      for(int j=cursor + 1; j < 4; j++) {
                std::cout<<a[j][i-j] << std::endl;
	      }
	  }
        }
/*
	for(m=0;m<=6;m++)
	{
		for(i=0;i<4;i++)
		{
			for(j=0;j<4;j++)
			{
				if((i+j)==m){
					printf("%d,",a[i][j]);
					// printf("i=%d, j=%d\n", i, j);
				}
			}
		}
	}
	printf("\n");
*/
	return 0;
}
