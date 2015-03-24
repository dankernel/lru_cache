/**
 * =====================================================================================
 *
 *          @file:  main.c
 *         @brief:  Main file
 *
 *          @date:  2014년 10월 30일 19시 48분 57초
 *        @author:  Jun-Hyung Park (), google@dankook.ac.kr
 *        Version:  2.0
 *
 *       Revision:  none
 *       Compiler:  gcc
 *   organization:  Dankook Univ.
 *    Description:  :
 *
 * =====================================================================================
 */

#include <stdio.h>
#include "./dkh/lru.c"

/**
 * Main function
 * @return error code
 */
int main(int argc, char *argv[]){

  FILE *fp;

  /* Set workload file */
  fp = open_workload(argv[1]);
  if (!fp)
    printf("FAIL open\n");

  /* Read MAIN function */
  read_workload(fp, atol(argv[2]) * 1024 * 1024);

end:
  fclose(fp);
  return 0;

}
