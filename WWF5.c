#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h> 
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

// Struct to hold words from the dictionary
typedef struct wordListNode
{
    char word[30];
    struct wordListNode *next;
} wordListNode;

// Struct to hold game words
typedef struct gameListNode
{
    char word[30];
    bool isFound;
    struct gameListNode *next;
} gameListNode;

// Struct to pass arguments to the thread
typedef struct pthreadArguments
{
    int *clientS;
    char masterWord[30];
} pthreadArguments;

// Global variables to store the word linked list roots
wordListNode *wordRoot = NULL;
gameListNode *gameRoot = NULL;
int wordcount = 0;
char userInput[100];
char mWord[100];

// Function declarations
int initialization();
void displayWorld(char *buffer, size_t bufferSize, char *masterWord);
void acceptInput(char *filePath);
int isDone();
int *getLetterDistribution(char *input, int *instancesOfLetters);
int compareCounts(int *letterDistribution, int *letterDistribution2);
wordListNode *getRandomWord();
int findWords(wordListNode *masterWord);
void cleanupGameListNodes();
void cleanupWordListNodes();
void *handleRequest(void *args);
int serverSetup();
void printGameLinkedList();

// Initializes the word list from a dictionary file
int initialization() {
    FILE *fp;
    wordListNode *tempNode = NULL;
    wordListNode *previous = NULL;
    char character[30];

    fp = fopen("2of12.txt", "r");
    if (fp == NULL) {
        perror("Failed to open dictionary file");
        exit(1);
    }

    // Read each line from the dictionary file and add it to the word linked list
    while ((fgets(character, sizeof(character), fp) != NULL)) {
        character[strcspn(character, "\r\n")] = '\0';

        tempNode = (wordListNode *)malloc(sizeof(wordListNode));
        if (!tempNode) {
            perror("Failed to allocate memory for word node");
            exit(1);
        }
        tempNode->next = NULL;
        strcpy(tempNode->word, character);

        if (wordRoot == NULL) {
            wordRoot = tempNode;
        } else {
            previous = wordRoot;
            while (previous->next != NULL) {
                previous = previous->next;
            }
            previous->next = tempNode;
        }
        wordcount++;
    }

    printf("Total words loaded: %d\n", wordcount);
    fclose(fp);
    srand(time(NULL));
    return 0;
}

// Displays the current game state as an HTML response
void displayWorld(char *buffer, size_t bufferSize, char *masterWord) {
    snprintf(buffer, bufferSize, "HTTP/1.1 200 OK\r\ncontent-type: text/html; charset=UTF-8\r\n\r\n<html><h1>%s</h1>", mWord);

    gameListNode *gameHead = gameRoot;
    while (gameHead != NULL) {
        strcat(buffer, "<div>");
        if (gameHead->isFound) {
            strcat(buffer, gameHead->word);
        } else {
            for (int i = 0; i < strlen(gameHead->word); i++) {
                strcat(buffer, "_");
            }
        }
        strcat(buffer, "</div>");
        gameHead = gameHead->next;
    }

    strcat(buffer, "<form submit=\"words\"><input type=\"text\" name=\"move\" autofocus></input></form></body></html>");
}

// Accepts the user's input and marks the word as found if it matches
void acceptInput(char *filePath) {
    gameListNode *gameHead = gameRoot;

    while (gameHead != NULL) {
        if (strcmp(filePath, gameHead->word) == 0 && gameHead->isFound == false) {
            gameHead->isFound = true;
            printf("Found: %s\n", gameHead->word);
            break;
        }
        gameHead = gameHead->next;
    }
}

// Checks if all words have been found, ending the game
int isDone() {
    gameListNode *gameHead = gameRoot;
    while (gameHead != NULL) {
        if (gameHead->isFound == false) {
            return 0; // Game is not done
        }
        gameHead = gameHead->next;
    }
    return 1; // Game is done
}

// Returns the letter distribution of the input string
int *getLetterDistribution(char *input, int *instancesOfLetters) {
    for (int i = 0; i < strlen(input); i++) {
        char word = tolower(input[i]);
        if (word >= 'a' && word <= 'z') {
            instancesOfLetters[word - 'a']++;
        }
    }
    return instancesOfLetters;
}

// Compares two letter distributions to check if a word can be formed
int compareCounts(int *letterDistribution, int *letterDistribution2) {
    for (int index = 0; index < 26; index++) {
        if (letterDistribution2[index] > letterDistribution[index]) {
            return 0; // Word cannot be formed
        }
    }
    return 1; // Word can be formed
}

// Picks a random word from the dictionary that is at least 6 characters long
wordListNode *getRandomWord() {
    int randomNumber = rand() % wordcount;
    int index = 0;
    wordListNode *head = wordRoot;

    while (index < randomNumber && head != NULL) {
        head = head->next;
        index++;
    }

    while (head != NULL && strlen(head->word) < 6) {
        head = head->next;
    }

    return head;
}

// Populates the game linked list with words that match the letter distribution of the master word
int findWords(wordListNode *masterWord) {
    gameListNode *tempNode = NULL;
    gameListNode *previous = NULL;
    wordListNode *wordListHead = wordRoot;

    int masterWordLetterDistribution[26] = {0};
    getLetterDistribution(masterWord->word, masterWordLetterDistribution);

    while (wordListHead != NULL) {
        int letterDistribution[26] = {0};
        getLetterDistribution(wordListHead->word, letterDistribution);

        if (compareCounts(masterWordLetterDistribution, letterDistribution) && strlen(wordListHead->word) > 4 && strcmp(masterWord->word, wordListHead->word) != 0) {
            tempNode = (gameListNode *)malloc(sizeof(gameListNode));
            if (!tempNode) {
                perror("Failed to allocate memory for game node");
                exit(1);
            }
            tempNode->next = NULL;
            strcpy(tempNode->word, wordListHead->word);

            if (gameRoot == NULL) {
                gameRoot = tempNode;
            } else {
                previous = gameRoot;
                while (previous->next != NULL) {
                    previous = previous->next;
                }
                previous->next = tempNode;
            }
        }
        wordListHead = wordListHead->next;
    }
    return 0;
}

// Frees the memory allocated for the game linked list
void cleanupGameListNodes() {
    gameListNode *nextNode;
    while (gameRoot != NULL) {
        nextNode = gameRoot->next;
        free(gameRoot);
        gameRoot = nextNode;
    }
    gameRoot = NULL;
}

// Frees the memory allocated for the word linked list
void cleanupWordListNodes() {
    wordListNode *nextNode;
    while (wordRoot != NULL) {
        nextNode = wordRoot->next;
        free(wordRoot);
        wordRoot = nextNode;
    }
    wordRoot = NULL;
}

// Handles the client request in a separate thread
void *handleRequest(void *args) {
    pthreadArguments *arguments = (pthreadArguments *)args;
    int clientfd = *(arguments->clientS);
    free(arguments->clientS);
    free(arguments);

    char buffer[1000];
    ssize_t bytes_received = recv(clientfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        perror("Failed to receive the data");
        close(clientfd);
        return NULL;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the received data

    char *getMethod = strtok(buffer, " ");
    if (getMethod == NULL || strcmp(getMethod, "GET") != 0) {
        perror("Invalid method, exiting...");
        close(clientfd);
        return NULL;
    }

    char *move = strtok(NULL, "=");
    char *filePath = strtok(NULL, " ");
    if (filePath == NULL) {
        perror("Invalid file path, exiting...");
        close(clientfd);
        return NULL;
    }

    acceptInput(filePath);

    if (strcmp(filePath, "/favicon.ico") == 0) {
        close(clientfd);
        return NULL;
    }

    char htmlBuffer[10000];
    displayWorld(htmlBuffer, sizeof(htmlBuffer), arguments->masterWord);

    char winPageBuffer[10000];
    snprintf(winPageBuffer, sizeof(winPageBuffer), "HTTP/1.1 200 OK\r\ncontent-type: text/html; charset=UTF-8\r\n\r\n<html><body><h1>Congratulations! You solved it!<h1><div><a href=\"/\">Play again?</a></div></body></html>");

    if (isDone() == 0) {
        if (send(clientfd, htmlBuffer, strlen(htmlBuffer), 0) == -1) {
            perror("Failed to send html");
        }
    } else {
        if (send(clientfd, winPageBuffer, strlen(winPageBuffer), 0) == -1) {
            perror("Failed to send win");
        }
    }

    close(clientfd);
    return NULL;
}

// Sets up the server socket
int serverSetup() {
    struct addrinfo hints, *servinfo = NULL, *current;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, "8000", &hints, &servinfo) != 0) {
        perror("Failed to call getaddrinfo, exiting...");
        exit(1);
    }

    for (current = servinfo; current != NULL; current = current->ai_next) {
        sockfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (sockfd == -1) {
            perror("Failed to create socket, trying again");
            continue;
        }

        if (bind(sockfd, current->ai_addr, current->ai_addrlen) == -1) {
            close(sockfd);
            perror("Failed to bind, trying again");
            continue;
        }
        break;
    }

    if (current == NULL) {
        perror("Failed to bind to any address, exiting...");
        exit(1);
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, 10) == -1) {
        perror("Failed to listen, exiting...");
        exit(1);
    }

    printf("=======Server is listening on port 8000=======\n");
    return sockfd;
}

// Prints all words in the game linked list
void printGameLinkedList() {
    gameListNode *gameHead = gameRoot;
    while (gameHead != NULL) {
        printf("%s\n", gameHead->word);
        gameHead = gameHead->next;
    }
}

int main(int argc, char *argv[]) {
    // Initialize the word list from the dictionary file
    initialization();

    wordListNode *masterWord = getRandomWord();
    findWords(masterWord);
    printGameLinkedList();

    // Copy master word to global variable
    strcpy(mWord, masterWord->word);

    // Set up the server socket
    int sockfd = serverSetup();
    struct sockaddr_storage their_addr;
    socklen_t sin_size;

    while (1) {
        pthreadArguments *args = malloc(sizeof(pthreadArguments));
        if (!args) {
            perror("Failed to allocate memory for thread arguments");
            exit(1);
        }

        int *client_sock = malloc(sizeof(int));
        if (!client_sock) {
            perror("Failed to allocate memory for client socket");
            exit(1);
        }

        sin_size = sizeof(their_addr);
        *client_sock = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (*client_sock == -1) {
            perror("Failed to accept connection");
            free(args);
            free(client_sock);
            continue;
        }

        args->clientS = client_sock;
        strncpy(args->masterWord, masterWord->word, sizeof(args->masterWord) - 1);
        args->masterWord[sizeof(args->masterWord) - 1] = '\0';

        pthread_t id;
        pthread_create(&id, NULL, handleRequest, (void *)args);
        pthread_detach(id);
    }

    close(sockfd);
    return 0;
}
