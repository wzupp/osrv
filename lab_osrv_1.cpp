#include <cstdlib>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <vector>
#include <time.h>

#define MAX_SIZE 5000

using namespace std;


typedef struct
{
	int x;
    int a;
    int c;
    int m;
    
    char* PathFileInput;
    char* PathFileOutput;
}Opt;

typedef struct
{
	int x;
    int a;
    int c;
    int m;
	
    int count_of_int ;
}lkgParams;

typedef struct 
{
	int downDiap;
    int topDiap;
	char* outMsgText;
    char* message;
    char* randomSequence;
    int size;
    pthread_barrier_t* barrier;
}worker;

void* lkg(void* s)
{
    lkgParams *param = reinterpret_cast<lkgParams *>(s);
    int a = param->a;
    int m = param->m;
    int c = param->c;
    int count_of_int  = param->count_of_int ;

    int* buff = new int[count_of_int  / sizeof(int) + 1];
    buff[0] = param->x;

    for(int i = 1; i < count_of_int  / sizeof(int) + 1; i++)
    {
        buff[i]= (a * buff[i-1] + c) % m;
    }

    return reinterpret_cast<void *>(buff);
}

void* cryptCalc(void * cryptParametrs)
{
	
    worker* param = reinterpret_cast<worker*>(cryptParametrs);
    int topDiap = param->topDiap;
	
    int downDiap = param->downDiap;
	
    
    
    while(downDiap <= topDiap)
    { 
        param->outMsgText[downDiap] = param->randomSequence[downDiap] ^ param->message[downDiap];
        
        downDiap++;
		
    }

    int status = pthread_barrier_wait(param->barrier);
    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD) {
        exit(status);
    }

    return nullptr;
}

void FreeMemory(char* outMsgText, char* message, char* randomSequence)
{
	printf("");
    delete[] outMsgText;
    delete[] message;

    if (randomSequence != nullptr)
        delete[] randomSequence;
}

int main (int argc, char **argv) 
{
    
    int option;
	Opt Args = { 0, 0, 0, 0, NULL, NULL};
	while ((option=getopt(argc, argv,"i:o:x:a:c:m:")) != -1)
	{
		switch(option)
		{
			case'i':
				printf("Found argument \"i = %s\".\n", optarg);
				Args.PathFileInput = optarg;
				printf("Name successfully written: %s.\n",Args.PathFileInput);
				break;				
			case'o':
				printf("Found argument \"o = %s\".\n", optarg);
				Args.PathFileOutput = optarg;
				printf("Name successfully written: %s.\n",Args.PathFileOutput);
				break;
			case'x':
				printf("Found argument \"x = %s\".\n", optarg);
				Args.x = atoi(optarg);
				printf("Optvalue successfully written: %d.\n",Args.x);
				break;
			case'a':
				printf("Found argument \"a = %s\".\n", optarg);
				Args.a = atoi(optarg);
				printf("Optvalue successfully written: %d.\n",Args.a);
				break;
			case'c':
				printf("Found argument \"c = %s\".\n", optarg);
				Args.c = atoi(optarg);
				printf("Optvalue successfully written: %d.\n",Args.c);
				break;
			case'm':
				printf("Found argument \"m = %s\".\n", optarg);
				Args.m = atoi(optarg);
				printf("Optvalue successfully written: %d.\n",Args.m);
				break;
			case'?':
				printf("Error found!\n");
				getchar();
				exit(1);
				break;
		}	
	}



    int inputFile = open(Args.PathFileInput, O_RDONLY);
    if (inputFile == -1)
    {
        printf("File is not opened %s \n",Args.PathFileInput);
        exit(-1);
    }

    int fSize = lseek(inputFile, 0, SEEK_END);
    printf("File size is %d \n",fSize);
    if(fSize == -1)
    {
        printf("Error with getting file size");
        exit(-1);
    }

    
    if (fSize == 0)
    {
        printf("File is empty \n");
        exit(-1);
    }

    
    if (fSize > 5000)
    {
        printf("File size is bigger then acceptable %d \n",MAX_SIZE);
        exit(-1);
    }
    
    
    char* outMsgText = new char[fSize];
    char* message = new char[fSize];
	char* randomSequence = nullptr;
    
    lseek(inputFile, 0, SEEK_SET);

    
    if(read(inputFile, message, fSize) == -1)
    {
        printf("Cannot read to buffer");
        FreeMemory(outMsgText, message, randomSequence);
        exit(-1);
    }

    
    
	int availableWorkerThread = sysconf(_SC_NPROCESSORS_ONLN);
    printf("Currently available processes: %d \n",availableWorkerThread);

    lkgParams lkgParam;
	
	lkgParam.x=Args.x;
    lkgParam.a=Args.a;
    lkgParam.c=Args.c;
    lkgParam.m=Args.m;
    lkgParam.count_of_int  = fSize;

    pthread_t Thread;
    pthread_t cryptThread[availableWorkerThread];

    if (pthread_create(&Thread, NULL, lkg, &lkgParam) != 0)
    {
        printf("Failed to create a new thread\n");
        FreeMemory(outMsgText, message, randomSequence);
        exit(-1);
    }

    int randomSequenceJoinStatus = pthread_join(Thread, (void**)&randomSequence);
    if(randomSequenceJoinStatus != 0)
    {
        printf("Unable to join randomSequence %d ",randomSequenceJoinStatus);
        FreeMemory(outMsgText, message, randomSequence);
        exit(-1);
    }

    pthread_barrier_t barrier;

    pthread_barrier_init(&barrier, NULL, availableWorkerThread + 1);
    vector <worker*> workers;
	
    for(int i = 0; i < availableWorkerThread; i++)
    {
        worker* workerPar = new worker;

        workerPar->randomSequence = randomSequence;
		
        workerPar->size = fSize;
        workerPar->outMsgText = outMsgText;
        workerPar->message = message;
        workerPar->barrier = &barrier;

        int current_len = fSize / availableWorkerThread;
        int ostatok = fSize % availableWorkerThread;
		printf("Ostatok %d\n", ostatok);
		
		
        workerPar->downDiap = i * current_len;
		
        workerPar->topDiap = (i+1) * current_len + (i == availableWorkerThread - 1 ? ostatok : -1);
		printf("topDiap %d \t - downDiap %d\n", workerPar->topDiap, workerPar->downDiap);
    
        workers.push_back(workerPar);
        pthread_create(&cryptThread[i], NULL, cryptCalc, workerPar);
    }
   

    int status = pthread_barrier_wait(&barrier);

    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD) 
    {
        FreeMemory(outMsgText, message, randomSequence);
        workers.clear();
        exit(status);
    }

    int out;
    if ((out=open(Args.PathFileOutput, O_WRONLY | O_TRUNC)) == -1) 
    {
        printf("File is not opened %s", Args.PathFileOutput);
    }
    else
    {
	
        if(write(out, outMsgText, fSize) != fSize)
            printf("Write fault\n");

        close(out);
    }

    pthread_barrier_destroy(&barrier);


    FreeMemory(outMsgText, message, randomSequence);

    workers.clear();
    
    return 0;
}
