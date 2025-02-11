#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define BLOCK_SIZE 512
#define NUM_TRAILING_BLOCKS 2

int fill_tar_header(tar_header *header, const char *filename);
int remove_trailing_bytes(const char *archive_name, size_t num_bytes);

int update_files_in_archive(const char *archive_name, const file_list_t *files) {
    // Open archive in read/write mode
    int archiveFile = open(archive_name, O_RDWR);
    if (archiveFile < 0) {
        perror("Error: Unable to open archive file");
        return -1;
    }

    // Find where to append new files
    off_t appendPointer = 0;
    while (1) {
        tar_header header;
        ssize_t bytesRead = read(archiveFile, &header, sizeof(tar_header));

        if (bytesRead < 0) {
            perror("Error: Failed to read header block");
            close(archiveFile);
            return -1;
        }
        if (bytesRead == 0 || header.name[0] == '\0') {
            break;
        }

        // Calculate padded file size
        off_t fileSize = strtol(header.size, NULL, 8);
        off_t paddedSize = ((fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

        // Skip past file data
        if (lseek(archiveFile, paddedSize, SEEK_CUR) < 0) {
            perror("Error: Failed to seek past file contents");
            close(archiveFile);
            return -1;
        }

        appendPointer = lseek(archiveFile, 0, SEEK_CUR);
    }

    // Seek to the position where new files will be appended
    if (lseek(archiveFile, appendPointer, SEEK_SET) < 0) {
        perror("Error: Failed to seek to append position");
        close(archiveFile);
        return -1;
    }

    // Append new files
    node_t *current = files->head;
    while (current != NULL) {
        tar_header header;
        if (fill_tar_header(&header, current->name) != 0) {
            perror("Error: Failed to fill tar header");
            close(archiveFile);
            return -1;
        }

        // Write the header
        if (write(archiveFile, &header, sizeof(tar_header)) != sizeof(tar_header)) {
            perror("Error: Failed to write header to archive");
            close(archiveFile);
            return -1;
        }

        // Open the file to copy its content
        int file_fd = open(current->name, O_RDONLY);
        if (file_fd < 0) {
            perror("Error: Failed to open file");
            close(archiveFile);
            return -1;
        }

        // Write the file content in blocks
        char buffer[BLOCK_SIZE] = {0};
        ssize_t bytesRead;
        while ((bytesRead = read(file_fd, buffer, BLOCK_SIZE)) > 0) {
            if (write(archiveFile, buffer, BLOCK_SIZE) != BLOCK_SIZE) {
                perror("Error: Failed to write file contents to archive");
                close(file_fd);
                close(archiveFile);
                return -1;
            }
            memset(buffer, 0, BLOCK_SIZE);
        }

        if (bytesRead < 0) {
            perror("Error: Failed to read file");
            close(file_fd);
            close(archiveFile);
            return -1;
        }

        close(file_fd);
        current = current->next;
    }

    // Write trailing blocks at end of archive
    char empty_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_TRAILING_BLOCKS; i++) {
        if (write(archiveFile, empty_block, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("Error: Failed to write trailing blocks");
            close(archiveFile);
            return -1;
        }
    }

    // Trim excess trailing bytes
    if (remove_trailing_bytes(archive_name, NUM_TRAILING_BLOCKS * BLOCK_SIZE) != 0) {
        perror("Error: Failed to remove trailing bytes");
        close(archiveFile);
        return -1;
    }

    close(archiveFile);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);

    // TODO: Parse command-line arguments and invoke functions from 'minitar.h'
    // to execute archive operations

    const char *archive_name = NULL;
    int operation = 0;

    // Checking the agruments in the command line to check for each minitat function
    // Depending on the operation, makes the value operation have a different number
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            operation = 1;
        } else if (strcmp(argv[i], "-a") == 0) {
            operation = 2;
        } else if (strcmp(argv[i], "-t") == 0) {
            operation = 3;
        } else if (strcmp(argv[i], "-u") == 0) {
            operation = 4;
        } else if (strcmp(argv[i], "-x") == 0) {
            operation = 5;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            archive_name = argv[i + 1];
            i++;
        } else {
            if (file_list_add(&files, argv[i]) != 0) {
                perror("Error: Failed to add file to list");
                file_list_clear(&files);
                return 1;
            }
        }
    }

    // If no real operation was used or archive_name does not exist, report an error
    if (operation == 0 || archive_name == NULL) {
        printf("Error: INvalid operation or missing archive name.\n");
        file_list_clear(&files);
        return 1;
    }

    // Used a switch case depending on the operation number and called respected function
    int result = 0;
    switch (operation) {
        case 1:
            result = create_archive(archive_name, &files);
            break;
        case 2:
            result = append_files_to_archive(archive_name, &files);
            break;
        case 3:
            result = get_archive_file_list(archive_name, &files);
            break;
        case 4:
            result = update_files_in_archive(archive_name, &files);
            break;
        case 5:
            result = extract_files_from_archive(archive_name);
            break;
        default:
            printf("Error: Unsupported operation.\n");
            file_list_clear(&files);
            return 1;
    }

    // If the operation failed, report an error
    if (result != 0) {
        fprintf(stderr, "Error: Archive creation failed.\n");
        file_list_clear(&files);
        return 1;
    }

    file_list_clear(&files);
    return 0;
}
