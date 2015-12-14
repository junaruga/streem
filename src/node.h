#ifndef STRM_NODE_H
#define STRM_NODE_H

typedef struct node_string {
    size_t len;
    char buf[0];
} *node_string;

typedef struct node_error {
    int type;
    strm_value arg;
} node_error;

typedef struct parser_state {
    int nerr;
    void *lval;
    const char *fname;
    int lineno;
    int tline;
    /* TODO: should be separated as another context structure */
} parser_state;

void node_parse_init(parser_state *);
void node_parse_free(parser_state *);
int node_parse_file(parser_state *, const char *);
int node_parse_input(parser_state *, FILE * in, const char *);
int node_parse_string(parser_state *, const char *);
int node_run(parser_state *);
void node_raise(strm_state *, const char *);

typedef enum {
    NODE_INT,
    NODE_FLOAT,
    NODE_STR,
    NODE_NIL,
    NODE_BOOL,
    NODE_ARGS,
    NODE_PAIR,
    NODE_CFUNC,
    NODE_LAMBDA,
    NODE_IDENT,
    NODE_LET,
    NODE_IF,
    NODE_EMIT,
    NODE_SKIP,
    NODE_RETURN,
    NODE_BREAK,
    NODE_VAR,
    NODE_CONST,
    NODE_OP,
    NODE_CALL,
    NODE_ARRAY,
    NODE_NODES,
    NODE_NS,
    NODE_IMPORT,
} node_type;

#define NODE_HEADER node_type type

typedef struct {
    NODE_HEADER;
} node;

typedef struct {
    NODE_HEADER;
    int32_t value;
} node_int;

typedef struct {
    NODE_HEADER;
    double value;
} node_float;

typedef struct {
    NODE_HEADER;
    node_string value;
} node_str;

typedef struct {
    NODE_HEADER;
    int value;
} node_bool;

typedef struct {
    NODE_HEADER;
    node_string key;
    node *value;
} node_pair;

typedef struct {
    NODE_HEADER;
    int len;
    int max;
    node_string *data;
} node_args;

typedef struct {
    NODE_HEADER;
    int len;
    int max;
    node **data;
} node_nodes;

typedef struct {
    NODE_HEADER;
    int len;
    int max;
    node **data;
    node_string *headers;
    node_string ns;
} node_array;

typedef struct {
    NODE_HEADER;
    node *cond;
    node *then;
    node *opt_else;
} node_if;

typedef struct {
    NODE_HEADER;
    node *emit;
} node_emit;

typedef struct {
    NODE_HEADER;
    node_string lhs;
    node *rhs;
} node_let;

typedef struct {
    NODE_HEADER;
    node_string name;
} node_ident;

typedef struct {
    NODE_HEADER;
    node_string op;
    node *lhs;
    node *rhs;
} node_op;

typedef struct node_lambda {
    NODE_HEADER;
    node *args;
    node *compstmt;
} node_lambda;

typedef struct {
    NODE_HEADER;
    node_string ident;
    node *args;
} node_call;

typedef struct {
    NODE_HEADER;
    node *rv;
} node_return;

typedef struct {
    NODE_HEADER;
    node_string name;
    node *body;
} node_ns;

typedef struct {
    NODE_HEADER;
    node_string name;
} node_import;

extern node *node_array_new();
extern node *node_array_headers(node *);
extern void node_array_add(node *, node *);
extern void node_array_free(node_array *);
extern node *node_nodes_new();
extern void node_nodes_add(node *, node *);
extern node *node_nodes_concat(node *, node *);
extern node *node_pair_new(node_string, node *);
extern node *node_args_new();
extern void node_args_add(node *, node_string);
extern node *node_ns_new(node_string, node *);
extern node *node_import_new(node_string);
extern node *node_let_new(node_string, node *);
extern node *node_op_new(const char *, node *, node *);
extern node *node_obj_new(node *, node_string);
extern node *node_lambda_new(node *, node *);
extern node *node_method_new(node *, node *);
extern node *node_call_new(node_string, node *, node *, node *);
extern node *node_int_new(long);
extern node *node_float_new(double);
extern node *node_string_new(const char *, size_t);
extern node *node_if_new(node *, node *, node *);
extern node *node_emit_new(node *);
extern node *node_skip_new();
extern node *node_return_new(node *);
extern node *node_break_new();
extern node *node_ident_new(node_string);
extern node_string node_str_new(const char *, size_t len);
extern node_string node_str_escaped(const char *s, size_t len);
extern node *node_nil();
extern node *node_true();
extern node *node_false();
extern void node_free(node *);

#endif                          /* STRM_NODE_H */
