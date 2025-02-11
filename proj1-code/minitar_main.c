#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

int update_files_in_archive(const char *archive_name, const file_list_t *files) {
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
