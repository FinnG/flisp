#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <editline/readline.h>
#include <editline/history.h>

#include "mpc.h"
#include "lval.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

lval* ast_read_num(mpc_ast_t* t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

lval* ast_read(mpc_ast_t* t)
{
    if(strstr(t->tag, "number")) { return ast_read_num(t); }
    if(strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If we get here, we're dealing with an sexpr */
    
    lval* x = NULL;
    /* TODO: perhaps an assert would be better here? x always = lval_sexpr() */
    if(strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } /* AST root */
    if(strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if(strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

    for(int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }

        x = lval_add(x, ast_read(t->children[i]));
    }

    return x;
}

lval* builtin_head(lval* a)
{
    LASSERT(a, a->count == 1,
            "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'head' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
            "Function 'head' passed {}!");

    lval* v = lval_take(a, 0);  
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval* builtin_tail(lval* a)
{
    LASSERT(a, a->count == 1,
            "Function 'tail' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'tail' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
            "Function 'tail' passed {}!");

    lval* v = lval_take(a, 0);  
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lval* a)
{
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_join(lval* a)
{
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type.");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_eval(lval* a)
{
    LASSERT(a, a->count == 1,
            "Function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect type!");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
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

lval* builtin(lval* a, char* func)
{
    if (strcmp("list", func) == 0) { return builtin_list(a); }
    if (strcmp("head", func) == 0) { return builtin_head(a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(a); }
    if (strcmp("join", func) == 0) { return builtin_join(a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(a); }
    if (strstr("+-/*", func)) { return builtin_op(a, func); }
    lval_del(a);
    return lval_err("Unknown Function!");
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
    lval* result = builtin(v, first->sym);
    lval_del(first);
    return result;
}

lval* lval_eval(lval* v)
{
    /* The only thing we evaluate is an sexpr */
    if(v->type == LVAL_SEXPR) return lval_eval_sexpr(v);
    return v;
}

int main()
{
    char* prompt = "flisp> ";

    /* Create Some Parsers */
    mpc_parser_t* number   = mpc_new("number");
    mpc_parser_t* symbol   = mpc_new("symbol");
    mpc_parser_t* qexpr    = mpc_new("qexpr");
    mpc_parser_t* sexpr    = mpc_new("sexpr");
    mpc_parser_t* expr     = mpc_new("expr");
    mpc_parser_t* flisp    = mpc_new("flisp");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                        \
                number : /-?[0-9]+/ ;                                  \
                symbol : \"list\" | \"head\" | \"tail\"                \
                       | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ; \
                sexpr  : '(' <expr>* ')' ;                             \
                qexpr  : '{' <expr>* '}' ;                             \
                expr   : <number> | <symbol> | <sexpr> | <qexpr> ;     \
                flisp  : /^/ <expr>* /$/ ;                             \
              ",
              number, symbol, sexpr, qexpr, expr, flisp);
    
    printf("Finn's lisp interpreter 0.0.1\n");
    printf("Press Ctrl+C to exit\n");

    while(true) {
        char* input = readline(prompt);
        add_history(input);

        /* Attempt to parse the user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, flisp, &r)) {
            /* On success evaluate the input */
            lval* x = ast_read(r.output);
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

    mpc_cleanup(5, number, symbol, sexpr, qexpr, expr, flisp);
    
    return 0;
}
