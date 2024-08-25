
#include "malloc.h"
#include "test.h"

#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <time.h>
#define REG_FILE                    8
#define REG_DIR                     4
#define RZIP_BUFFER_SIZE            4096 * 5 // met * 2 == 4.16
#define RZIP_TERMINAL_BUFFER_SIZE   4096 * 5
#define RZIP_TEST_SMALL_FILE_COUNT  100
#define RZIP_TEST_MEDIUM_FILE_COUNT 50
#define RZIP_TEST_BIG_FILE_COUNT    5
char   RZIP_EXECUTABLE[4096];
char * RZIP_CYPHER_STRING = "zip";
char   RZIP_CYPHER = 'R';
bool   RZIP_USE_CYPHER_STRING = false;
bool   RZIP_USE_CYPHER_KEY = true;
bool   RZIP_SILENT = false;
bool   RZIP_DEBUG = false;
char   rzip_term_buff[RZIP_TERMINAL_BUFFER_SIZE];
int    rzip_printf(const char * format, ...)
{
    if (RZIP_SILENT)
        return 0;
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stdout, format, args);
    if (format[strlen(format) - 1] != '\n') {
        fflush(stdout);
    }
    va_end(args);
    return ret;
}
int rzip_debf(const char * format, ...)
{
    if (!RZIP_DEBUG)
        return 0;
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stdout, format, args);
    if (format[strlen(format) - 1] != '\n') {
        fflush(stdout);
    }
    va_end(args);
    return ret;
}
char * rzip_format(const char * format, ...)
{
    static char res[8096];
    res[0] = '\0';
    va_list args;
    va_start(args, format);
    int ret = vsprintf(res, format, args);
    va_end(args);
    return res;
}
char * rzip_format_size(long long size)
{
    const char * suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    int          i = 0;
    double       dbl_size = size;
    while (dbl_size >= 1024 && i < (sizeof(suffixes) / sizeof(suffixes[0])) - 1) {
        dbl_size /= 1024;
        i++;
    }
    static char result[20];
    result[0] = '\0';
    sprintf(result, "%.2f %s", dbl_size, suffixes[i]);
    return result;
}
typedef enum RzipFileType
{
    FT_UNKNOWN = 0,
    FT_FILE = 8,
    FT_DIR = 4
} RzipFileType;
typedef struct RzipFile
{
    unsigned long int index;
    RzipFileType      type;
    char              path[4096];
    char              name[4096];
    off_t             start;
    off_t             size;
    unsigned long     mode;
} RzipFile;
typedef struct Rzip
{
    char        path[4096];
    RzipFile ** files;
    int         file_count;
} Rzip;
char * match_option(char * str, const char ** options)
{
    int index = 0;
    while (options[index]) {
        if (!strcmp(str, options[index]))
            return (char *)options[index];
        index++;
    }
    return NULL;
}
char *       rzip_lreplace_char(char * path, char chr, char new_char);
char *       rzip_rreplace_char(char * path, char chr, char new_char);
RzipFile *   rzip_add_file(Rzip * rzip, RzipFile * file);
unsigned int rzip_file_get_mode(char * path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return st.st_mode;
}
bool rzip_file_exists(char * path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    return false;
}
off_t rzip_file_size(char * path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return st.st_size;
}
Rzip * rzip_create()
{
    Rzip * rzip = rzip_malloc(sizeof(Rzip));
    rzip->files = NULL;
    rzip->file_count = 0;
    return rzip;
}

void rzip_destruct(Rzip * rzip)
{
    // according rzip_malloc this has to be quoted. Else allocated in the end is a negative value
    // valgrind doesn't care, same values.
    // code doesn't crash!
    // for (int i = 0; i < rzip->file_count; i++) {
    //     rzip_free(rzip->files[i]);
    // }
    rzip_free(rzip->files);
    rzip_free(rzip);
    // Must ?
    rzip = NULL;
}
char * rzip_get_path_part(char * path, int path_index)
{
    int    index = 0;
    char * part;
    char   clean_path[4096];
    clean_path[0] = '\0';
    strcpy(clean_path, path);
    rzip_rreplace_char(clean_path, '/', '\0');
    static char result[4096];
    memset(result, 0, sizeof(result));
    while ((part = strtok(index == 0 ? clean_path : NULL, "/")) != NULL) {
        if (index == path_index) {
            strcpy(result, part);
            return result;
        } else {
            strcpy(result, part);
        }
        index++;
    }
    if (path_index == -1)
        return result;
    return NULL;
}
RzipFile * rzip_add_file(Rzip * rzip, RzipFile * file)
{
    if (rzip->files == NULL) {
        rzip->files = rzip_malloc(sizeof(RzipFile) * 1);
    } else {
        rzip->files = rzip_realloc(rzip->files, (rzip->file_count + 1) * sizeof(RzipFile) * 2);
    }

    rzip->files[rzip->file_count] = (RzipFile *)malloc(sizeof(RzipFile));
    file->index = rzip->file_count;

    if (rzip->file_count == 0) {
        file->start = (rzip->file_count + 1) * sizeof(RzipFile);
    } else {
        file->start = rzip->files[rzip->file_count - 1]->start + rzip->files[rzip->file_count - 1]->size + sizeof(RzipFile);
    }

    memcpy(rzip->files[rzip->file_count], file, sizeof(RzipFile));
    rzip->file_count++;
    return rzip->files[rzip->file_count - 1];
}
char * rzip_rreplace_char(char * path, char chr, char new_char)
{
    while (strlen(path) > 1 && path[strlen(path) - 1] == chr) {
        path[strlen(path) - 1] = new_char;
    }
    return path;
}
char * rzip_shift_char(char * path, char chr)
{
    while (path[0] == chr) {
        // +1 because we want to take the \0 with us
        for (int j = 0; j < strlen(path) + 1; j++) {
            path[j] = path[j + 1];
        }
    }
    return path;
}
__uint8_t rzip_cypher_string(__uint8_t inp, char * cypher_string)
{
    int len = strlen(cypher_string);
    for (int i = 0; i < len; i++) {
        inp = inp ^ cypher_string[i];
    }
    return inp;
}
__uint8_t rzip_rot255(__uint8_t inp)
{
    if (RZIP_USE_CYPHER_STRING) {
        return rzip_cypher_string(inp, RZIP_CYPHER_STRING);
    } else if (RZIP_USE_CYPHER_KEY) {
        return inp ^ RZIP_CYPHER;
    }
    return inp;
}
void rzip_cypher_bytes(char * bytes, size_t len)
{
    for (int i = 0; i < len; i++) {
        bytes[i] = rzip_rot255(bytes[i]);
    }
}
char * rzip_localize_path(char * path)
{
    static char result[4096];
    result[0] = '\0';
    strcpy(result, path);
    if (path[0] == '/') {
        rzip_shift_char(result, '/');
    }
    return result;
}
char * rzip_normalize_path(char * path)
{
    static char result[4096];
    result[0] = '\0';
    strcpy(result, path);
    if ((result[0] == '.')) {
        rzip_shift_char(result, '.');
    }
    return result;
}
RzipFile * rzip_source_to_file(char * path, RzipFileType type)
{
    RzipFile * file = (RzipFile *)rzip_malloc(sizeof(RzipFile));
    file->mode = rzip_file_get_mode(path);
    file->type = type;
    if (file->type == FT_FILE)
        file->size = rzip_file_size(path);
    else
        file->size = 0;
    char * path_memory = strdup(path); // For if given path is a const char *
    strcpy(file->path, rzip_rreplace_char(path_memory, '/', '\0'));
    rzip_free(path_memory);
    char * name = rzip_get_path_part(file->path, -1);
    strcpy(file->name, name);
    return file;
}
int rzip_is_valid_name(char * name)
{
    if (name == NULL)
        return 0;
    if (!strcmp(name, "."))
        return 0;
    if (!strcmp(name, ".."))
        return 0;
    if (!strcmp(name, "/"))
        return 0;
    if (!strcmp(name, ""))
        return 0;
    return 1;
}
void * td(char * content, int length)
{
    static char data[4096];
    memset(data, 0, length);
    memcpy(data, content, length);
    return data;
}
char * ts(char * content)
{
    char * result = (char *)td(content, strlen(content));
    result[strlen(content)] = '\0';
    return result;
}
char * rzip_path_join(char * path1, char * path2)
{
    char path1_copy[4096] = {0};
    char path2_copy[4096] = {0};
    strcpy(path1_copy, path1);
    strcpy(path2_copy, path2);
    // rzip_shift_char(path1_copy, '/');
    rzip_shift_char(path2_copy, '/');
    rzip_rreplace_char(path1_copy, '/', '\0');
    rzip_rreplace_char(path2_copy, '/', '\0');
    static char result[4096] = {0};
    result[4096] = 0;
    strcpy(result, path1_copy);
    strcat(result, "/");
    strcat(result, path2_copy);
    return strdup(result);
}
RzipFileType rzip_get_type(char * path)
{
    struct stat s;
    stat(path, &s);
    if (S_ISLNK(s.st_mode))
        return FT_UNKNOWN;
    if (S_ISDIR(s.st_mode))
        return FT_DIR;
    return FT_FILE;
}
Rzip * rzip_add(Rzip * rzip, char * path)
{

    char clean_path[4096];
    clean_path[0] = 0;
    strcpy(clean_path, rzip_normalize_path(path));

    if (!rzip_file_exists(clean_path)) {
        rzip_printf("Path does not exist: %s\n", clean_path);
        return rzip;
    }
    if (rzip_get_type(clean_path) == FT_DIR) {

        DIR *           dir;
        struct dirent * dirEnt;

        dir = opendir(clean_path);

        if (dir == NULL) {

            printf("PATH NULL:%s\n", clean_path);
            return rzip;
        }

        while ((dirEnt = readdir(dir)) != NULL) {
            if (!rzip_is_valid_name(dirEnt->d_name)) {
                continue;
            }
            char * item_path = rzip_path_join(clean_path, dirEnt->d_name);

            RzipFileType ft = FT_UNKNOWN;
            if (dirEnt->d_type == REG_DIR)
                ft = FT_DIR;
            else if (dirEnt->d_type == REG_FILE) {
                ft = FT_FILE;
            }
            if (ft == FT_UNKNOWN) {
                // Symlinks come here. We dont want them
                rzip_printf("Skipping unknown file type: %s\n", item_path);
                continue;
            }
            RzipFile * f = rzip_source_to_file(item_path, ft);
            rzip_add_file(rzip, f);

            f->type = dirEnt->d_type == REG_DIR ? FT_DIR : FT_FILE;
            if (dirEnt->d_type == FT_DIR) {
                if (f->type == FT_DIR) {
                    char * new_path = rzip_path_join(clean_path, dirEnt->d_name);
                    rzip_add(rzip, new_path);
                }
            } else {
                // rzip_printf("%5ld.  bytes(%lu) %s \n", f->index, rzip_file_size(item_path), item_path);
            }
        }
        closedir(dir);
    } else if (rzip_get_type(clean_path) == FT_FILE) {
        RzipFile * f = rzip_source_to_file(clean_path, FT_FILE);
        rzip_add_file(rzip, f);
        f->type = FT_FILE;
    }
    return rzip;
}
void rzip_mkdirs(char * path, unsigned long mode)
{
    char   complete_path[4096] = {0};
    char * sub;
    int    i = 0;
    // Bit extreme, but for safety removing first /'s here
    char * clean_path = rzip_strdup(path);
    rzip_shift_char(clean_path, '/');
    while ((sub = rzip_get_path_part(clean_path, i)) != NULL) {
        if (i > 0) {
            strcpy(complete_path, rzip_path_join(complete_path, sub));
        } else {
            strcpy(complete_path, sub);
        }
        if (!rzip_file_exists(complete_path)) {
            rzip_printf("Dir create: %s\n", complete_path);
            int err = mkdir(complete_path, mode);
            if (err == EEXIST) {
                // We don't care
            } else if (err != -1) {
                // False positive does not exist error
                // rzip_printf("Dir create failed: %s\n", complete_path);
                /// perror(err);
            }
        }
        i++;
    }
    rzip_free(clean_path);
}
void rzip_skip_file(FILE * f, RzipFile * source) { fseek(f, source->start + source->size, SEEK_SET); }
void rzip_include_file(FILE * dest, RzipFile * source, int rot255)
{
    size_t buffer_size = RZIP_BUFFER_SIZE;
    char   buffer[buffer_size];
    FILE * f = fopen(source->path, "rb");
    size_t len = 0;
    size_t total_len = 0;
    size_t buff_size = source->size > buffer_size ? buffer_size : source->size;
    while ((len = fread(buffer, 1, buff_size, f)) > 0) {
        total_len += len;
        if (rot255) {
            for (int i = 0; i < len; i++)
                buffer[i] = rzip_rot255(buffer[i]);
        }
        fwrite(buffer, len, sizeof(unsigned char), dest);
        buff_size = buffer_size + total_len > source->size ? source->size - total_len : buffer_size;
    }
    source->size = total_len;
    fclose(f);
}
Rzip * rzip_load(char * path)
{
    if (!rzip_file_exists(path)) {
        rzip_printf("Error loading rzip, %s does not exist.\n", path);
        return NULL;
    }
    rzip_printf("Reading %s\n", path);
    Rzip * rzip = rzip_create();
    FILE * f = fopen(path, "rb");
    strcpy(rzip->path, path);
    char buffer[sizeof(RzipFile)];
    int  len;
    while (len = fread(buffer, sizeof(RzipFile), sizeof(unsigned char), f) > 0) {
        rzip_cypher_bytes(buffer, sizeof(RzipFile));
        RzipFile * rfile = (RzipFile *)buffer;
        if (rfile->type == FT_FILE && rfile->size) {
            rzip_skip_file(f, rfile);
        }
        rzip_add_file(rzip, rfile);
    }
    fclose(f);
    return rzip;
}
void rzip_raw(Rzip * rzip, FILE * fout2)
{
    FILE * fout = stdout;
    for (int i = 0; i < rzip->file_count; i++) {
        RzipFile * file = rzip->files[i];
        rzip_printf("Archiving %s (%s)\n", file->path, rzip_format_size(file->size));
        unsigned char * bfile = (unsigned char *)file;
        char            rotted[sizeof(RzipFile)];
        for (int i = 0; i < sizeof(RzipFile); i++) {
            char c = bfile[i];
            rotted[i] = rzip_rot255(c);
        }
        fwrite(rotted, sizeof(rotted), sizeof(unsigned char), fout2);
        if (file->type == FT_FILE) {
            rzip_include_file(fout2, file, 1);
        }
    }
}
char * rzip_strip_filename(char * path)
{
    static char result[4096];
    result[0] = '\0';
    strcpy(result, rzip_normalize_path(path));
    char * file_name = rzip_get_path_part(path, -1);
    result[strlen(result) - strlen(file_name)] = '\0';
    return result;
}
Rzip * rzip_save(Rzip * rzip, char * path)
{
    FILE * fd;
    strcpy(rzip->path, path);
    if (!strcmp("path", ":stdout:")) {
        fd = stdout;
    } else {
        fd = fopen(path, "wb+");
    }
    rzip_raw(rzip, fd);
    fclose(fd);
    return rzip;
}
/*
Rzip * rzip_dump(Rzip * rzip){
    rzip_raw(rzip,"stdout");
}*/
void rzip_extract_file(FILE * fd, RzipFile * rfile, char * to)
{
    char path_destiny[4096] = {0};
    strcpy(path_destiny, rzip_path_join(to, rfile->path));
    FILE * fdd = fopen(path_destiny, "wb+");
    fseek(fd, rfile->start, SEEK_SET);
    size_t buffer_size = RZIP_BUFFER_SIZE;
    char   buffer[buffer_size];
    size_t buff_size = rfile->size < buffer_size ? rfile->size : buffer_size;
    size_t bytes_read = 0;
    size_t bytes_total_read = 0;
    size_t len = 0;
    size_t len_total = 0;
    while ((len = fread(buffer, 1, buff_size, fd)) > 0) {
        for (int i = 0; i < len; i++) {
            buffer[i] = rzip_rot255(buffer[i]);
        }
        fwrite(buffer, len, sizeof(unsigned char), fdd);
        len_total += len;
        buff_size = len_total + buffer_size > rfile->size ? rfile->size - len_total : buffer_size;
    }
    chmod(path_destiny, (mode_t)rfile->mode);
    fclose(fdd);
}
void rzip_list_fast(char * path)
{
    size_t     buffer_size = sizeof(RzipFile);
    size_t     len = 0;
    char       buffer[buffer_size];
    FILE *     f = fopen(path, "rb");
    RzipFile * rfile;
    while ((len = fread(buffer, sizeof(RzipFile), sizeof(unsigned char), f)) > 0) {
        rzip_cypher_bytes(buffer, sizeof(RzipFile));
        rfile = (RzipFile *)buffer;
        // memcpy(&rfile,buffer,buffer_size);
        if (rfile->type == FT_DIR) {
            rzip_printf("%5d. (dir) %s\n", rfile->index + 1, rfile->path);
        } else if (rfile->type == FT_FILE) {
            rzip_printf("%5d. (file) %s (%s)\n", rfile->index + 1, rfile->path, rzip_format_size(rfile->size));
        } else {
            rzip_printf("%d. (unknown): %s\n", rfile->index + 1, rfile->path);
        }
        rzip_skip_file(f, rfile);
    }
    fclose(f);
}
void rzip_extract(Rzip * rzip, char * to)
{
    rzip_mkdirs(to, 0700);
    FILE * fd = fopen(rzip->path, "rb");
    for (long i = 0; i < rzip->file_count; i++) {
        RzipFile * rfile = rzip->files[i];
        char *     original_path = rfile->path;
        char       destination_path[4096];
        destination_path[0] = '\0';
        strcpy(destination_path, original_path);
        strcpy(destination_path, rzip_path_join(to, rzip_localize_path(destination_path)));
        if (rfile->type == FT_DIR) {
            rzip_printf("Extract dir: %s\n", destination_path);
            rzip_mkdirs(destination_path, rfile->mode);
        } else if (rfile->type == FT_FILE) {
            rzip_printf("Extract %s: %s\n", rzip_format_size(rfile->size), destination_path);
            rzip_mkdirs(rzip_strip_filename(destination_path), 0700);
            rzip_extract_file(fd, rfile, to);
        } else {
            rzip_printf("Unknown: %s\n", rfile->path);
        }
    }
    fclose(fd);
}
void rzip_test_create_file(char * path, size_t size)
{
    rzip_mkdirs(rzip_strip_filename(path), 0700);
    FILE * fd = fopen(path, "wb+");
    size_t i = 0;
    char   buffer[size];
    char   j = 0;
    for (int i = 0; i < size; i++) {
        buffer[i] = (char)(j ^ 255);
        if (j == 254)
            j = 0;
        j++;
    }
    fwrite(buffer, sizeof(buffer), sizeof(unsigned char), fd);
    fclose(fd);
}
bool rzip_unlink_dir(char * directory)
{
    DIR *           d;
    struct dirent * f;
    d = opendir(directory);
    size_t sum = 0;
    while ((f = readdir(d)) != NULL) {
        if (!rzip_is_valid_name(f->d_name))
            continue;
        if (f->d_type == REG_FILE) {
            if (unlink(rzip_path_join(directory, f->d_name)) == -1) {
                return false;
            }
        } else if (f->d_type == REG_DIR) {
            if (!rzip_unlink_dir(rzip_path_join(directory, f->d_name))) {
                return false;
            }
        }
    }
    closedir(d);
    return rmdir(directory) == 0;
}
char * rzip_test_create_testdata(char * path)
{
    if (rzip_file_exists(path)) {
        rzip_debf("Removing old testing directory: %s\n", path);

        if (!rzip_unlink_dir(path)) {
            perror("Couldn't delete old testing directory.\n");
            return NULL;
        } else {

            rzip_debf("Remved old testing directory: %s\n", path);
        }
    }
    size_t kb = 1024;
    size_t mb = kb * 1024;
    rzip_debf("Creating %d 1kb files in %s\n", RZIP_TEST_SMALL_FILE_COUNT, rzip_path_join(path, rzip_format("/1kb")));
    for (int i = 0; i < RZIP_TEST_SMALL_FILE_COUNT; i++) {
        rzip_test_create_file(rzip_path_join(path, rzip_format("/1kb/file_1kb_%d_%s.dat", i, rzip_format_size(kb))), kb);
    }
    rzip_debf("Creating %d 1kb - 25kb files in %s\n", RZIP_TEST_MEDIUM_FILE_COUNT, rzip_path_join(path, rzip_format("/nkb")));
    for (int i = 0; i < RZIP_TEST_MEDIUM_FILE_COUNT; i++) {
        rzip_test_create_file(rzip_path_join(path, rzip_format("/nkb/file_kb_%d_%s.dat", i, rzip_format_size(kb * i))), kb * i);
    }
    rzip_debf("Creating %d 1mb files in %s\n", RZIP_TEST_BIG_FILE_COUNT, rzip_path_join(path, rzip_format("/nmb")));
    for (int i = 0; i < RZIP_TEST_BIG_FILE_COUNT; i++) {
        rzip_test_create_file(rzip_path_join(path, rzip_format("/nmb/file_%d_%s.dat", i, rzip_format_size(mb * i))), mb * i);
    }
    return path;
}
bool rzip_zip_dir_to_file(char * path, char * source)
{
    Rzip * rzip_dest = rzip_create();
    rzip_add(rzip_dest, source);
    Rzip * rzip_saved = rzip_save(rzip_dest, path);
    rzip_destruct(rzip_saved);
    return true;
}
bool rzip_unzip_file_to_dir(char * fzip_path, char * destination_path)
{
    Rzip * rzip_source = rzip_load(fzip_path);
    rzip_extract(rzip_source, destination_path);
    rzip_destruct(rzip_source);
    return true;
}
size_t rzip_sum_directory_size(char * directory)
{
    DIR *           d;
    struct dirent * f;
    d = opendir(directory);
    if (d == NULL) {
        return 0;
    }
    size_t sum = 0;
    while ((f = readdir(d)) != NULL) {
        if (!rzip_is_valid_name(f->d_name))
            continue;
        if (f->d_type == REG_FILE) {
            sum += rzip_file_size(rzip_path_join(directory, f->d_name));
        } else if (f->d_type == REG_DIR) {
            sum += rzip_sum_directory_size(rzip_path_join(directory, f->d_name));
        }
    }
    closedir(d);
    return sum;
}
bool rzip_integration_test()
{
    test_banner("Integration tests");
    char * testdata_path = "testdata";
    char * testdata_target_path = "testdata/testdata";
    char * result = rzip_test_create_testdata(testdata_path);
    if (!result) {
        perror("Couldn't create testdata.\n");
        return false;
    }
    char * testdata_rzip = "testdata.rzip";
    test_banner("Creating and zipping 75 test files");

    rzip_zip_dir_to_file(testdata_rzip, testdata_path);

    // return true;
    test_banner("Unzipping 75 test files");

    rzip_unzip_file_to_dir(testdata_rzip, testdata_target_path);

    size_t total_directory_size = rzip_sum_directory_size(testdata_path);
    size_t new_directory_size = rzip_sum_directory_size(testdata_target_path);

    test_true(new_directory_size == total_directory_size / 2);

    return true;
}
int rzip_test()
{
    test_banner("Path normalizing tests");
    test_string(rzip_shift_char(ts("//abc//"), '/'), "abc//");
    test_string(rzip_shift_char(ts("\\abc//"), '/'), "\\abc//");
    test_string(rzip_shift_char(ts("abc//"), '/'), "abc//");
    test_banner("Path split tests");
    char * file_path = ts("part1/part2/part3/part4/part5.txt");
    test_string(rzip_get_path_part(file_path, 0), "part1");
    test_string(rzip_get_path_part(file_path, 1), "part2");
    test_string(rzip_get_path_part(file_path, 2), "part3");
    test_string(rzip_get_path_part(file_path, 3), "part4");
    test_string(rzip_get_path_part(file_path, 4), "part5.txt");
    test_string(rzip_get_path_part(file_path, 7), NULL);         // out of range
    test_string(rzip_get_path_part(file_path, -1), "part5.txt"); // -1 == last file path
    test_banner("Path join tests");
    test_string(rzip_path_join("abc", "def"), "abc/def");
    test_string(rzip_path_join("abc/", "def"), "abc/def");
    test_string(rzip_path_join("abc/", "def/"), "abc/def");
    test_string(rzip_path_join("abc", "/def"), "abc/def");
    test_string(rzip_path_join("abc/", "/def"), "abc/def");
    test_string(rzip_path_join("abc/", "def"), "abc/def");
    test_string(rzip_path_join("/abc/", "def/"), "/abc/def");
    test_banner("Path name validation");
    test_false(rzip_is_valid_name(".."));
    test_false(rzip_is_valid_name("/"));
    test_false(rzip_is_valid_name(""));
    test_false(rzip_is_valid_name(NULL));
    test_banner("cyphers");
    test_true(rzip_rot255(rzip_rot255(254)) == 254);
    test_true(rzip_rot255(rzip_rot255(200)) == 200);
    test_true(rzip_rot255(rzip_rot255(0)) == 0);
    test_true(rzip_rot255(rzip_rot255(1)) == 1);
    test_banner("get file mode");
    test_true(rzip_file_get_mode("/") > 0);
    test_false(rzip_file_get_mode("/does-not-exist"));
    test_banner("file exists");
    test_false(rzip_file_exists("/does-not-exist"));
    test_true(rzip_file_exists("/tmp"));
    test_banner("file size");
    test_false(rzip_file_size("/does-not-exisst"));
    test_true(rzip_file_size("/tmp") > 0); // directories always have 4096b size
    return 0;
}
int rzip_print_help()
{
    rzip_printf("Rzip 0.9\n");
    rzip_printf("Usage:\n");
    rzip_printf("  zip [dest] [files]\n");
    rzip_printf("    dest  : Destination zip file. E.g. backup.rzip\n");
    rzip_printf("    files : List of files to compress. E.g. * for all recursive\n");
    rzip_printf("\n");
    rzip_printf("  unzip [source] [destination]\n");
    rzip_printf("    source      : Source zip file to extract\n");
    rzip_printf("    destination : Destination directory to extract files\n");
    rzip_printf("\n");
    rzip_printf("  test");
    rzip_printf("    tests will be ran by every execution to ensure safity\n");
    rzip_printf("\n");
    rzip_printf("Examples:\n");
    rzip_printf("  zip archive.zip file1.txt file2.txt\n");
    rzip_printf("  unzip archive.zip extracted_files/\n");
    return rzip_test();
}
typedef enum rzip_command_t
{
    RZIP_COMMAND_NONE = 0,
    RZIP_COMMAND_VIEW,
    RZIP_COMMAND_LIST,
    RZIP_COMMAND_ZIP,
    RZIP_COMMAND_UNZIP,
    RZIP_COMMAND_TEST,
    RZIP_COMMAND_STATS,
    RZIP_COMMAND_HELP
} rzip_command_t;
const char * rzip_options_list[] = {"list", "--list", "-l", "l", NULL};
const char * rzip_options_zip[] = {"zip", "z", "--zip", "-z", "z", NULL};
const char * rzip_options_unzip[] = {"unzip", "--unzip", "-u", "u", NULL};
const char * rzip_options_test[] = {"test", "--test", "-t", "t", NULL};
const char * rzip_options_view[] = {"view", "--view", "-v", "v", NULL};
const char * rzip_options_silent[] = {"silent", "s", "-s", "--silent", NULL};
const char * rzip_options_no_cypher_key[] = {"-nck", "--no-cypher-key", NULL};
const char * rzip_options_yes_cypher_string[] = {"-cs", "--cypher-string", NULL};
const char * rzip_options_debug[] = {"debug", "-d", "--debug", NULL};
const char * rzip_options_help[] = {"help", "-h", "--help", NULL};
typedef struct rzip_arguments_t
{
    char *         executable;
    char *         arg;
    rzip_command_t command;
    char **        argv;
    char *         regex;
    bool *         stats;
    bool           silent;
    bool           use_cypher_key;
    bool           use_cypher_string;
    bool           debug;
    int            argc;
} rzip_arguments_t;
char * rzip_format_bool(bool val) { return val ? "True" : "False"; }
char * rzip_format_string(char * val) { return val ? val : "NULL"; }
char * rzip_format_int(int val)
{
    static char res[20];
    res[0] = '\0';
    sprintf(res, "%d", val);
    return res;
}
char * rzip_arguments_to_string(rzip_arguments_t * args)
{
    static char res[4096];
    res[0] = '\0';
    char * format = " @%s: %s\n";
    strcat(res, rzip_format(format, "executable", rzip_format_string(args->executable)));
    strcat(res, rzip_format(format, "arg", rzip_format_string(args->executable)));
    strcat(res, rzip_format(format, "command", rzip_format_int(args->command)));
    strcat(res, rzip_format(format, "silent", rzip_format_bool(args->silent)));
    strcat(res, rzip_format(format, "use_cypher_key", rzip_format_bool(args->use_cypher_key)));
    strcat(res, rzip_format(format, "use_cypher_string", rzip_format_bool(args->use_cypher_string)));
    strcat(res, " @vargs:\n");
    for (int i = 0; i < args->argc; i++) {
        strcat(res, rzip_format(" - varg%2d: %s\n", i, args->argv[i]));
    }
    return res;
}
bool rzip_argument_isset(rzip_arguments_t * args, const char ** flags)
{
    for (int i = 0; i < args->argc; i++) {
        if (match_option(args->argv[i], flags))
            return true;
    }
    return false;
}
rzip_arguments_t * rzip_parse_arguments(int argc, char * argv[])
{
    static rzip_arguments_t args;
    args.executable = argv[0];
    args.arg = argv[1];
    args.argc = argc;
    args.argv = argv;
    args.debug = rzip_argument_isset(&args, rzip_options_debug);
    args.silent = rzip_argument_isset(&args, rzip_options_silent);
    args.use_cypher_key = !rzip_argument_isset(&args, rzip_options_no_cypher_key);
    args.use_cypher_string = rzip_argument_isset(&args, rzip_options_yes_cypher_string);
    if (args.use_cypher_string)
        args.use_cypher_key = false;
    if (match_option(args.arg, rzip_options_view)) {
        args.command = RZIP_COMMAND_VIEW;
    } else if (match_option(args.arg, rzip_options_help)) {
        args.command = RZIP_COMMAND_HELP;
    } else if (match_option(args.arg, rzip_options_zip)) {
        if (argc < 4) {
            perror("Incorrect parameter(s) for zip.\n");
            return NULL;
        }
        args.command = RZIP_COMMAND_ZIP;
    } else if (match_option(args.arg, rzip_options_unzip)) {
        if (argc < 4) {
            perror("Incorrect parameter(s) for unzip.\n");
            return NULL;
        }
        if (!rzip_file_exists(args.argv[2])) {
            rzip_printf("File to unzip doesn't exist: %s\n", args.argv[2]);
            return NULL;
        }
        args.command = RZIP_COMMAND_UNZIP;
    } else if (match_option(args.arg, rzip_options_test)) {
        args.command = RZIP_COMMAND_TEST;
    } else if (match_option(args.arg, rzip_options_view)) {
        args.command = RZIP_COMMAND_VIEW;
    } else if (match_option(args.arg, rzip_options_list)) {
        args.command = RZIP_COMMAND_LIST;
    }
    RZIP_SILENT = args.silent;
    RZIP_USE_CYPHER_KEY = args.use_cypher_key;
    RZIP_USE_CYPHER_STRING = args.use_cypher_string;
    RZIP_DEBUG = args.debug;
    char    self_path[4096];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    strcpy(RZIP_EXECUTABLE, self_path);
    return &args;
}
void rzip_optimize_terminal()
{
    // Adjustable with RZiP_TERMINAl_BUFFER_SiZE
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdout, rzip_term_buff, _IOFBF, sizeof(rzip_term_buff));
}
int main(int argc, char * argv[])
{
    rzip_optimize_terminal();
    clock_t            c_start = clock();
    rzip_arguments_t * argsp = rzip_parse_arguments(argc, argv);
    if (argsp == NULL)
        return 0;
    rzip_arguments_t args = *argsp;
    switch (args.command) {
    case RZIP_COMMAND_TEST:
        rzip_integration_test();
        rzip_debf("%s", rzip_arguments_to_string(&args));
        if (rzip_test())
            return 1;
        return test_end("");
        break;
    case RZIP_COMMAND_ZIP:
        char * backup_path = args.argv[2];

        Rzip * rzip_dest = rzip_create();

        for (int i = 3; i < argc; i++) {
            rzip_add(rzip_dest, args.argv[i]);
        }
        Rzip * rzip_saved = rzip_save(rzip_dest, args.argv[2]);
        rzip_printf("Archived in %s\n", args.argv[2]);
        rzip_destruct(rzip_saved);

        break;
    case RZIP_COMMAND_UNZIP:
        char * source_path = args.argv[2];
        char * destination_path = args.argv[3];
        rzip_unzip_file_to_dir(source_path, destination_path);
        rzip_printf("Archive extracted in %s\n", destination_path);
        break;
    case RZIP_COMMAND_LIST:
    case RZIP_COMMAND_VIEW:
        char * view_source_path = args.argv[2];
        if (!rzip_file_exists(view_source_path)) {
            rzip_printf("File not found: %s\n", view_source_path);
            return 1;
        }
        rzip_list_fast(view_source_path);
        break;
    case RZIP_COMMAND_HELP:
        rzip_printf("%s", rzip_arguments_to_string(&args));
        return rzip_print_help();
    default:
        rzip_printf("ERROR: Unknown operation %s.\n", args.arg);
        return rzip_print_help();
    }
    rzip_debf(" @duration: %f\n", (double)(clock() - c_start) / CLOCKS_PER_SEC);
    if (args.debug) {
        rzip_debf("%s", rzip_arguments_to_string(&args));
        return test_end("");
    }
    return 0;
}
