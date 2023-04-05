#include <getopt.h>
#include <iostream>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <vector>

using namespace std;
int SIZE_OF_INPUT;

struct ArgCmd{
    char* path_to_input;
    char* path_to_code;
    int x;
    int a;
    int c;
    int m;
};

struct work {
    pthread_barrier_t* barrier;
    char* text;
    char* output_text;
    char* pseudorandom_seq;
    int down_index;
    int top_index;
};

void* LCG(void* cmd_argv_ptr)
{
    ArgCmd* cmd_argv = static_cast<ArgCmd*>(cmd_argv_ptr);
    int x = cmd_argv->x;
    int a = cmd_argv->a;
    int c = cmd_argv->c;
    int m = cmd_argv->m;

    int count_of_int = SIZE_OF_INPUT / sizeof(int);
    int* buff = new int[count_of_int + 1];
    buff[0] = x;

    for(size_t i = 1; i < count_of_int + 1; i++)
    {
        buff[i]= (a * buff[i-1] + c) % m;
    }

    char* seq = reinterpret_cast<char *>(buff);
    return seq;
}
void* encrypt(void * work_ptr)
{
    work* work = static_cast<work*>(work_ptr);
    int top_index = work->top_index;
    int down_index = work->down_index;

    while(down_index < top_index)
    {
        work->output_text[down_index] = work->pseudorandom_seq[down_index] ^ work->text[down_index];
        down_index++;
    }

    int status = pthread_barrier_wait(work->barrier);
    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD) {
        exit(status);
    }

    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc != 13)
    {
        std::cout << "Arguments is not correct" << std::endl;
        exit(1);
    }
    int c;
    ArgCmd cmd_argv;
    while ((c = getopt(argc, argv, "i:o:x:a:c:m:")) != -1)
    {
        switch (c)
        {
            case 'i':
                cmd_argv.path_to_input = optarg;
                break;
            case 'o':
                cmd_argv.path_to_code = optarg;
                break;
            case 'x':
                cmd_argv.x = atoi(optarg);
                break;
            case 'a':
                cmd_argv.a = atoi(optarg);
                break;
            case 'c':
                cmd_argv.c = atoi(optarg);
                break;
            case 'm':
                cmd_argv.m = atoi(optarg);
                break;
            default:
                break;
        }
    }
    if (optind < argc) {
        std::cout << "elements weren't recognised" << std::endl;
        exit(1);
    }

    int input_file = open(cmd_argv.path_to_input, O_RDONLY);
    if (input_file == -1)
    {
        std::cout << "сant open " << cmd_argv.path_to_input << " file" << std::endl;
        exit(1);
    }

    SIZE_OF_INPUT = lseek(input_file, 0, SEEK_END);
    if (SIZE_OF_INPUT > 15000)
    {
        std::cout << "the file is too large "<< std:: endl;
        exit(1);
    }
    lseek(input_file, 0, SEEK_SET);

    char* text = new char[SIZE_OF_INPUT];
    if(read(input_file, text, SIZE_OF_INPUT) == -1)
    {
        std::cout << "the input file cannot be mapped to RAM" << std::endl;
        exit(1);
    }

    pthread_t keygen_thread;
    if (pthread_create(&keygen_thread, NULL, LCG, &cmd_argv) != 0)
    {
        std::cout << "cant create a new keygen thread" << std::endl;
        exit(1);
    }

    char* pseudorandom_seq = nullptr;
    if(pthread_join(keygen_thread, (void**)&pseudorandom_seq))
    {
        std::cout << "cant join a keygen thread thread" << std::endl;
        exit(1);
    }

    pthread_barrier_t barrier;

    
    int number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);

    pthread_barrier_init(&barrier, NULL, number_of_processors + 1);
    pthread_t crypt_threads[number_of_processors];
    std::vector <work*> works;

    size_t part_len = SIZE_OF_INPUT / number_of_processors;
    if (SIZE_OF_INPUT % number_of_processors != 0) {
        part_len ++;
    }

    char* output_text = new char[SIZE_OF_INPUT];
    for(int i = 0; i < number_of_processors; i++)
    {
        work* work = new work;

        work->barrier = &barrier;
        work->text = text;
        work->output_text = output_text;
        work->pseudorandom_seq = pseudorandom_seq;
        work->down_index = i * part_len;

        if (i == number_of_processors - 1)
            work->top_index = SIZE_OF_INPUT;
        else
            work->top_index = work->down_index + part_len;

        works.push_back(work);
        pthread_create(&crypt_threads[i], NULL, encrypt, work);
    }

    int status = pthread_barrier_wait(&barrier);
    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
        std::cout << "some problems" << std::endl;
        exit(status);
    }

    int output_file = open(cmd_argv.path_to_code, O_WRONLY, O_TRUNC);
    if (output_file == -1)
    {
        std::cout << "cant open " << cmd_argv.path_to_code << " file" << std::endl;
        exit(1);
    }

    write(output_file, output_text, SIZE_OF_INPUT);

    close(output_file);

    pthread_barrier_destroy(&barrier);

    return 0;
}