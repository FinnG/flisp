#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <editline/readline.h>
#include <editline/history.h>

#include "mpc.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

char* load_grammar(const char* filename)
{
    /* TODO: this function needs lots of error checking */
    
    int fd = open(filename, O_RDONLY);
    if(fd == -1) {
        goto error;
    }

    int buffer_size = 1024;
    char* grammar = malloc(buffer_size);
    printf("Read %d\n", read(fd, grammar, buffer_size));

    return grammar;
    
error:
    return NULL;
}

/* Use operator string to see which operation to perform */
long eval_op(long x, char* op, long y) {
    
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    return 0;
}

long eval(mpc_ast_t* t)
{
    /* If tagged as number return it directly. */ 
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }
    
    /* The operator is always second child. */
    char* op = t->children[1]->contents;
    
    /* We store the third child in `x` */
    long x = eval(t->children[2]);
    
    /* Iterate the remaining children and combining. */
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    
    return x;  
}

int main()
{
    char* prompt = "flisp> ";

    char* grammar = load_grammar("grammar");
    
    /* Create Some Parsers */
    mpc_parser_t* number   = mpc_new("number");
    mpc_parser_t* operator = mpc_new("operator");
    mpc_parser_t* expr     = mpc_new("expr");
    mpc_parser_t* flisp    = mpc_new("flisp");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT, grammar,
              number, operator, expr, flisp);
    
    printf("Finn's lisp interpreter 0.0.1\n");
    printf("Press Ctrl+C to exit\n");

    while(true) {
        char* input = readline(prompt);
        add_history(input);

        /* Attempt to parse the user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, flisp, &r)) {
            /* On success evaluate the input */
            long result = eval(r.output);
            printf("%li\n", result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print the error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        free(input);
    }
    
    return 0;
}
