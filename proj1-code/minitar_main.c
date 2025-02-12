#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

// Adding the functions made so they can run in the main function
int update_files_in_archive(const char *archive_name, const file_list_t *file);

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
            if (result == 0) {
                node_t *current = files.head;
                while (current != NULL) {
                    printf("%s\n", current->name);
                    current = current->next;
                }
            }
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
        file_list_clear(&files);
        return 1;
    }

    file_list_clear(&files);
    return 0;
}


/*
 * Updates an archive file using archive_name and a new list of files to possibly be updated
 *
 * Returns 0 upon success, and -1 upon error
 *
 * Creates a new file list and obtains the current list in the archive file
 *
 * The compares the two files list using file_list_is_subset, then uses the append method to attach the files if valid
 */
int update_files_in_archive(const char *archive_name, const file_list_t *files) {
    // Initialize a new linked list of files that will be extracted by the archive list function
    file_list_t archive_files;
    file_list_init(&archive_files);

    // Getting the linked list of files from the current acrhive
    int result = get_archive_file_list(archive_name, &archive_files);
    if (result != 0) {
        perror("Error: Failed to obtain archive list");
        file_list_clear(&archive_files);
        return -1;
    }

    // Checking to see if the files being appended is a subset of the archive files present
    if(!file_list_is_subset(files, &archive_files)) {
        fprintf(stderr, "Error: One or more of the specified files is not already present in archive\n");
        file_list_clear(&archive_files);
        return -1;
    }

    // Appends new versions of the files to the archive, ensuring they are written in chunks
    result = append_files_to_archive(archive_name, files);
    if (result != 0) {
        perror("Error: Failed to update archive file");
        file_list_clear(&archive_files);
        return -1;
    }

    file_list_clear(&archive_files);
    return 0;
}
