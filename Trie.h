
// double array trie tree

typedef unsigned short tchar;
typedef unsigned char tbyte;
typedef unsigned int tdword;

struct trie_tree;

typedef enum 
{
  STATE_NULL = 0,
  STATE_PREFIX = 1,
  STATE_WORD = 2,
  STATE_WORD_PREFIX = 3,
} TRIE_STATE;

// create function
void trie_tree_create_begin(int input_num);

trie_tree* trie_tree_create_end();

void trie_tree_free(trie_tree* tree);

// set input function
void trie_tree_set_input(int index, tchar* str);

//DFA function
TRIE_STATE trie_tree_check_state(trie_tree* tree, tchar c);

void trie_tree_clear_state(trie_tree* tree);

bool trie_tree_check_string(trie_tree* tree, tchar* str);

// insert and remove function
void trie_tree_insert_begin(int input_num);

void trie_tree_insert_end(trie_tree* tree);

// serialize
int trie_tree_serialize_len(trie_tree* tree);

void trie_tree_serialize(trie_tree* tree, tbyte* buf);

trie_tree* trie_tree_unserialize(tbyte* buf);

