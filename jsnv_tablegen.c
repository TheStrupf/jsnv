// ============================================================================
// MIT License
//
// Copyright(c) 2023 Lukas Wolski
//
// Permission is hereby granted,
// free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all copies
// or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS",
// WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER
// LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ============================================================================

#include <stdio.h>

enum jsn_states {
    // ROOT STATE
    JSNV___0 = 0,
    // STRING STATES
    JSNV_STR, // string parsing
    JSNV_SSL, // previous character was '\' -> escape
    JSNV_SH1, // "\u", check for 4 hex characters
    JSNV_SH2, // hex character 2
    JSNV_SH3, // hex character 3
    JSNV_SH4, // hex character 4
    // ARRAY STATES
    JSNV_ARR, // array after '['
    JSNV_A_V, // array after value
    JSNV_A_C, // array after comma
    // OBJECT STATES
    JSNV_OBJ, // object after '{'
    JSNV_O_S, // object after string (key name)
    JSNV_O_K, // object after ':'
    JSNV_O_V, // object after value
    JSNV_O_C, // object after comma
    // TRUE STATES
    JSNV_T_0, // "true" after 't'
    JSNV_T_1, // "true" after 'r'
    // FALSE STATES
    JSNV_F_0, // "false" after 'f'
    JSNV_F_1, // "false" after 'a'
    JSNV_F_2, // "false" after 'l'
    // TRUE/FALSE ENDING STATE
    JSNV_T_F, // "false" and "true": both look for ending 'e'
    // NULL STATES
    JSNV_0_0, // "null" after 'n'
    JSNV_0_1, // "null" after 'u'
    JSNV_0_2, // "null" after 'l'
    // NUMBER STATES
    JSNV_N_M, // number after '-'
    JSNV_N_0, // number after leading '0'
    JSNV_N_I, // number in integer digit parsing mode
    JSNV_NF1, // number after '.' (fraction)
    JSNV_NF2, // number in fraction digit parsing mode
    JSNV_NE1, // number after 'e'/'E', exponent
    JSNV_NE2, // number after '+'/'-', exponent
    JSNV_NE3, // number in exponent digit parsing mode
};

enum jsn_actions {
    JSNV_ACTION_INVALID = 0,
    JSNV_ACTION_SET_PUT,  // push a state on the stack and set a current state
    JSNV_ACTION_SET,      // set a current state
    JSNV_ACTION_POP,      // pop a state from the stack and set it as the current state
    JSNV_ACTION_POP_DECR, // pop, but also decrement the string character counter by 1
};

static unsigned short jsnv_encode_pop()
{
    return 3;
}

static unsigned short jsnv_encode_pop_decr()
{
    return 4;
}

static unsigned short jsnv_encode_push(int push, int next)
{
    unsigned short a = (push << 10) | (next << 4) | 1;
    return a;
}

static unsigned short jsnv_encode_next(int next)
{
    unsigned short a = (next << 4) | 2;
    return a;
}

// macro for mapping whitespace to the same state
#define JSNV_MAP_WS(STATE)                                  \
    jsnv_action_map[STATE][' ']  = jsnv_encode_next(STATE); \
    jsnv_action_map[STATE]['\n'] = jsnv_encode_next(STATE); \
    jsnv_action_map[STATE]['\t'] = jsnv_encode_next(STATE); \
    jsnv_action_map[STATE]['\r'] = jsnv_encode_next(STATE);

// macro to map value transition
#define JSNV_MAP_VALUE(STATE, PUSH)                                         \
    jsnv_action_map[STATE]['\"'] = jsnv_encode_push(PUSH, JSNV_STR);        \
    jsnv_action_map[STATE]['t']  = jsnv_encode_push(PUSH, JSNV_T_0);        \
    jsnv_action_map[STATE]['f']  = jsnv_encode_push(PUSH, JSNV_F_0);        \
    jsnv_action_map[STATE]['n']  = jsnv_encode_push(PUSH, JSNV_0_0);        \
    jsnv_action_map[STATE]['-']  = jsnv_encode_push(PUSH, JSNV_N_M);        \
    jsnv_action_map[STATE]['0']  = jsnv_encode_push(PUSH, JSNV_N_0);        \
    for (int n = 1; n <= 9; n++)                                            \
        jsnv_action_map[STATE]['0' + n] = jsnv_encode_push(PUSH, JSNV_N_I); \
    jsnv_action_map[STATE]['{'] = jsnv_encode_push(PUSH, JSNV_OBJ);         \
    jsnv_action_map[STATE]['['] = jsnv_encode_push(PUSH, JSNV_ARR);

// macro for mapping number ending characters
#define JSNV_MAP_NUM_END(STATE)                            \
    jsnv_action_map[STATE]['\0'] = jsnv_encode_pop_decr(); \
    jsnv_action_map[STATE][' ']  = jsnv_encode_pop();      \
    jsnv_action_map[STATE]['\n'] = jsnv_encode_pop();      \
    jsnv_action_map[STATE]['\t'] = jsnv_encode_pop();      \
    jsnv_action_map[STATE]['\r'] = jsnv_encode_pop();      \
    jsnv_action_map[STATE][',']  = jsnv_encode_pop_decr(); \
    jsnv_action_map[STATE][']']  = jsnv_encode_pop_decr(); \
    jsnv_action_map[STATE]['}']  = jsnv_encode_pop_decr();

int main(void)
{
    unsigned short jsnv_action_map[32][128] = {0};

    // OUTER ROOT OBJECT
    JSNV_MAP_WS(JSNV___0)
    JSNV_MAP_VALUE(JSNV___0, JSNV___0) // after the first value we want to go back to the root object again

    // TRUE ===================================================================
    jsnv_action_map[JSNV_T_0]['r'] = jsnv_encode_next(JSNV_T_1);
    jsnv_action_map[JSNV_T_1]['u'] = jsnv_encode_next(JSNV_T_F);

    // FALSE ==================================================================
    jsnv_action_map[JSNV_F_0]['a'] = jsnv_encode_next(JSNV_F_1);
    jsnv_action_map[JSNV_F_1]['l'] = jsnv_encode_next(JSNV_F_2);
    jsnv_action_map[JSNV_F_2]['s'] = jsnv_encode_next(JSNV_T_F);

    jsnv_action_map[JSNV_T_F]['e'] = jsnv_encode_pop();

    // NULL ===================================================================
    jsnv_action_map[JSNV_0_0]['u'] = jsnv_encode_next(JSNV_0_1);
    jsnv_action_map[JSNV_0_1]['l'] = jsnv_encode_next(JSNV_0_2);
    jsnv_action_map[JSNV_0_2]['l'] = jsnv_encode_pop();

    // STRING  ================================================================
    for (int n = 32; n <= 126; n++)
        jsnv_action_map[JSNV_STR][n] = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_STR]['\"'] = jsnv_encode_pop();
    jsnv_action_map[JSNV_STR]['\\'] = jsnv_encode_next(JSNV_SSL);

    // STRING SLASH ESCAPE CHARACTER  =========================================
    jsnv_action_map[JSNV_SSL]['\"'] = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_SSL]['\\'] = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_SSL]['/']  = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_SSL]['b']  = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_SSL]['f']  = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_SSL]['n']  = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_SSL]['r']  = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_SSL]['t']  = jsnv_encode_next(JSNV_STR);
    jsnv_action_map[JSNV_SSL]['u']  = jsnv_encode_next(JSNV_SH1);
    // STRING HEX 1-4  ========================================================
    {
        // STRING HEX 1
        for (int n = 0; n <= 9; n++)
            jsnv_action_map[JSNV_SH1]['0' + n] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['a'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['b'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['c'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['d'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['e'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['f'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['A'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['B'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['C'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['D'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['E'] = jsnv_encode_next(JSNV_SH2);
        jsnv_action_map[JSNV_SH1]['F'] = jsnv_encode_next(JSNV_SH2);
        // STRING HEX 2
        for (int n = 0; n <= 9; n++)
            jsnv_action_map[JSNV_SH2]['0' + n] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['a'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['b'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['c'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['d'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['e'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['f'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['A'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['B'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['C'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['D'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['E'] = jsnv_encode_next(JSNV_SH3);
        jsnv_action_map[JSNV_SH2]['F'] = jsnv_encode_next(JSNV_SH3);
        // STRING HEX 3
        for (int n = 0; n <= 9; n++)
            jsnv_action_map[JSNV_SH3]['0' + n] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['a'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['b'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['c'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['d'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['e'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['f'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['A'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['B'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['C'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['D'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['E'] = jsnv_encode_next(JSNV_SH4);
        jsnv_action_map[JSNV_SH3]['F'] = jsnv_encode_next(JSNV_SH4);
        // STRING HEX 4
        for (int n = 0; n <= 9; n++)
            jsnv_action_map[JSNV_SH4]['0' + n] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['a'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['b'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['c'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['d'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['e'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['f'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['A'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['B'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['C'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['D'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['E'] = jsnv_encode_next(JSNV_STR);
        jsnv_action_map[JSNV_SH4]['F'] = jsnv_encode_next(JSNV_STR);
    }

    // ARRAY ==================================================================

    // ARRAY OPENING
    JSNV_MAP_WS(JSNV_ARR)
    JSNV_MAP_VALUE(JSNV_ARR, JSNV_A_V)
    jsnv_action_map[JSNV_ARR][']'] = jsnv_encode_pop();

    // ARRAY AFTER COMMA
    JSNV_MAP_WS(JSNV_A_C)
    jsnv_action_map[JSNV_A_C]['\"'] = jsnv_encode_push(JSNV_A_V, JSNV_STR);
    jsnv_action_map[JSNV_A_C]['t']  = jsnv_encode_push(JSNV_A_V, JSNV_T_0);
    jsnv_action_map[JSNV_A_C]['f']  = jsnv_encode_push(JSNV_A_V, JSNV_F_0);
    jsnv_action_map[JSNV_A_C]['n']  = jsnv_encode_push(JSNV_A_V, JSNV_0_0);
    jsnv_action_map[JSNV_A_C]['-']  = jsnv_encode_push(JSNV_A_V, JSNV_N_M);
    jsnv_action_map[JSNV_A_C]['0']  = jsnv_encode_push(JSNV_A_V, JSNV_N_0);
    for (int n = 1; n <= 9; n++)
        jsnv_action_map[JSNV_A_C]['0' + n] = jsnv_encode_push(JSNV_A_V, JSNV_N_I);
    jsnv_action_map[JSNV_A_C]['{'] = jsnv_encode_push(JSNV_A_V, JSNV_OBJ);
    jsnv_action_map[JSNV_A_C]['['] = jsnv_encode_push(JSNV_A_V, JSNV_ARR);

    // ARRAY AFTER VALUE
    JSNV_MAP_WS(JSNV_A_V)
    jsnv_action_map[JSNV_A_V][']'] = jsnv_encode_pop();
    jsnv_action_map[JSNV_A_V][','] = jsnv_encode_next(JSNV_A_C);

    // OBJECT =================================================================

    // OBJECT OPENING
    JSNV_MAP_WS(JSNV_OBJ)
    jsnv_action_map[JSNV_OBJ]['}']  = jsnv_encode_pop();
    jsnv_action_map[JSNV_OBJ]['\"'] = jsnv_encode_push(JSNV_O_S, JSNV_STR);

    // OBJECT AFTER KEY NAME
    JSNV_MAP_WS(JSNV_O_S)
    jsnv_action_map[JSNV_O_S][':'] = jsnv_encode_next(JSNV_O_K);

    // OBJECT AFTER :
    JSNV_MAP_WS(JSNV_O_K)
    JSNV_MAP_VALUE(JSNV_O_K, JSNV_O_V)

    // OBJECT AFTER VALUE
    JSNV_MAP_WS(JSNV_O_V)
    jsnv_action_map[JSNV_O_V]['}'] = jsnv_encode_pop();
    jsnv_action_map[JSNV_O_V][','] = jsnv_encode_next(JSNV_O_C);

    // OBJECT COMMA
    JSNV_MAP_WS(JSNV_O_C)
    jsnv_action_map[JSNV_O_C]['\"'] = jsnv_encode_push(JSNV_O_S, JSNV_STR);

    // NUMBER =================================================================

    // after -sign
    jsnv_action_map[JSNV_N_M]['0'] = jsnv_encode_next(JSNV_N_0);
    for (int n = 1; n <= 9; n++)
        jsnv_action_map[JSNV_N_M]['0' + n] = jsnv_encode_next(JSNV_N_I);

    // integer number
    JSNV_MAP_NUM_END(JSNV_N_I)
    for (int n = 0; n <= 9; n++)
        jsnv_action_map[JSNV_N_I]['0' + n] = jsnv_encode_next(JSNV_N_I);
    jsnv_action_map[JSNV_N_I]['.'] = jsnv_encode_next(JSNV_NF1);
    jsnv_action_map[JSNV_N_I]['e'] = jsnv_encode_next(JSNV_NE1);
    jsnv_action_map[JSNV_N_I]['E'] = jsnv_encode_next(JSNV_NE1);

    // after first 0
    JSNV_MAP_NUM_END(JSNV_N_0)
    jsnv_action_map[JSNV_N_0]['.'] = jsnv_encode_next(JSNV_NF1);
    jsnv_action_map[JSNV_N_0]['e'] = jsnv_encode_next(JSNV_NE1);
    jsnv_action_map[JSNV_N_0]['E'] = jsnv_encode_next(JSNV_NE1);

    // fraction 1
    for (int n = 0; n <= 9; n++)
        jsnv_action_map[JSNV_NF1]['0' + n] = jsnv_encode_next(JSNV_NF2);

    // fraction 2
    JSNV_MAP_NUM_END(JSNV_NF2)
    for (int n = 0; n <= 9; n++)
        jsnv_action_map[JSNV_NF2]['0' + n] = jsnv_encode_next(JSNV_NF2);
    jsnv_action_map[JSNV_NF2]['e'] = jsnv_encode_next(JSNV_NE1);
    jsnv_action_map[JSNV_NF2]['E'] = jsnv_encode_next(JSNV_NE1);

    // after e/E exponent
    jsnv_action_map[JSNV_NE1]['-'] = jsnv_encode_next(JSNV_NE2);
    jsnv_action_map[JSNV_NE1]['+'] = jsnv_encode_next(JSNV_NE2);
    for (int n = 0; n <= 9; n++)
        jsnv_action_map[JSNV_NE1]['0' + n] = jsnv_encode_next(JSNV_NE3);

    // after +/- in exponent
    for (int n = 0; n <= 9; n++)
        jsnv_action_map[JSNV_NE2]['0' + n] = jsnv_encode_next(JSNV_NE3);

    // after first digit in exponent
    JSNV_MAP_NUM_END(JSNV_NE3)
    for (int n = 0; n <= 9; n++)
        jsnv_action_map[JSNV_NE3]['0' + n] = jsnv_encode_next(JSNV_NE3);

    // PRINT MAP ==============================================================

    printf("static const unsigned short jsnv_action_map[32][128] = {");
    for (int i = 0; i < 32; i++) {
        for (int n = 0; n < 128; n++) {
            if (n % 16 == 0) printf("\n    ");
            printf("0x%04x", jsnv_action_map[i][n]);
            if (i != 31 || n != 127)
                printf(", ");
        }
    }
    printf("};\n");
    return 0;
}