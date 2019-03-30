//Main Author: Michael Murray
//Date: 11/10/18
//Class: CS270

//Notes: This program newsh.c is heavily based upon shellex.c that was provided for us, 
//as well as including some verbatim code transplanting from SIGCHLD example procmask2.c.


/* $begin shellmain */
#include "csapp.h"
#include <stdbool.h>
#define MAXARGS   128
#define MAXVARS	  100
#define MAXARGSIZE 32

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
void setCommand(char **argv);
bool checkValidSet(char *varName);
void checkSetTable(char **argv);
void checkPaths(char **argv);
void parsePATH(char *buf, char **argP);
void insertBP(int pid, char *name);
void removeBP(int pid);
void handler(int sig);

char SHELLPROMPT[MAXLINE]; //holds PROMPT test
char cwd[MAXLINE];   //holds current working directory


struct variableInfo //structure for holding log of variables
{
     char name[MAXARGSIZE];
     char value[MAXARGSIZE];     
};

struct backgroundProcesses  //structure for hold log of background processes
{
     int processID;
     char processName[MAXARGSIZE];     
};

struct variableInfo setTable[MAXVARS];
struct backgroundProcesses bpTable[MAXVARS];

int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    strcpy(SHELLPROMPT,"newsh$"); // PROMPT string initialized to newsh$
    strcpy(setTable[0].name,"PATH");  //PATH variable starts initialized to 
    strcpy(setTable[0].value,"/bin/");   // "/bin/" this means immediatly typing command ls will work
    while (1) {
	/* Read */
	printf("%s ",SHELLPROMPT);                   
	Fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);

	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    //sig proc masking inits taken from procmask2.c from class slides
    //*************************
    sigset_t mask_all, mask_one, prev_one;  
    Sigfillset(&mask_all);
	Sigemptyset(&mask_one);
	Sigaddset(&mask_one, SIGCHLD);
	Signal(SIGCHLD, handler);
	//***************************

    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */

    if (!builtin_command(argv)) { 
    	Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);//block SIGCHILD
        if ((pid = Fork()) == 0) {   /* Child runs user job */
    		Sigprocmask(SIG_SETMASK, &prev_one, NULL);  //unblock SIGCHILD
            if (execve(argv[0], argv, environ) < 0) {
            	checkPaths(argv);  //if default fails, run against all paths saved in PATH variable
                fprintf(stderr,"%s: Command not found.\n", argv[0]);        
                exit(0);
            }
        }

		/* Parent waits for foreground job to terminate */
		Sigprocmask(SIG_BLOCK, &mask_all, NULL);//block SIGCHILD
		if (!bg) {
		    int status;
		    if (waitpid(pid, &status, 0) < 0)
				unix_error("waitfg: waitpid error");
		}
		else{
			insertBP((int)pid,argv[0]);		
		}
		Sigprocmask(SIG_SETMASK, &prev_one, NULL);//unblock SIGCHILD
    }
    return;
}

void handler(int sig) //handler for sigchild
{
	sigset_t mask_all, prev_all;
	pid_t pid;
	Sigfillset(&mask_all);	
	pid = waitpid(-1, NULL, 0);  //Reap child
	removeBP(pid); //remove child pid from background processes
}

//for adding new pid to background processes
void insertBP(int pid, char *name){
	for(int i=0; i < MAXVARS; i++){ //put info in first open slot
		if(bpTable[i].processID == 0){
			bpTable[i].processID = pid;
			strcpy(bpTable[i].processName,name);			
			break;
		}
	}
}

//for removing pid from background processes
void removeBP(int pid){
	for(int i=0; i < MAXVARS; i++){	
		if(bpTable[i].processID == 0) //if current inspect id is 0, stop loop
			break;
		if(bpTable[i].processID == pid){ //if match is found
			while(bpTable[i+1].processID != 0 || i+1 < MAXVARS){ //shift all items back until new item is blank
				bpTable[i].processID = bpTable[i+1].processID;
				strcpy(bpTable[i].processName,bpTable[i+1].processName);
				i++;
			}
			bpTable[i].processID = 0; //blank this last item as right now i and i-1 are identical
			strcpy(bpTable[i].processName,"");
			break;
		}
	}
}

// will check all paths saved in PATH variable until exhausted or successful run
void checkPaths(char **argv){
	char *argP[MAXARGS];
	parsePATH(setTable[0].value,argP); //tokenize PATH variable
	int i =0;
    while(argP[i]!=NULL){ //which token is not NULL
	    char str[MAXLINE];
		strcpy(str,argP[i]);  //copy token to str
		if(str[strlen(str)-1] != '/'){ //if token doesnt end with a '/' add one
			strcat(str,"/");
		}		
		strcat(str,argv[0]);  //concat with cmd issued
		if (execve(str, argv, environ) >= 0)  //this may not actually be needed, but theoretically will return if execution is valid
			return;		
	    i++;
	}	
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
		exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
		return 1;
	/*	Debugging function for testing removeBP
	if (!strcmp(argv[0], "r")){    Ignore singleton &
		removeBP(atoi(argv[1]));
		return 1;
	}
	*/
    if(!strcmp(argv[0],"cd")){ //if user enters cd
    	if(argv[1]==NULL)  //if there is no 2nd argument print to stderror and dont run rest of logic
    		fprintf(stderr, "One parameter required\n");
    	else{
	    	if (getcwd(cwd, sizeof(cwd)) != NULL) //first print current directory before change
	       		printf("Old working dir: %s\n", cwd); 		
	   		if (!chdir(argv[1])) {  //if new dir is valid, change and continue
	   			if (getcwd(cwd, sizeof(cwd)) != NULL) 
	       		printf("New working dir: %s\n", cwd);  //print new dir
	   		}
	   		else
	   			fprintf(stderr, "That directory does not exist\n" );  //error if not valid dir
   		}		
		return 1;
	}
    if(!strcmp(argv[0],"set")){ //if user types set, continue to set function
		setCommand(argv);
		return 1;
	}
    if(!strcmp(argv[0],"bp")){  //if user types bp
    	if(bpTable[0].processID != 0)  //if list isnt empty, print list header
    		printf("PID\tName\n");
    	else
    		printf("No background processes\n");  //if list is empty print that there isnt anything in the list, I dont count this as an error
		for(int i=0; i< MAXVARS; i++){ //for all elements of the background list
			if(bpTable[i].processID == 0)  //since list is contiguous, break loop if find a blank
				break;
			printf("%i\t%s\n",bpTable[i].processID,bpTable[i].processName);  //print info
		}
		return 1;
	}
    return 0;                     /* Not a builtin command */
}


void setCommand(char **argv){
	if (argv[1]==NULL || argv[2]==NULL){  //ensure not less than 3 args, I dont stop more than 3, but they will just be ignored
		fprintf(stderr,"PROMPT Assign failed, not enough arguments\n");
	}
	else{
	    if(!strcmp(argv[1], "PROMPT")) //special condition if var is PROMPT
	   		strcpy(SHELLPROMPT,argv[2]);
	    else if(argv[1][0] > 64 && argv[1][0] < 123 && checkValidSet(argv[1]))	  //if first char is letter and check rest of variable characters to be valid  	
	   		checkSetTable(argv);  //add/edit set table
	    else
	    	fprintf(stderr, "Invalid Variable Name\n");  //error if failed previous logic
	}
}

void checkSetTable(char **argv){
	for(int i=0; i < MAXVARS; i++){
		if(!strcmp(setTable[i].name,"")){  //if we find a blank, put in the info
			strcpy(setTable[i].name,argv[1]);
			strcpy(setTable[i].value,argv[2]);
			break;
		}
		if(!strcmp(setTable[i].name,argv[1])){ //if we find a name match, update the value
			strcpy(setTable[i].value,argv[2]);
			break;
		}
	}	
}


bool checkValidSet(char *varName){ //ensure letter or number for chars past index 0 of var name
	int i =1;
	while(varName[i] != '\0'){
			if((varName[i] > 64 && varName[i] < 123)|| (varName[i] > 47 && varName[i] < 58))
				i++;
			else
				return false;			
	}
	return true;
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */
    bool varFlag = false;

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	    if(buf[0] == '%') //If first character of buf is a %, break loop which will ignore rest of the line
	    	break;
	    
	    if(buf[0] == '$'){ //if first char is a $ will perform token substituition at end of the loop iteration
	    	varFlag = true;
	    }    	    
		argv[argc++] = buf;
		*delim = '\0';
		buf = delim + 1;
		while (*buf && (*buf == ' ')) /* Ignore spaces */
	            buf++;

	    if(varFlag){
	    	for(int i=0; i < MAXVARS; i++){
	    		if(!strcmp(setTable[i].name,argv[argc-1]+1)){  //name match is found against token ignoring char 0 (the $)
	    			strcpy(argv[argc-1],setTable[i].value);  //overwrite token with var value
	    			break;
	    		}
	    		else if(i == MAXVARS-1){  //if end of table without a match
	    			fprintf(stderr, "Variable not found\n");  //var not found error
	    			argv[0] = NULL;  // to ensure return function doesnt attempt to process the line, nulling argv 0 will trigger intended response in calling function
	    			varFlag =false;
	    			return bg;
	    		}
	    	}
	    	varFlag =false;
	    }
	    //printf("%s\n",argv[argc-1] ); <-debug
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
		return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
		argv[--argc] = NULL;

    return bg;
}

void parsePATH(char *buf, char **argP) //this is an edited version of provided parseLine function, changed to target : instead of spaces
{
    char *delim;         /* Points to first : delimiter */
    int argc;            /* Number of args */
    strcat(buf,":"); // Add : to the end of the string
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;

    /* Build the argP list */
    argc = 0;
    while ((delim = strchr(buf, ':'))) {  //delimit by :	        
		argP[argc++] = buf;
		*delim = '\0';
		buf = delim + 1;
		while (*buf && (*buf == ':')) /* Ignore ':' */
	            buf++;	    
    }
    argP[argc] = NULL;    
}