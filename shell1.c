/*
*  Course: System Programming with Linux
*  Shell 1
*  main() displays a prompt, receives a string from keyboard, pass it to tokenize()
*  tokenize() allocates dynamic memory and tokenize the string and return a char**
*  main() then pass the tokenized string to execute() which calls fork and exec
*  finally main() again displays the prompt and waits for next command string
*   Limitations:
*   if user press enter without any input the program gives sigsegv 
*   if user give only spaces and press enter it gives sigsegv
*   if user press ctrl+D it give sigsegv
*   however if you give spaces and give a cmd and press enter it works
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LEN 512    //sets limit for commad input length
#define MAXARGS 10     // maximum arguments
#define ARGLEN 30      // arguments length
#define PROMPT "NewShell:- "



int execute(char* arglist[]);  
char** tokenize(char* cmdline);
char* read_cmd(char*, FILE*);
int main(){
   char *cmdline;
   char** arglist;
   char* prompt = PROMPT;   
   // infinte loop
   while((cmdline = read_cmd(prompt,stdin)) != NULL){  // reads input from stdin and returns it
      if((arglist = tokenize(cmdline)) != NULL){  // tokenizing the command
            execute(arglist);  // function that executes the command
       //  need to free arglist
         for(int j=0; j < MAXARGS+1; j++)
	         free(arglist[j]);
         free(arglist);
         free(cmdline);
      }
  }//end of while loop
   printf("\n");
   return 0;
}
int execute(char* arglist[]){  
   int status;
   int cpid = fork(); // use of fork to create a child
   switch(cpid){
      case -1:
         perror("fork failed");
	      exit(1);
      case 0:
	      execvp(arglist[0], arglist); // child runs the command using execvp
 	      perror("Command not found...");
	      exit(1);
      default:
	      waitpid(cpid, &status, 0);
         printf("child exited with status %d \n", status >> 8);
         return 0;
   }
}
char** tokenize(char* cmdline){
//allocate memory to store command arguments
   char** arglist = (char**)malloc(sizeof(char*)* (MAXARGS+1));
   for(int j=0; j < MAXARGS+1; j++){
	   arglist[j] = (char*)malloc(sizeof(char)* ARGLEN);
      bzero(arglist[j],ARGLEN);
    }
   if(cmdline[0] == '\0')//if user has entered nothing and pressed enter key
      return NULL;
   int argnum = 0; //slots used
   char*cp = cmdline; // pos in string
   char*start;
   int len;
   while(*cp != '\0'){
      while(*cp == ' ' || *cp == '\t') //skip leading spaces
          cp++;
      start = cp; //start of the word
      len = 1;
      //find the end of the word (Splits cmdline into tokens based on spaces or tabs, storing each argument in arglist.)
      while(*++cp != '\0' && !(*cp ==' ' || *cp == '\t'))
         len++;
      strncpy(arglist[argnum], start, len);
      arglist[argnum][len] = '\0';
      argnum++;
   }
   arglist[argnum] = NULL;
   return arglist;
}      

char* read_cmd(char* prompt, FILE* fp){
   printf("%s", prompt);
  int c; //input character
   int pos = 0; //position of character in cmdline
   char* cmdline = (char*) malloc(sizeof(char)*MAX_LEN);
   while((c = getc(fp)) != EOF){ // f the user presses Ctrl+D (EOF), getc(fp) returns EOF, causing read_cmd to return NULL and allowing the shell to exit.
       if(c == '\n')
	  break;
       cmdline[pos++] = c;
   }
//these two lines are added, in case user press ctrl+d to exit the shell
   if(c == EOF && pos == 0) 
      return NULL;
   cmdline[pos] = '\0';
   return cmdline;  //eturns a dynamically allocated cmdline string containing the command typed by the user.
}