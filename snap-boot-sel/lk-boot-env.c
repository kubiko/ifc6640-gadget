/**
 * Copyright (C) 2021 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
  * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>
#include <malloc.h>
// #include "snappy_boot_v1.h"
#include "snappy_boot_v2.h"

#define RECOVERY "recovery"
#define RUNTIME "runtime"
enum Modes {unknown, runtime, recovery};

static uint32_t crc32(uint32_t crc, unsigned char *buf, size_t len)
{
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
    }
    return ~crc;
}

/**
 * Program to operare snap bootsel environment
 */
void print_usage() {
    printf("usage:\n"
           "\tTwo environment types are supported, 'recovery' and 'runtime'\n"
           "\tIf not passed, for update and read operation, type is determined\n"
           "\t\t-A, --runtime\n"
           "\t\t-B, --recovery\n"
           "\n"
           "\tAction options(required):\n"
           "\t\t-r, --read==<file name>\n"
           "\t\t\tread content of passed boot env\n"
           "\t\t\tadditional passed parameters are ignored\n\n"
           "\t\t-w, --write=<file name>\n"
           "\t\t\twrite new clean boot env to the file (file is created if it does not exists\n"
           "\t\t-u, --update=<file name>\n\n"
           "\t\t\tupdate existing boot env, file has to exists\n\n"
           "\tOptional parameters:\n"
           "\t\truntime:\n"
           "\t\t\t[-a, --kernel-status=<kernel status>]\n"
           "\t\t\t[-b, --snap-kernel=<kernel-snap revision>]\n"
           "\t\t\t[-c, --snap-try-kernel=<kernel-snap revision>]\n"
           "\t\t\t[-g, --boot-0-part=<boot image 0 part name>]\n"
           "\t\t\t[-i, --boot-1-part=<boot image 1 part name>]\n"
           "\t\t\t[-j, --boot-0-snap=<installed kernel snap revision>]\n"
           "\t\t\t[-k, --boot-1-snap=<installed kernel snap revision>]\n"
           "\t\t\t[-l, --bootimg-file=<name of bootimg in kernel snap>]\n"
           "\t\t\t[-m, --gadget-0-part=<gadget image 0 part name>]\n"
           "\t\t\t[-n, --gadget-1-part=<gadget image 1 part name>]\n"
           "\t\t\t[-o, --gadget-0-snap=<installed gadget snap revision>]\n"
           "\t\t\t[-p, --gadget-1-snap=<installed gadget snap revision>]\n"
           "\t\t\t[-q, --gadget-mode=<gadget mode>]\n"
           "\t\t\t[-s, --snap-gadget=<gadget snap revision>]\n"
           "\t\t\t[-t, --snap-try-gadget=<gadget snap revision>]\n"
           "\t\trecovery:\n"
           "\t\t\t[-d, --revovery-mode=<recovery mode>]\n"
           "\t\t\t[-e, --revovery-system=<recovery system label>]\n"
           "\t\t\t[-v, --recovery-0-part=<recovery boot image 0 part name>]\n"
           "\t\t\t[-x, --recovery-1-part=<recovery boot image 1 part name>]\n"
           "\t\t\t[-y, --recovery-0-snap=<installed kernel snap revision>]\n"
           "\t\t\t[-z, --recovery-1-snap=<installed kernel snap revision>]\n"
           "\t\t\t[-l, --bootimg-file=<name of bootimg(recovery) in kernel snap>]\n"
           "examples:\n"
           "\t$ lk-boot-env -r /dev/disk/by-partlabel/snapbootsel\n"
           "\t$ lk-boot-env runtime -r /dev/disk/by-partlabel/snapbootsel\n"
           "\t$ lk-boot-env runtime -u /dev/disk/by-partlabel/snapbootsel --kernel-status=try\n"
           "\t$ lk-boot-env recovery -r /dev/disk/by-partlabel/snaprecoverysel\n"
           "\n");
}

// Open env file, exit if it fails
FILE* open_file(const char *filename, const char *mode, const char *arg) {
    FILE *fp = NULL;
    if(!optarg) {
        printf( "Missing file name argument for %s option\n", arg );
        print_usage();
        exit(EXIT_FAILURE);
    }
    fp = fopen(filename, mode);
    if (!fp) {  /* validate file open for reading */
        fprintf (stderr, "error: file open failed '%s'.\n", filename);
        exit(EXIT_FAILURE);
    }
    return fp;
}

// read and validate boot environment from passed file
SNAP_RUN_BOOT_SELECTION_t* read_validate_runtime_environment(FILE *fp, enum Modes* mode) {
    SNAP_RUN_BOOT_SELECTION_t *bootSel;
    bootSel = (SNAP_RUN_BOOT_SELECTION_t *)malloc(sizeof(SNAP_RUN_BOOT_SELECTION_t));
    if (!bootSel) {
        fprintf (stderr, "error: failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    fseek(fp, 0, SEEK_SET);
    fread(bootSel, 1, sizeof(SNAP_RUN_BOOT_SELECTION_t), fp);
    if (bootSel->crc32 != crc32(0x0, (void *)bootSel, sizeof(SNAP_RUN_BOOT_SELECTION_t)-sizeof(uint32_t))) {
        free(bootSel);
        // if mode is not defined, fail softly
        if ( *mode != runtime ) {
            return NULL;
        } else {
            printf("BROKEN crc32!!!!! [0x%X] vs [0x%X]\n", bootSel->crc32, crc32(0x0, (void *)bootSel, sizeof(SNAP_RUN_BOOT_SELECTION_t)-sizeof(uint32_t)));
            exit(EXIT_FAILURE);
        }
    }
    printf ("Type: runtime environment\n");
    *mode = runtime;
    // check compulsory values of boot env structure
    if (bootSel->signature != SNAP_BOOTSELECT_SIGNATURE_RUN){
        fprintf (stderr, "error: Missing or wrong SIGNATURE value in environment\n");
        free(bootSel);
        exit(EXIT_FAILURE);
    }
    if (bootSel->version != SNAP_BOOTSELECT_VERSION_V2) {
        fprintf (stderr, "error: Missing or wrong VERSION value in environment\n");
        free(bootSel);
        exit(EXIT_FAILURE);
    }
    printf("crc32 validated [0x%X]\n", bootSel->crc32);
    return bootSel;
}

// read and validate recovery environment from passed file
SNAP_RECOVERY_BOOT_SELECTION_t* read_validate_recovery_environment(FILE *fp, enum Modes* mode) {
    SNAP_RECOVERY_BOOT_SELECTION_t *recoverySel;
    recoverySel = (SNAP_RECOVERY_BOOT_SELECTION_t *)malloc(sizeof(SNAP_RECOVERY_BOOT_SELECTION_t));
    if (!recoverySel) {
        fprintf (stderr, "error: failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    fseek(fp, 0, SEEK_SET);
    fread(recoverySel, 1, sizeof(SNAP_RECOVERY_BOOT_SELECTION_t), fp);
    if (recoverySel->crc32 != crc32(0x0, (void *)recoverySel, sizeof(SNAP_RECOVERY_BOOT_SELECTION_t)-sizeof(uint32_t))) {
        free(recoverySel);
        if ( *mode != recovery ) {
            return NULL;
        } else {
            printf("BROKEN crc32!!!!! [0x%X] vs [0x%X]\n", recoverySel->crc32, crc32(0x0, (void *)recoverySel, sizeof(SNAP_RECOVERY_BOOT_SELECTION_t)-sizeof(uint32_t)));
            exit(EXIT_FAILURE);
        }
    }
    *mode = recovery;
    printf ("Type: recovery environment\n");
    // check compulsory values of boot env structure
    if (recoverySel->signature != SNAP_BOOTSELECT_SIGNATURE_RECOVERY){
        fprintf (stderr, "error: Missing or wrong SIGNATURE value in environment\n");
        free(recoverySel);
        exit(EXIT_FAILURE);
    }
    if (recoverySel->version != SNAP_BOOTSELECT_VERSION_V2) {
        fprintf (stderr, "error: Missing or wrong VERSION value in environment\n");
        free(recoverySel);
        exit(EXIT_FAILURE);
    }
    printf("crc32 validated [0x%X]\n", recoverySel->crc32);
    return recoverySel;
}

// create clean boot select environment
SNAP_RUN_BOOT_SELECTION_t* create_clean_runtime_environment() {
    SNAP_RUN_BOOT_SELECTION_t *bootSel;
    bootSel = (SNAP_RUN_BOOT_SELECTION_t *)malloc(sizeof(SNAP_RUN_BOOT_SELECTION_t));
    if (!bootSel) {
        fprintf (stderr, "error: failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    memset(bootSel, 0, sizeof(SNAP_RUN_BOOT_SELECTION_t));
    // fill compulsory values of boot env structure
    bootSel->signature=SNAP_BOOTSELECT_SIGNATURE_RUN;
    bootSel->version=SNAP_BOOTSELECT_VERSION_V2;
    return bootSel;
}

// create clean recovery select environment
SNAP_RECOVERY_BOOT_SELECTION_t* create_clean_recovery_environment() {
    SNAP_RECOVERY_BOOT_SELECTION_t *recoverySel;
    recoverySel = (SNAP_RECOVERY_BOOT_SELECTION_t *)malloc(sizeof(SNAP_RECOVERY_BOOT_SELECTION_t));
    if (!recoverySel) {
        fprintf (stderr, "error: failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    memset(recoverySel, 0, sizeof(SNAP_RECOVERY_BOOT_SELECTION_t));
    // fill compulsory values of boot env structure
    recoverySel->signature=SNAP_BOOTSELECT_SIGNATURE_RECOVERY;
    recoverySel->version=SNAP_BOOTSELECT_VERSION_V2;
    return recoverySel;
}

void print_keys_value(const char *key, const char *value) {
    if (strlen(value)) {
      printf("%s [%s]\n", key, value);
    }
}

// print environment values
void print_runtime_environment(const SNAP_RUN_BOOT_SELECTION_t *bootSel) {
    printf("signature: [0x%X]\n", bootSel->signature);
    printf("version:   [0x%X]\n", bootSel->version);
    // only show populated values
    print_keys_value("kernel_status", bootSel->kernel_status);
    print_keys_value("snap_kernel", bootSel->snap_kernel);
    print_keys_value("snap_try_kernel", bootSel->snap_try_kernel);
    printf("bootimg_matrix [%s][%s]\n", bootSel->bootimg_matrix[0][0], bootSel->bootimg_matrix[0][1]);
    printf("bootimg_matrix [%s][%s]\n", bootSel->bootimg_matrix[1][0], bootSel->bootimg_matrix[1][1]);
    print_keys_value("bootimg_file_name", bootSel->bootimg_file_name);
    print_keys_value("gadget_mode", bootSel->gadget_mode);
    print_keys_value("snap_gadget", bootSel->snap_gadget);
    print_keys_value("snap_try_gadget", bootSel->snap_try_gadget);
    printf("gadget_asset_matrix [%s][%s]\n", bootSel->gadget_asset_matrix[0][0], bootSel->gadget_asset_matrix[0][1]);
    printf("gadget_asset_matrix [%s][%s]\n", bootSel->gadget_asset_matrix[1][0], bootSel->gadget_asset_matrix[1][1]);
    print_keys_value("Recovery boot partition 0", bootSel->unused_key_01);
    print_keys_value("Recovery boot partition 1", bootSel->unused_key_02);
}

// print environment values
void print_recovery_environment(const SNAP_RECOVERY_BOOT_SELECTION_t *recoverySel) {
    printf("signature: [0x%X]\n", recoverySel->signature);
    printf("version:   [0x%X]\n", recoverySel->version);
    // only show populated values
    print_keys_value("recovery_mode", recoverySel->snapd_recovery_mode);
    print_keys_value("recovery_system", recoverySel->snapd_recovery_system);
    printf("recovery_boot_matrix [%s][%s]\n", recoverySel->bootimg_matrix[0][0], recoverySel->bootimg_matrix[0][1]);
    printf("recovery_boot_matrix [%s][%s]\n", recoverySel->bootimg_matrix[1][0], recoverySel->bootimg_matrix[1][1]);
    print_keys_value("bootimg_file_name", recoverySel->bootimg_file_name);
}

void check_env_is_valid(void* env, int opt) {
    if (!env) {
        printf( "Option -%c is not supported by this env type\n", opt);
        exit(EXIT_FAILURE);
    }
}

int main (int argc, char **argv) {
    SNAP_RUN_BOOT_SELECTION_t *bootSel = NULL;
    SNAP_RECOVERY_BOOT_SELECTION_t *recoverySel = NULL;
    FILE *fp = NULL;
    int opt = 0;
    size_t bwriten = 0;
    enum Modes mode = unknown;
    static struct option long_options[] = {
        // long name      | has argument  | flag | short value
        {"runtime",         no_argument,       0, 'A'},
        {"recovery",        no_argument,       0, 'B'},
        {"write",           required_argument, 0, 'w'},
        {"read",            required_argument, 0, 'r'},
        {"update",          required_argument, 0, 'u'},
        {"help",            no_argument,       0, 'h'},
        {"kernel-status",   required_argument, 0, 'a'},
        {"snap-kernel",     required_argument, 0, 'b'},
        {"snap-try-kernel", required_argument, 0, 'c'},
        {"boot-0-part",     required_argument, 0, 'g'},
        {"boot-1-part",     required_argument, 0, 'i'},
        {"boot-0-snap",     required_argument, 0, 'j'},
        {"boot-1-snap",     required_argument, 0, 'k'},
        {"bootimg-file",    required_argument, 0, 'l'},
        {"gadget-0-part",   required_argument, 0, 'm'},
        {"gadget-1-part",   required_argument, 0, 'n'},
        {"gadget-0-snap",   required_argument, 0, 'o'},
        {"gadget-1-snap",   required_argument, 0, 'p'},
        {"gadget-mode",     required_argument, 0, 'q'},
        {"snap-gadget",     required_argument, 0, 's'},
        {"snap-try-gadget", required_argument, 0, 't'},
        {"revovery-mode",   required_argument, 0, 'd'},
        {"revovery-system", required_argument, 0, 'e'},
        {"recovery-0-part", required_argument, 0, 'v'},
        {"recovery-1-part", required_argument, 0, 'x'},
        {"recovery-0-snap", required_argument, 0, 'y'},
        {"recovery-1-snap", required_argument, 0, 'z'},
        {0, 0, 0, 0 }
    };

    int long_index =0;
    if (argc <= 1) {
        printf( "Missing compulsory arguments\n");
        print_usage();
        exit(EXIT_FAILURE);
    }
    while ((opt = getopt_long(argc, argv, "ABw:r:u:ha:b:c:d:e:f:g:i:j:k:l:m:n:o:p:q:s:t:v:x:y:z:",
                   long_options, &long_index )) != -1) {
        switch (opt) {
          case 'A':
              printf( "Handling runtime environment type\n");
              mode = runtime;
              break;
          case 'B':
              printf( "Handling recovery environment type\n");
              mode = recovery;
              break;
          case 'w' :
              printf( "Running in write mode [%d]\n\n", mode);
              fp = open_file( optarg, "w", "write");
              //  if mode is not defined, fail
              if (mode == unknown) {
                  printf("For write operation type of environment has to be defined\n");
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              // prepare empty boot_env structure
              if (mode == runtime) {
                  bootSel = create_clean_runtime_environment();
              } else {
                  recoverySel = create_clean_recovery_environment();
              }
              break;
          case 'r' :
              printf( "Running in read mode\n\n");
              fp = open_file( optarg, "r", "read");
              // try to determine type, check is done in read functions
              bootSel = read_validate_runtime_environment(fp, &mode);
              if (bootSel){
                  print_runtime_environment(bootSel);
                  free(bootSel);
              } else {
                  recoverySel = read_validate_recovery_environment(fp, &mode);
                  if (recoverySel) {
                      print_recovery_environment(recoverySel);
                      free(recoverySel);
                  } else {
                      // failed to determine type
                      printf("Failed to validate environment type\n");
                      exit(EXIT_FAILURE);
                  }
              }
              fclose(fp);
              exit(EXIT_SUCCESS);
              break;
          case 'u' :
              printf( "Running in updated mode\n\n");
              fp = open_file( optarg, "r", "update");
              // try to determine type, check is done in read functions
              bootSel = read_validate_runtime_environment(fp, &mode);
              if (!bootSel){
                  recoverySel = read_validate_recovery_environment(fp, &mode);
              }
              fclose(fp);
              if (mode == unknown || (!bootSel && !recoverySel)) {
                // failed to determine type
                printf("Failed to validate/determine environment type\n");
                exit(EXIT_FAILURE);
              }
              fp = open_file( optarg, "w", "update");
              break;
          case 'a' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->kernel_status, optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'b' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->snap_kernel, optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'c' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->snap_try_kernel, optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'g' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->bootimg_matrix[0][0], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'i' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->bootimg_matrix[1][0], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'j' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->bootimg_matrix[0][1], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'k' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->bootimg_matrix[1][1], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'l' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              if (mode == runtime) {
                  strncpy(bootSel->bootimg_file_name, optarg, SNAP_NAME_MAX_LEN);
              } else {
                  strncpy(recoverySel->bootimg_file_name, optarg, SNAP_NAME_MAX_LEN);
              }
              break;
          case 'm' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->gadget_asset_matrix[0][0], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'n' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->gadget_asset_matrix[1][0], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'o' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->gadget_asset_matrix[1][0], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'p' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->gadget_asset_matrix[1][1], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'q' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->gadget_mode, optarg, SNAP_NAME_MAX_LEN);
              break;
          case 's' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->snap_gadget, optarg, SNAP_NAME_MAX_LEN);
              break;
          case 't' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(bootSel, opt);
              strncpy(bootSel->snap_try_gadget, optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'd' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(recoverySel, opt);
              strncpy(recoverySel->snapd_recovery_mode, optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'e' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(recoverySel, opt);
              strncpy(recoverySel->snapd_recovery_system, optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'v' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(recoverySel, opt);
              strncpy(recoverySel->bootimg_matrix[0][0], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'x' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(recoverySel, opt);
              strncpy(recoverySel->bootimg_matrix[1][0], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'y' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(recoverySel, opt);
              strncpy(recoverySel->bootimg_matrix[0][1], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'z' :
              if(!optarg) {
                  printf( "Missing argument for %c option\n", opt);
                  print_usage();
                  exit(EXIT_FAILURE);
              }
              check_env_is_valid(recoverySel, opt);
              strncpy(recoverySel->bootimg_matrix[1][1], optarg, SNAP_NAME_MAX_LEN);
              break;
          case 'h' :
          default:
              print_usage();
              exit(EXIT_FAILURE);
        }
    }
    printf( "Done populating[%ld][%ld]\n",sizeof(SNAP_RUN_BOOT_SELECTION_t), sizeof(SNAP_RECOVERY_BOOT_SELECTION_t));
    if (bootSel) {
        printf( "Write bootSel\n");
        bootSel->crc32 = crc32(0x0, (void *)bootSel, sizeof(SNAP_RUN_BOOT_SELECTION_t)-sizeof(uint32_t));
        printf( "\nNew calculated crc32 is [0x%X]\n", bootSel->crc32);
        printf( "SIGNATURE [0x%X]\n", bootSel->signature);
        printf( "VERSION [0x%X]\n", bootSel->version);
        // save structure
        // bwriten = fwrite(bootSel, sizeof(SNAP_RUN_BOOT_SELECTION_t), 1, fp);
        bwriten = fwrite(bootSel, 1, sizeof(SNAP_RUN_BOOT_SELECTION_t), fp);
        printf( "Written %ld bytes\n", bwriten);
        print_runtime_environment(bootSel);
        free(bootSel);
    }
    if (recoverySel) {
        printf( "Write recovery sel\n");
        recoverySel->crc32 = crc32(0x0, (void *)recoverySel, sizeof(SNAP_RECOVERY_BOOT_SELECTION_t)-sizeof(uint32_t));
        printf( "\nNew calculated crc32 is [0x%X]\n", recoverySel->crc32);
        printf( "SIGNATURE [0x%X]\n", recoverySel->signature);
        printf( "VERSION [0x%X]\n", recoverySel->version);
        // save structure
        // bwriten = fwrite(bootSel, sizeof(SNAP_RUN_BOOT_SELECTION_t), 1, fp);
        bwriten = fwrite(recoverySel, 1, sizeof(SNAP_RECOVERY_BOOT_SELECTION_t), fp);
        printf( "Written %ld bytes\n", bwriten);
        print_recovery_environment(recoverySel);
        free(recoverySel);
    }
    fclose(fp);
    return 0;
}
