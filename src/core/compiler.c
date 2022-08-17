#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/common.h"
#include "../common/object.h"
#include "compiler.h"
#include "lexer.h"

#ifdef DEBUG_PRINT_CODE
#include "../common/debug.h"
#endif //DEBUG_PRINT_CODE

Lexer lexer;
Parser parser;
Compiler* current = NULL;

static ParseRule* get_rule(TokenType type);
static void declaration();
static void var_declaration();
static void function_declaration();
static void statement();
static void print_statement();
static void if_statement();
static void while_statement();
static void for_statement();
static void block();
static void expression_statement();
static void return_statement();

static void expression();
static void parse_precedence(Precedence prec);

static void init_compiler(Compiler* compiler, FunctionType type){
    compiler->enclosing = current;
    compiler->fn = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->fn = new_function();
    current = compiler;
    if (type != TYPE_SCRIPT){
        current->fn->name = copy_string(parser.previous.start, parser.previous.length);
    }

    Local* local = &current->locals[current->local_count++];
    local->depth = 0;
    local->is_captured = false;
    local->name.start = "";
    local->name.length = 0;
}

static size_t get_column(Token* token){
    size_t col;
    for (col = 0; token->start[token->length-col-1] != '\n' && 
                ((token->start + token->length - col-1) - lexer.source) >= 0; col++);
    return col;
}

static void error_at(Token* token, const char* message){
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d:%d] Error", token->line, get_column(token));
    if (token->type == TOKEN_EOF){
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error_at_current(const char* message){
    error_at(&parser.current, message);
}

static void error(const char* message){
    error_at(&parser.previous, message);
}

static void advance(){
    parser.previous = parser.current;
    for(;;){
        parser.current = scan_token(&lexer);
        if (parser.current.type != TOKEN_ERROR) break;
        error_at_current(parser.current.err_msg);
    }
}

static void consume(TokenType type, const char* message){
    if (parser.current.type == type){
        advance();
        return;
    }
    error_at_current(message);
}

static bool match(TokenType type){
    if (parser.current.type != type) return false;
    advance();
    return true;
}

static Chunk* current_chunk(){
    return &current->fn->chunk;
}

static void emit_byte(uint8_t byte){
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2){
    write_chunk(current_chunk(), byte1, parser.previous.line);
    write_chunk(current_chunk(), byte2, parser.previous.line);
}

static void emit_return(){
    emit_byte(OP_NIL);
    emit_byte(OP_RETURN);
}

static void emit_loop(int32_t loop_start){
    int32_t diff = current_chunk()->count - loop_start + 1;
    emit_bytes(OP_LOOP, diff);
    emit_bytes(diff >> 8, diff >> 16);
}

static int32_t emit_jump(uint8_t byte){
    emit_bytes(byte, 0);
    emit_bytes(0, 0);
    return current_chunk()->count - 3;
}

static void patch_jump(int32_t jmp){
    int32_t diff = current_chunk()->count - jmp;
    current_chunk()->code[jmp+0] = diff;
    current_chunk()->code[jmp+1] = diff >> 8;
    current_chunk()->code[jmp+2] = diff >> 16;
}

static bool identifiers_equal(Token* a, Token* b){
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int32_t resolve_local(Compiler* compiler, Token* name){
    for (int32_t i = compiler->local_count - 1; i >= 0; i--){
        Local* local = compiler->locals + i;
        if (identifiers_equal(name, &local->name)){
            if (local->depth == -1) error("Can't read local variable in its own initializer");
            return i;
        }
    }
    return -1;
}

static int32_t add_upvalue(Compiler* compiler, int32_t index, bool is_local){
    size_t upvalue_count = compiler->fn->upvalue_count;
    for (size_t i = 0; i < upvalue_count; i++){
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local)
            return i;
    }
    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->fn->upvalue_count++;
    
}

static int32_t resolve_upvalue(Compiler* compiler, Token* name){
    if (compiler->enclosing == NULL) return -1;
    int32_t local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, local, true);
    }
    int32_t upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) return add_upvalue(compiler, upvalue, false);
    
    return -1; 
}

// static size_t identifier_constant(Token* name){
//     return add_constant(current_chunk(), OBJ_VAL(copy_string(name->start, name->length)));
// }

static ObjFunction* end_compiler(){
    emit_return();
    ObjFunction* fn = current->fn;
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error){
        disassemble_chunk(current_chunk(), fn->name != NULL ? fn->name->chars : "<Script>");
    }
#endif //DEBUG_PRINT_CODE
    current = current->enclosing;
    return fn;
}

static void synchronize(){
    parser.panic_mode = false;
    while (parser.current.type != TOKEN_EOF){
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch(parser.current.type){
            case TOKEN_CLASS: case TOKEN_FUN:    case TOKEN_VAR:
            case TOKEN_FOR:   case TOKEN_IF:     case TOKEN_WHILE:
            case TOKEN_PRINT: case TOKEN_RETURN: 
                return;
            default:;
        }
        advance();
    }
}

static void begin_scope(){
    current->scope_depth++;
}

static void end_scope(){
    current->scope_depth--;
    while(current->local_count > 0 && 
          current->locals[current->local_count - 1].depth > current->scope_depth)
    {
        if (current->locals[current->local_count-1].is_captured){
            emit_byte(OP_CLOSE_UPVALUE);
        } else {
            emit_byte(OP_POP);
        }
        current->local_count--;
    }
}

static void add_local(Token name){
    if (current->local_count == UINT24_COUNT){
        error("Too many local variables in function");
        return;
    }
    Local* local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
}

static void declare_var(){
    if (current->scope_depth == 0) return;
    Token* name = &parser.previous;
    for (int32_t i = current->local_count - 1; i >= 0; i--){
        Local* local = current->locals + i;
        if (local->depth != -1 && local->depth < current->scope_depth) break;
        if (identifiers_equal(name, &local->name)){
            error("Already a variable with this name in this scope");
        }
    }
    add_local(*name); // declaring variable to local scope
}

static void define_var(Value value){
    if (current->scope_depth > 0) {
        current->locals[current->local_count-1].depth = current->scope_depth; // defining local variable
        return;
    }
    if (current_chunk()->constants.count + 1 > UINT8_MAX){
        emit_byte(OP_DEFINE_GLOBAL_LONG);
        write_constant(current_chunk(), value, parser.previous.line);
    } else {
        emit_bytes(OP_DEFINE_GLOBAL, add_constant(current_chunk(), value));
    }
}

static Value parse_var(const char* err_msg){
    consume(TOKEN_IDENTIFIER, err_msg);
    declare_var();
    if (current->scope_depth > 0) return NIL_VAL;
    return OBJ_VAL(copy_string(parser.previous.start, parser.previous.length));
}

static void parse_precedence(Precedence prec){
    advance();
    if (parser.previous.type == TOKEN_EOF) return;
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL){
        error("Expected an expression");
        return;
    }
    bool can_assign = prec <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while(prec <= get_rule(parser.current.type)->precedence){
        advance();
        get_rule(parser.previous.type)->infix(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)){
        error("Invalid assignment target");
    }
}

static void declaration(){
    if (match(TOKEN_VAR)){
        var_declaration();
    } else if (match(TOKEN_FUN)){
        function_declaration();
    } else {
        statement();
    }
    if (parser.panic_mode) synchronize();
}

static void var_declaration(){
    consume(TOKEN_IDENTIFIER, "expected a variable name identifier");
    Value value;
    if (current->scope_depth > 0){
        declare_var();
    } else {
        value = OBJ_VAL(copy_string(parser.previous.start, parser.previous.length));
    }

    if (match(TOKEN_EQUAL)){
        expression();
    } else {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    
    define_var(value);
}

static void function(FunctionType type){
    Compiler compiler;
    init_compiler(&compiler, type);
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expected '(' after function name");
    if (parser.current.type != TOKEN_RIGHT_PAREN){
        do {
            current->fn->arity++;
            if (current->fn->arity > 255) error_at_current("Can't have more than 255 parameters in function");
            Value var = parse_var("Expected parameter name");
            define_var(var);
        } while(match(TOKEN_COMMA));
    } 
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters");
    consume(TOKEN_LEFT_BRACE, "Expected '{' before function body");
    block();
    ObjFunction* function = end_compiler();
    if (current_chunk()->constants.count + 1 > UINT8_MAX){
        emit_byte(OP_CLOSURE_LONG);
        write_constant(current_chunk(), OBJ_VAL(function), parser.previous.line);
    } else {
        emit_bytes(OP_CLOSURE, add_constant(current_chunk(), OBJ_VAL(function)));
    }
    for (size_t i = 0; i < function->upvalue_count; i++){
        emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
        int32_t index = compiler.upvalues[i].index;
        emit_bytes(index, index << 8);
        emit_byte(index << 16);
    }
}

static void function_declaration(){
    consume(TOKEN_IDENTIFIER, "Expected valid function name");
    Value value;
    if (current->scope_depth > 0){
        declare_var();
    } else {
        value = OBJ_VAL(copy_string(parser.previous.start, parser.previous.length));
    }
    
    if (current->scope_depth > 0) {
        current->locals[current->local_count-1].depth = current->scope_depth; // defining local variable
    }

    function(TYPE_FUNCTION);

    if (current->scope_depth > 0) {
        current->locals[current->local_count-1].depth = current->scope_depth; // defining local variable
        return;
    }
    if (current_chunk()->constants.count + 1 > UINT8_MAX){
        emit_byte(OP_DEFINE_GLOBAL_LONG);
        write_constant(current_chunk(), value, parser.previous.line);
    } else {
        emit_bytes(OP_DEFINE_GLOBAL, add_constant(current_chunk(), value));
    }
}

static void statement(){
    if (match(TOKEN_PRINT)){
        print_statement();
    } else if (match(TOKEN_IF)){
        if_statement();
    } else if (match(TOKEN_WHILE)){
        while_statement();
    } else if (match(TOKEN_FOR)){
        for_statement();
    } else if (match(TOKEN_LEFT_BRACE)){
        begin_scope();
        block();
        end_scope();
    } else if (match(TOKEN_RETURN)){
        return_statement();
    } else {
        expression_statement();
    }
}

static void print_statement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after value");
    emit_byte(OP_PRINT);
}

static void if_statement(){
    consume(TOKEN_LEFT_PAREN, "Expected '(' after if keyword");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' in if statement");
    int32_t then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    
    int32_t else_jump = emit_jump(OP_JUMP);
    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (match(TOKEN_ELSE)) statement();
    patch_jump(else_jump);
}

static void while_statement(){
    int32_t loop = current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expected '(' after while keyword");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' in while statement");
    int32_t exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void for_statement(){
    // for (vardecl; cond; incr) body
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expected '(' after for keyword");
    if (match(TOKEN_SEMICOLON)){
        // no initializer
    } else if (match(TOKEN_VAR)){
        var_declaration();
    } else {
        expression_statement();
    }
    int32_t loop_start = current_chunk()->count;
    int32_t exit_jump = -1;
    if (!match(TOKEN_SEMICOLON)){
        expression();
        consume(TOKEN_SEMICOLON, "Expected ';' after condition in for statement");
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }
    if (!match(TOKEN_RIGHT_PAREN)){
        int32_t body_jump = emit_jump(OP_JUMP);
        int32_t increment_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expected ')' after for clause");
        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }
    statement();
    emit_loop(loop_start);
    if (exit_jump != -1){
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }
    end_scope();
}

static void block(){
    while(parser.current.type != TOKEN_RIGHT_BRACE && parser.current.type != TOKEN_EOF){
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void expression_statement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after value");
    emit_byte(OP_POP);
}

static void return_statement(){
    if (current->type == TYPE_SCRIPT) 
        error("Can't return from top-level code");

    if (match(TOKEN_SEMICOLON)){
        emit_return();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expected ';' after return statement");
        emit_byte(OP_RETURN);
    }
}

static void expression(){
    parse_precedence(PREC_ASSIGNMENT);
}

static void number(bool can_assign){
    UNUSED(can_assign);
    double value = strtod(parser.previous.start, NULL);
    if (current_chunk()->constants.count + 1 > UINT8_MAX){
        emit_byte(OP_CONSTANT_LONG);
        write_constant(current_chunk(), NUM_VAL(value), parser.previous.line);
    } else {
        emit_bytes(OP_CONSTANT, add_constant(current_chunk(), NUM_VAL(value)));
    }
}

static void literal(bool can_assign){
    UNUSED(can_assign);
    switch(parser.previous.type){
        case TOKEN_NIL:   emit_byte(OP_NIL); break;
        case TOKEN_TRUE:  emit_byte(OP_TRUE); break;
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        default: return;
    }
}

static void string(bool can_assign){
    UNUSED(can_assign);
    Value value = OBJ_VAL(copy_string(parser.previous.start+1, parser.previous.length-2));
    if (current_chunk()->constants.count + 1 > UINT8_MAX){
        emit_byte(OP_CONSTANT_LONG);
        write_constant(current_chunk(), value, parser.previous.line);
    } else {
        emit_bytes(OP_CONSTANT, add_constant(current_chunk(), value));
    }
}


static void indices(bool can_assign){\
    UNUSED(can_assign);
    expression();
    consume(TOKEN_RIGHT_BRACKET, "Expected ']' after array index");
    emit_byte(OP_GET_INDEX);
}

static void variable(bool can_assign){
    
    uint8_t get_op, set_op;
    int32_t arg = resolve_local(current, &parser.previous);
    if (arg != -1){
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else if ((arg = resolve_upvalue(current, &parser.previous)) != -1){
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    } else {
        Value value = OBJ_VAL(copy_string(parser.previous.start, parser.previous.length));
        arg = add_constant(current_chunk(), value);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && match(TOKEN_EQUAL)){
        expression();
        if (set_op == OP_SET_LOCAL || set_op == OP_SET_UPVALUE){
            emit_bytes(set_op, (uint8_t)arg);
            emit_bytes((uint8_t)(arg >> 8), (uint8_t)(arg >> 16));
        } else if (current_chunk()->constants.count + 1 > UINT8_MAX){
            emit_bytes(OP_SET_GLOBAL_LONG, (uint8_t)arg);
            emit_bytes((uint8_t)(arg >> 8), (uint8_t)(arg >> 16));
        } else {
            emit_bytes(set_op, arg);
        }
    } else {
        if (get_op == OP_GET_LOCAL || get_op == OP_GET_UPVALUE){
            emit_bytes(get_op, (uint8_t)arg);
            emit_bytes((uint8_t)(arg >> 8), (uint8_t)(arg >> 16));
        } else if (current_chunk()->constants.count + 1 > UINT8_MAX){
            emit_bytes(OP_SET_GLOBAL_LONG, (uint8_t)arg);
            emit_bytes((uint8_t)(arg >> 8), (uint8_t)(arg >> 16));
        } else {
            emit_bytes(get_op, arg);
        }
        while (match(TOKEN_LEFT_BRACKET)){
            indices(can_assign);
        }
    }

}

static void grouping(bool can_assign){
    UNUSED(can_assign);
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void array(bool can_assign){
    UNUSED(can_assign);
    size_t count = 0;
    if (!match(TOKEN_RIGHT_BRACKET)){
        expression();
        count++;
        while(!match(TOKEN_RIGHT_BRACKET)){
            consume(TOKEN_COMMA, "Expected ',' in list");
            if (match(TOKEN_RIGHT_BRACKET)) break;
            expression();
            count++;
        }
    }

    if (count > UINT8_MAX){
        emit_bytes(OP_ARRAY_LONG, count);
        emit_bytes(count >> 8, count >> 16);
    } else {
        emit_bytes(OP_ARRAY, count);
    }
}

static void unary(bool can_assign){
    UNUSED(can_assign);
    TokenType operator = parser.previous.type;
    parse_precedence(PREC_UNARY);
    switch(operator){
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        default: return;
    }
}

static void binary(bool can_assign){
    UNUSED(can_assign);
    TokenType operator = parser.previous.type;
    ParseRule* rule = get_rule(operator);
    parse_precedence((Precedence)(rule->precedence+1));
    switch(operator){
        case TOKEN_PLUS:           emit_byte(OP_ADD); break;
        case TOKEN_MINUS:          emit_byte(OP_SUB); break;
        case TOKEN_STAR:           emit_byte(OP_MUL); break;
        case TOKEN_SLASH:          emit_byte(OP_DIV); break;
        case TOKEN_MOD:            emit_byte(OP_MOD); break;
        // equality and comparison
        case TOKEN_BANG_EQUAL:     emit_byte(OP_NOT_EQUAL); break;
        case TOKEN_EQUAL_EQUAL:    emit_byte(OP_EQUAL); break;
        case TOKEN_LESS:           emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:     emit_byte(OP_LESS_EQUAL); break;
        case TOKEN_GREATER:        emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL:  emit_byte(OP_GREATER_EQUAL); break;
        default: return;
    }
}

static uint8_t argument_list(){
    uint8_t count = 0;
    if (parser.current.type != TOKEN_RIGHT_PAREN){
        do {
            expression();
            if (count == 255){
                error("Can't have more than 255 arguments in function call");
            }
            count++;
        } while(match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
    return count;
}

static void call(bool can_assign){
    UNUSED(can_assign);
    uint8_t arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);
}

static void and_(bool can_assign){
    UNUSED(can_assign);
    int32_t end_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    parse_precedence(PREC_AND);
    patch_jump(end_jump);
}

static void or_(bool can_assign){
    UNUSED(can_assign);
    int32_t else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int32_t end_jump = emit_jump(OP_JUMP);
    patch_jump(else_jump);
    emit_byte(OP_POP);
    parse_precedence(PREC_OR);
    patch_jump(end_jump);
}

static void ternary(bool can_assign){
    UNUSED(can_assign);
    int32_t jmp = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    expression();
    int32_t end_jmp = emit_jump(OP_JUMP);
    consume(TOKEN_COLON, "expected ':' in ternary expression");
    patch_jump(jmp);
    expression();
    patch_jump(end_jmp);
}

ParseRule rules[] = { //      prefix     infix     precedence
    [TOKEN_LEFT_PAREN]     = {grouping,  call,     PREC_CALL},    
    [TOKEN_RIGHT_PAREN]    = {NULL,      NULL,     PREC_NONE},    
    [TOKEN_LEFT_BRACE]     = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]    = {NULL,      NULL,     PREC_NONE},
    [TOKEN_LEFT_BRACKET]   = {array,     NULL,     PREC_NONE}, 
    [TOKEN_RIGHT_BRACKET]  = {NULL,      NULL,     PREC_NONE},
    [TOKEN_COMMA]          = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_DOT]            = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_MINUS]          = {unary,     binary,   PREC_TERM}, 
    [TOKEN_PLUS]           = {NULL,      binary,   PREC_TERM},
    [TOKEN_SEMICOLON]      = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_SLASH]          = {NULL,      binary,   PREC_FACTOR}, 
    [TOKEN_STAR]           = {NULL,      binary,   PREC_FACTOR},
    [TOKEN_MOD]            = {NULL,      binary,   PREC_FACTOR}, 
    [TOKEN_QMARK]          = {NULL,      ternary,  PREC_TERNARY}, 
    [TOKEN_COLON]          = {NULL,      NULL,     PREC_NONE},
    [TOKEN_BANG]           = {unary,     NULL,     PREC_NONE}, 
    [TOKEN_BANG_EQUAL]     = {NULL,      binary,   PREC_EQUALITY}, 
    [TOKEN_EQUAL]          = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_EQUAL_EQUAL]    = {NULL,      binary,   PREC_EQUALITY},
    [TOKEN_GREATER]        = {NULL,      binary,   PREC_COMPARISON}, 
    [TOKEN_GREATER_EQUAL]  = {NULL,      binary,   PREC_COMPARISON}, 
    [TOKEN_LESS]           = {NULL,      binary,   PREC_COMPARISON}, 
    [TOKEN_LESS_EQUAL]     = {NULL,      binary,   PREC_COMPARISON},
    [TOKEN_IDENTIFIER]     = {variable,  NULL,     PREC_NONE}, 
    [TOKEN_STRING]         = {string,    NULL,     PREC_NONE},
    [TOKEN_NUMBER]         = {number,    NULL,     PREC_NONE},
    [TOKEN_AND]            = {NULL,      and_,     PREC_AND}, 
    [TOKEN_OR]             = {NULL,      or_,      PREC_OR}, 
    [TOKEN_PRINT]          = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_IF]             = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_ELSE]           = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_TRUE]           = {literal,   NULL,     PREC_NONE}, 
    [TOKEN_FALSE]          = {literal,   NULL,     PREC_NONE}, 
    [TOKEN_NIL]            = {literal,   NULL,     PREC_NONE},
    [TOKEN_FOR]            = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_WHILE]          = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_FUN]            = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_RETURN]         = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_CLASS]          = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_SUPER]          = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_THIS]           = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_VAR]            = {NULL,      NULL,     PREC_NONE},
    [TOKEN_ERROR]          = {NULL,      NULL,     PREC_NONE}, 
    [TOKEN_EOF]            = {NULL,      NULL,     PREC_NONE}, 
};

static ParseRule* get_rule(TokenType type){
    return &rules[type];
}

ObjFunction* compile(const char* source){

    init_lexer(&lexer, source);
    Compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    while(!match(TOKEN_EOF)){
        declaration();
    }
    ObjFunction* function = end_compiler();
    return parser.had_error ? NULL : function;
}

void print_tokens(Lexer* lexer){
    size_t line = -1;
    for (;;){
        Token token = scan_token(lexer);
        if(token.line != line){
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);
        if (token.type == TOKEN_EOF) break;
    }
}