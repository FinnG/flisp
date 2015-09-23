/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_SYM, LVAL_QEXPR, LVAL_SEXPR, LVAL_ERR };

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

lval* lval_num(long x);

lval* lval_err(char* m);

lval* lval_qexpr(void);

lval* lval_sym(char* s);

lval* lval_sexpr(void);

void lval_del(lval* v);

void lval_expr_print(lval* v, char open, char close);

void lval_print(lval* v);

void lval_println(lval* v);

lval* lval_add(lval* v, lval* x);

lval* lval_eval_sexpr(lval* v);

lval* lval_eval(lval* v);

lval* lval_pop(lval* v, int i);

lval* lval_take(lval* v, int i);

lval* lval_join(lval* x, lval* y);
