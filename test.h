#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "malloc.h"
#include <string.h>
#define debug(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__);

char * current_banner;
int assert_count = 0;
unsigned short test_is_first = 1;
unsigned int test_fail_count = 0;

int test_end(char * content){
    printf("\n @assertions: %d\n",assert_count);
    printf(" @memory: %s\n", rzip_malloc_stats());
    if(rzip_malloc_count != 0){
        printf("MEMORY ERROR\n");
        return test_fail_count > 0;
    }
    return test_fail_count > 0;
}

int test_test_banner(char * content, char * file, int line){
    if(test_is_first == 1){
        printf("%s tests", file);
        test_is_first = 0;
        return 1;
    }
    printf("\n - %s ",content);
}


void test_test_true(char * expr, int res, int line){
    assert_count++;
    if(res){
        printf(".");
    }else{
        printf("\nERROR on line %d: %s",line, expr);
        test_fail_count++;
    }
    fflush(stdout);
}
void test_test_false(char * expr, int res, int line){
    test_test_true(expr, !res, line);
}
void test_test_string(char * expr, char * str1, char * str2,int line){
      assert_count++;
    if(str1 != NULL && str2 != NULL && !strcmp(str1,str2)){
        printf(".");
    }else if(str1 == NULL && str2 == NULL) {
        printf(".");
        
        }else{
        printf("\nERROR on line %d: %s (\"%s\"!=\"%s\"\n",line, expr,str1 == NULL ? "(null)" : str1,str2 == NULL ? "(null)":str2);
        
        test_fail_count++;
    }
    fflush(stdout);
}
void test_test_skip(char * expr, int line){
    printf("\n @skip(%s) on line %d\n",expr, line);
}

#define test_banner(content) current_banner=content; test_test_banner(content,__FILE__,__LINE__);
#define test_end2(content) current_banner=content; test_test_end();
#define test_ok(content) assert_count++; current_banner=content; printf(" [OK]\t(%d)\t%s:%d\t%s\n", assert_count, __FILE__, __LINE__, content);
#define test_assert test_ok(current_banner) assert
#define test_true(expr) test_test_true(#expr, expr,__LINE__);
#define test_string(expr,value) test_test_string(#expr, expr, value,__LINE__);
#define test_skip(expr) test_test_skip(#expr, __LINE__);
#define test_false(expr) test_test_false(#expr, expr,__LINE__);

FILE * test_create_file(char * path, char * content){
    FILE * fd = fopen(path,"wb");
    
    char c;
    int index = 0;
    
    while((c = content[index]) != 0){
        fputc(c,fd);
        index++;
    }   
    fclose(fd);
    fd = fopen(path, "rb");
    return fd;
}


void test_delete_file(char * path){
    unlink(path);
}
