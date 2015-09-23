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

/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_ERR };

/* Create Enumeration of Possible Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM }; 

typedef struct lval {
    int type;
    long num; /* for LVAL_NUM */

    char* err; /* for LVAL_ERR */
    char* sym; /* for LVAL_SYM */

    /* for LVAL_SEXPR */
    int count;
    struct lval** cell;
} lval;




lval* lval_eval(lval* x);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* builtin_op(lval* a, char* op);


/* Construct a pointer to a new Number lval */ 
lval* lval_num(long x)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error lval */ 
lval* lval_err(char* m)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

/* Construct a pointer to a new Symbol lval */ 
lval* lval_sym(char* s)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* A pointer to a new empty Sexpr lval */
lval* lval_sexpr(void)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v)
{

    switch (v->type) {
        /* Do nothing special for number type */
    case LVAL_NUM: break;

        /* For Err or Sym free the string data */
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

        /* If Sexpr then delete all elements inside */
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++) {
            lval_del(v->cell[i]);
        }
        /* Also free the memory allocated to contain the pointers */
        free(v->cell);
        break;
    }

    /* Free the memory allocated for the "lval" struct itself */
    free(v);
}

/* TODO: move into headers */
void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {

        /* Print Value contained within */
        lval_print(v->cell[i]);

        /* Don't print trailing space if last element */
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval* v)
{
    lval_print(v);
    putchar('\n');
}

lval* lval_read_num(mpc_ast_t* t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

lval* lval_read(mpc_ast_t* t)
{
    if(strstr(t->tag, "number")) { return lval_read_num(t); }
    if(strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If we get here, we're dealing with an sexpr */
    
    lval* x = NULL;
    /* TODO: perhaps an assert would be better here? x always = lval_sexpr() */
    if(strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } /* AST root */
    if(strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }

    for(int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }

        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

lval* lval_eval_sexpr(lval* v)
{
    /* Evaluate children and handle errors */
    for(int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
        if(v->cell[i]->type == LVAL_ERR) return lval_take(v, i);
    }

    /* Empty expression */
    if(v->count == 0) return v;

    /* Single expression */
    if(v->count == 1) return lval_take(v, 0);

    /* Ensure first element is symbol */
    lval* first = lval_pop(v, 0);
    if(first->type != LVAL_SYM) {
        lval_del(first);
        lval_del(v);
        return lval_err("S-expression does not start with a symbol.");
    }

    /* Call builtin with operator */
    lval* result = builtin_op(v, first->sym);
    lval_del(first);
    return result;
}

lval* lval_eval(lval* v)
{
    /* The only thing we evaluate is an sexpr */
    if(v->type == LVAL_SEXPR) return lval_eval_sexpr(v);
    return v;
}

lval* lval_pop(lval* v, int i)
{
    /* Find the item at "i" */
    lval* x = v->cell[i];

    /* Shift memory after the item at "i" over the top */
    memmove(&v->cell[i], &v->cell[i+1],
            sizeof(lval*) * (v->count-i-1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i)
{
    lval* result = lval_pop(v, i);
    lval_del(v);
    return result;
}

lval* builtin_op(lval* a, char* op)
{
    /* Ensure all arguments are numbers */
    for(int i = 0; i < a->count; i++) {
        if(a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }            
    }

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    /* If no arguments and sub then perform unary negation */
    if((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while(a->count > 0) {
        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        /* Select the op */
        if(strcmp(op, "+") == 0) x->num += y->num;
        if(strcmp(op, "-") == 0) x->num -= y->num;
        if(strcmp(op, "*") == 0) x->num *= y->num;
        if(strcmp(op, "/") == 0) {
            if(y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by zero");
                break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }

    /* Cleanup and return */
    lval_del(a);
    return x;
}

int main()
{
    char* prompt = "flisp> ";

    char* grammar =
        "number   : /-?[0-9]+/ ;\n"
        "symbol   : '+' | '-' | '*' | '/' ;\n"
        "expr     : <number> | '(' <operator> <expr>+ ')' ;\n"
        "sexpr    : '(' <expr>* ')' ;\n"
        "flisp    : /^/ <operator> <expr>+ /$/ ;\n";
    
    /* Create Some Parsers */
    mpc_parser_t* number   = mpc_new("number");
    mpc_parser_t* symbol   = mpc_new("symbol");
    mpc_parser_t* sexpr     = mpc_new("sexpr");
    mpc_parser_t* expr     = mpc_new("expr");
    mpc_parser_t* flisp    = mpc_new("flisp");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                      \
                number : /-?[0-9]+/ ;                                \
                symbol : '+' | '-' | '*' | '/' ;                     \
                sexpr  : '(' <expr>* ')' ;                           \
                expr   : <number> | <symbol> | <sexpr> ;             \
                flisp  : /^/ <expr>* /$/ ;                           \
              ",
              number, symbol, sexpr, expr, flisp);
    
    printf("Finn's lisp interpreter 0.0.1\n");
    printf("Press Ctrl+C to exit\n");

    while(true) {
        char* input = readline(prompt);
        add_history(input);

        /* Attempt to parse the user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, flisp, &r)) {
            /* On success evaluate the input */
            lval* x = lval_read(r.output);
            x = lval_eval(x);
            lval_println(x);
            lval_del(x);            
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print the error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        free(input);
    }

    mpc_cleanup(5, number, symbol, sexpr, expr, flisp);
    
    return 0;
}
