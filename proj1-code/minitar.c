#include "minitar.h"

#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 128
#define BLOCK_SIZE 512

// Constants for tar compatibility information
#define MAGIC "ustar"

// Constants to represent different file types
// We'll only use regular files in this project
#define REGTYPE '0'
#define DIRTYPE '5'

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
  // Have to initially set header's checksum to "all blanks"
  memset(header->chksum, ' ', 8);
  unsigned sum = 0;
  char *bytes = (char *)header;
  for (int i = 0; i < sizeof(tar_header); i++) {
    sum += bytes[i];
  }
  snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
  memset(header, 0, sizeof(tar_header));
  char err_msg[MAX_MSG_LEN];
  struct stat stat_buf;
  // stat is a system call to inspect file metadata
  if (stat(file_name, &stat_buf) != 0) {
    snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
    perror(err_msg);
    return -1;
  }

  strncpy(header->name, file_name,
          100); // Name of the file, null-terminated string
  snprintf(header->mode, 8, "%07o",
           stat_buf.st_mode & 07777); // Permissions for file, 0-padded octal

  snprintf(header->uid, 8, "%07o",
           stat_buf.st_uid); // Owner ID of the file, 0-padded octal
  struct passwd *pwd =
      getpwuid(stat_buf.st_uid); // Look up name corresponding to owner ID
  if (pwd == NULL) {
    snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s",
             file_name);
    perror(err_msg);
    return -1;
  }
  strncpy(header->uname, pwd->pw_name,
          32); // Owner name of the file, null-terminated string

  snprintf(header->gid, 8, "%07o",
           stat_buf.st_gid); // Group ID of the file, 0-padded octal
  struct group *grp =
      getgrgid(stat_buf.st_gid); // Look up name corresponding to group ID
  if (grp == NULL) {
    snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s",
             file_name);
    perror(err_msg);
    return -1;
  }
  strncpy(header->gname, grp->gr_name,
          32); // Group name of the file, null-terminated string

  snprintf(header->size, 12, "%011o",
           (unsigned)stat_buf.st_size); // File size, 0-padded octal
  snprintf(header->mtime, 12, "%011o",
           (unsigned)stat_buf.st_mtime); // Modification time, 0-padded octal
  header->typeflag = REGTYPE; // File type, always regular file in this project
  strncpy(header->magic, MAGIC, 6); // Special, standardized sequence of bytes
  memcpy(header->version, "00", 2); // A bit weird, sidesteps null termination
  snprintf(header->devmajor, 8, "%07o",
           major(stat_buf.st_dev)); // Major device number, 0-padded octal
  snprintf(header->devminor, 8, "%07o",
           minor(stat_buf.st_dev)); // Minor device number, 0-padded octal

  compute_checksum(header);
  return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll
 * learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
  char err_msg[MAX_MSG_LEN];

  struct stat stat_buf;
  if (stat(file_name, &stat_buf) != 0) {
    snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
    perror(err_msg);
    return -1;
  }

  off_t file_size = stat_buf.st_size;
  if (nbytes > file_size) {
    file_size = 0;
  } else {
    file_size -= nbytes;
  }

  if (truncate(file_name, file_size) != 0) {
    snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
    perror(err_msg);
    return -1;
  }
  return 0;
}

/**
 * Creates an archive file using archive_name and stores the provided list of files
 * within it using files
 *
 * Returns 0 upon success, -1 upon error
 *
 * Loops through the linked list of files and using fill_tar_header to make the header
 * Then it opens the file using open() and reads and writes the contents to the archive file
 * using a buffer
 *
 * Then loops again to make the footer blocks by adding empty blocks at the end of the entire archive file
 */
int create_archive(const char *archive_name, const file_list_t *files) {
  // Creating and Opening the Archive file
  FILE *archive_file = fopen(archive_name, "wb");
  if (!archive_file) {
    perror("Error: Unable to open archive file");
    return -1;
  }

  // Looping through each file, and creating a header block and content blocks
  node_t *current = files->head;
  while (current != NULL) {
    // Through each file, fill the header by calling fill_tarr_header with the file name
    tar_header header;
    if (fill_tar_header(&header, current->name) != 0) {
      perror("Error: Failed to fill tar header");
      fclose(archive_file);
      return -1;
    }

    // Write the header to the archive file
    if (fwrite(&header, 1, sizeof(tar_header), archive_file) !=
        sizeof(tar_header)) {
      perror("Error: Failed to write header to archive");
      fclose(archive_file);
      return -1;
    }

    // Open the contents of the file after header creation
    FILE *file_fd = fopen(current->name, "rb");
    if (!file_fd) {
      perror("Error: Failed to open file");
      fclose(archive_file);
      return -1;
    }

    // Read the contents of the file and write to the archive file
    char buffer[BLOCK_SIZE] = {0};
    ssize_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, file_fd)) > 0) {
      if (fwrite(buffer, 1, BLOCK_SIZE, archive_file) != BLOCK_SIZE) {
        perror("Error: Failed to write file contents to archive");
        fclose(file_fd);
        fclose(archive_file);
        return -1;
      }
      memset(buffer, 0, BLOCK_SIZE);
    }

    if (ferror(file_fd)) {
      perror("Error: Failed to read file");
      fclose(file_fd);
      fclose(archive_file);
      return -1;
    }

    fclose(file_fd);
    current = current->next;
  }

  // Write trailing blocks to mark end of archive
    char empty_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_TRAILING_BLOCKS; i++) {
      if (fwrite(empty_block, 1, BLOCK_SIZE, archive_file) != BLOCK_SIZE) {
        perror("Error: Failed to write trailing blocks");
        fclose(archive_file);
        return -1;
      }
    }

    fclose(archive_file);
    return 0;
  }

  /*
  * Appends files to an existing tar archive.
  *
  * Returns 0 on success, -1 on error.
  *
  * Opens the archive in read/write mode. Then locates the end of the archive
  * and appends new files. Each file is appedned by creating a tar header,
  * writing file data in blocks, and calculating proper padding.
  *
  */
  int append_files_to_archive(const char *archive_name,
                              const file_list_t *files) {
    // Open arcieve in read/write mode
    FILE *archiveFile = fopen(archive_name, "rb+");
    if (!archiveFile) {
        perror("Error: Unable to open archive file");
        return -1;
    }

    // Find where to append new files
    off_t appendPointer = 0;
    while (1) {
        tar_header header;
        ssize_t bytesRead = fread(&header, 1, sizeof(tar_header), archiveFile);

        if (bytesRead < 0) {
            perror("Error: Failed to read header block");
            fclose(archiveFile);
            return -1;
        }
        if (bytesRead == 0 || header.name[0] == '\0') {
            break; // Found empty block (end of archive)
        }

        // Calculate padded file size
        off_t fileSize = strtol(header.size, NULL, 8);
        off_t paddedSize = ((fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

        // Skip past file data
        if (fseek(archiveFile, paddedSize, SEEK_CUR) < 0) {
            perror("Error: Failed to seek past file contents");
            fclose(archiveFile);
            return -1;
        }

        appendPointer = ftell(archiveFile);
    }

    // Seek to the position where new files will be appended
    if (fseek(archiveFile, appendPointer, SEEK_SET) < 0) {
        perror("Error: Failed to seek to append position");
        fclose(archiveFile);
        return -1;
    }

    // Append new files
    node_t *current = files->head;
    while (current != NULL) {
        tar_header header;
        if (fill_tar_header(&header, current->name) != 0) {
            perror("Error: Failed to fill tar header");
            fclose(archiveFile);
            return -1;
        }

        // Write the header
        if (fwrite(&header, 1, sizeof(tar_header), archiveFile) != sizeof(tar_header)) {
            perror("Error: Failed to write header to archive");
            fclose(archiveFile);
            return -1;
        }

        // Open the file to copy its content
        FILE *file_fd = fopen(current->name, "rb");
        if (!file_fd) {
            perror("Error: Failed to open file");
            fclose(archiveFile);
            return -1;
        }

        // Write the file content in blocks
        char buffer[BLOCK_SIZE] = {0};
        ssize_t bytesRead;
        while ((bytesRead = fread(buffer, 1, BLOCK_SIZE, file_fd)) > 0) {
            if (fwrite(buffer, 1, BLOCK_SIZE, archiveFile) != BLOCK_SIZE) {
                perror("Error: Failed to write file contents to archive");
                fclose(file_fd);
                fclose(archiveFile);
                return -1;
            }
            memset(buffer, 0, BLOCK_SIZE);
        }

        if (bytesRead < 0) {
            perror("Error: Failed to read file");
            fclose(file_fd);
            fclose(archiveFile);
            return -1;
        }

        fclose(file_fd);
        current = current->next;
    }

    // Write trailing blocks at end of archieve
    char empty_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_TRAILING_BLOCKS; i++) {
        if (fwrite(empty_block, 1, BLOCK_SIZE, archiveFile) != BLOCK_SIZE) {
            perror("Error: Failed to write trailing blocks");
            fclose(archiveFile);
            return -1;
        }
    }

    // Trim excess trailing bytes
    // if (remove_trailing_bytes(archive_name, NUM_TRAILING_BLOCKS * BLOCK_SIZE) != 0) {
    //     perror("Error: Failed to remove trailing bytes");
    //     fclose(archiveFile);
    //     return -1;
    // }

    fclose(archiveFile);
    return 0;
  }

  /*
   * Reads an archive file and extracts the list of contained file names
   *
   * Returns 0 for success, -1 for error
   *
   * Opens the file an extracts the tar header and reads the name of the header to the terminal
   * Then need to find the next header by calculating block sizes and finding the right number for the next header
   * and repeat the process
   *
   */
  int get_archive_file_list(const char *archive_name, file_list_t *files) {
    FILE *archive_file = fopen(archive_name, "rb");

    if (!archive_file) {
      perror("Error: Unable to open archive file");
      return -1;
    }

    while (1) {
      tar_header header;
      ssize_t bytes_read;

      if ((bytes_read = fread(&header, 1, sizeof(tar_header), archive_file)) < sizeof(tar_header)) {
        if (feof(archive_file)) {
          break;
        }
        perror("Error: Failed to read header block");
        fclose(archive_file);
        return -1;
      }

      if (header.name[0] == '\0') {
        break;
      }

      if (file_list_add(files, header.name) != 0) {
        perror("Error: Failed to add file to list");
        fclose(archive_file);
        return -1;
      }

      // finding the next header block to get file information
      off_t file_size = strtol(header.size, NULL, 8);
      off_t padded_size = ((file_size + BLOCK_SIZE -1) / BLOCK_SIZE) * BLOCK_SIZE;

      if (fseek(archive_file, padded_size, SEEK_CUR) != 0) {
        perror("Error: Failed to seek to next header");
        fclose(archive_file);
        return -1;
      }
    }

    fclose(archive_file);
    return 0;
  }

  /*
  * Extracts files from a tar archive and writes them to the filesystem.
  *
  * Returns 0 on success, -1 on error.
  *
  * Opens the archive file in read mode. Then iterates through each tar header
  * and extracts the corresponding file. It calculates the padded file size
  * to locate the next header.
  *
  * The extracted files are then written to disk, maintaining original structure.
  *
  */
  int extract_files_from_archive(const char *archive_name) {
    // Open archive in read mode
    FILE *archiveFile = fopen(archive_name, "rb");
    if (!archiveFile) {
        perror("Error: Unable to open archive file");
        return -1;
    }

    // Read through and extract the archive
    while (1) {
        tar_header header;
        ssize_t bytesRead = fread(&header, 1, sizeof(tar_header), archiveFile);

        if (bytesRead < sizeof(tar_header)) {
            perror("Error: Failed to read header block");
            fclose(archiveFile);
            return -1;
        }
        if (bytesRead == 0 || header.name[0] == '\0') {
            break;
        }

        // Calculate padded file size
        off_t fileSize = strtol(header.size, NULL, 8);
        off_t paddedSize = ((fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

        // Ensure valid file name
        if (strchr(header.name, '/') != NULL) {
            fprintf(stderr, "Error: Extraction of file with path not allowed: %s\n", header.name);
            fseek(archiveFile, paddedSize, SEEK_CUR);
            continue;
        }

        // Create and open the extracted file
        FILE *file_fd = fopen(header.name, "wb");
        if (!file_fd) {
            perror("Error: Failed to create extracted file");
            fseek(archiveFile, paddedSize, SEEK_CUR);
            continue;
        }

        // Read file content and write it to the extracted file
        char buffer[BLOCK_SIZE];
        ssize_t totalBytesRead = 0;
        while (totalBytesRead < fileSize) {
            ssize_t chunkSize = fread(buffer, 1, BLOCK_SIZE, archiveFile);
            if (chunkSize == 0) {
                perror("Error: Failed to read file data");
                fclose(file_fd);
                fclose(archiveFile);
                return -1;
            }
            ssize_t bytesToWrite = (chunkSize > (fileSize - totalBytesRead)) ? (fileSize - totalBytesRead) : chunkSize;
            if (fwrite(buffer, 1, bytesToWrite, file_fd) != bytesToWrite) {
                perror("Error: Failed to write extracted file");
                fclose(file_fd);
                fseek(archiveFile, paddedSize - totalBytesRead, SEEK_CUR);
                break;
            }
            totalBytesRead += chunkSize;
        }

        fclose(file_fd);

        // Skip any remaining padding bytes
        if (fseek(archiveFile, paddedSize - totalBytesRead, SEEK_CUR) != 0) {
            perror("Error: Failed to seek past file data");
            fclose(archiveFile);
            return -1;
        }
    }

    fclose(archiveFile);
    return 0;
}
