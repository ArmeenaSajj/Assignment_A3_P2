//Armeena Sajjad, Salma Khai
#include <iostream>
#include <fstream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>

struct SharedBlock {
    char rubric[5];        // stores letters for the 5 rubric items
    char studentID[5];     // 4-digit student id + null
    bool marked[5];        // keeps track of which questions are done
};

// random delay between two times (in seconds)
static void waitRandom(double low, double high) {
    int micro = low * 1e6 + (rand() % (int)((high - low) * 1e6));
    usleep(micro);
}

// loads rubric from the rubric file into shared memory
static void loadRubric(const std::string &path, SharedBlock *mem) {
    std::ifstream file(path);
    std::string line;
    int i = 0;

    while (std::getline(file, line) && i < 5) {
        // format: 1,A → the letter is at position 2
        mem->rubric[i] = line[2];
        i++;
    }
}

// loads the student id from current exam file
static void loadExam(const std::string &path, SharedBlock *mem) {
    std::ifstream file(path);
    std::string id;
    std::getline(file, id);

    // copy into char array
    strncpy(mem->studentID, id.c_str(), 4);
    mem->studentID[4] = '\0';

    // reset questions for new exam
    for (int i = 0; i < 5; i++)
        mem->marked[i] = false;
}

// the TA process code
static void runTA(int id, SharedBlock *mem, int *examPtr) {
    int &examNum = *examPtr;

    while (true) {

        // check and maybe change rubric
        for (int i = 0; i < 5; i++) {
            std::cout << "TA " << id << " checking rubric item " << (i + 1) << "\n";
            waitRandom(0.5, 1.0);

            // small chance of modifying the rubric
            if (rand() % 5 == 0) {
                mem->rubric[i]++;

                // write back to rubric file (in part A it's okay if races happen)
                std::ofstream out("rubric.txt");
                for (int j = 0; j < 5; j++) {
                    out << (j + 1) << "," << mem->rubric[j] << "\n";
                }
            }
        }

        // marking questions
        for (int q = 0; q < 5; q++) {
            if (!mem->marked[q]) {
                mem->marked[q] = true;

                std::cout << "TA " << id << " marking student "
                          << mem->studentID << " question " << (q + 1) << "\n";

                waitRandom(1.0, 2.0);
            }
        }

        // check for sentinel student "9999"
        if (strcmp(mem->studentID, "9999") == 0)
            break;

        // load next exam
        examNum++;
        std::string nextExam = "exam_folder/exam" + std::to_string(examNum);

        loadExam(nextExam, mem);
    }

    exit(0);
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cout << "Usage: ./part2a <num_TAs>\n";
        return 1;
    }

    int nTAs = std::stoi(argv[1]);
    if (nTAs < 2) {
        std::cout << "Need at least 2 TAs.\n";
        return 1;
    }

    srand(time(NULL));

    // create shared memory region
    SharedBlock *shared = (SharedBlock *) mmap(
        NULL,
        sizeof(SharedBlock),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1, 0
    );

    if (shared == MAP_FAILED) {
        std::cout << "Shared memory failed.\n";
        return 1;
    }

    loadRubric("rubric.txt", shared);

    int examCounter = 1;
    loadExam("exam_folder/exam1", shared);

    // fork TA processes
    for (int i = 0; i < nTAs; i++) {
        int taID = i + 1;
        pid_t pid = fork();

        if (pid == 0) {
            runTA(taID, shared, &examCounter);
        }
    }

    // parent waits for all TAs to finish
    for (int i = 0; i < nTAs; i++)
        wait(NULL);

    munmap(shared, sizeof(SharedBlock));
    return 0;
}
