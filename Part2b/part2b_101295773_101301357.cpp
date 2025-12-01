// Armeena Sajjad, Salma Khai 

#include <iostream>
#include <fstream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <cstring>
#include <cstdlib>

struct SharedMemory {
    char rubric[5];     
    char sid[5];        
    bool finished[5];   

    sem_t rubricLock;   
    sem_t markLock;     
    sem_t loadLock;     
};

// short random delay between two times
static void randomDelay(double low, double high) {
    int usec = low * 1e6 + (rand() % (int)((high - low) * 1e6));
    usleep(usec);
}

// reads rubric file into shared memory
static void loadRubric(const std::string &file, SharedMemory *sh) {
    std::ifstream in(file);
    std::string line;
    int i = 0;

    while (std::getline(in, line) && i < 5) {
        sh->rubric[i] = line[2];
        i++;
    }
}

// loads examID from exam file
static void loadExam(const std::string &file, SharedMemory *sh) {
    std::ifstream in(file);
    std::string id;
    std::getline(in, id);

    strncpy(sh->sid, id.c_str(), 4);
    sh->sid[4] = '\0';

    for (int i = 0; i < 5; i++)
        sh->finished[i] = false;
}

// TA logic using semaphores
static void TAwork(int taID, SharedMemory *sh, int *examIndex) {
    int &idx = *examIndex;

    while (true) {

        // checking rubric
        for (int i = 0; i < 5; i++) {
            std::cout << "TA " << taID << " checking rubric item " << (i + 1) << "\n";
            randomDelay(0.5, 1.0);

            sem_wait(&sh->rubricLock);

            if (rand() % 5 == 0) {
                sh->rubric[i]++;

                std::ofstream out("rubric.txt");
                for (int j = 0; j < 5; j++)
                    out << (j + 1) << "," << sh->rubric[j] << "\n";

                std::cout << "TA " << taID << " updated rubric entry " << (i + 1) << "\n";
            }

            sem_post(&sh->rubricLock);
        }

        // marking questions
        for (int q = 0; q < 5; q++) {

            sem_wait(&sh->markLock);
            bool available = !sh->finished[q];
            if (available)
                sh->finished[q] = true;
            sem_post(&sh->markLock);

            if (available) {
                std::cout << "TA " << taID << " marking SID "
                          << sh->sid << " question " << (q + 1) << "\n";
                randomDelay(1.0, 2.0);
            }
        }

        // stop if student id is sentinel
        if (strcmp(sh->sid, "9999") == 0)
            break;

        // load next exam safely
        sem_wait(&sh->loadLock);
        idx++;
        std::string nextFile = "exam_folder/exam" + std::to_string(idx);
        loadExam(nextFile, sh);
        sem_post(&sh->loadLock);
    }

    exit(0);
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cout << "Usage: ./part2b <num_TAs>\n";
        return 1;
    }

    int numTAs = std::stoi(argv[1]);
    if (numTAs < 2) {
        std::cout << "At least 2 TAs required.\n";
        return 1;
    }

    srand(time(NULL));

    SharedMemory *shared =
        (SharedMemory *) mmap(NULL, sizeof(SharedMemory),
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS,
                              -1, 0);

    if (shared == MAP_FAILED) {
        std::cout << "Shared memory error.\n";
        return 1;
    }

    // initialize semaphores
    sem_init(&shared->rubricLock, 1, 1);
    sem_init(&shared->markLock,   1, 1);
    sem_init(&shared->loadLock,   1, 1);

    // load initial files
    loadRubric("rubric.txt", shared);

    int examCounter = 1;
    loadExam("exam_folder/exam1", shared);

    for (int i = 0; i < numTAs; i++) {
        pid_t p = fork();
        if (p == 0) {
            TAwork(i + 1, shared, &examCounter);
        }
    }

    for (int i = 0; i < numTAs; i++)
        wait(NULL);

    sem_destroy(&shared->rubricLock);
    sem_destroy(&shared->markLock);
    sem_destroy(&shared->loadLock);

    munmap(shared, sizeof(SharedMemory));
    return 0;
}
