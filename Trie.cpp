#include "Trie.h"
#include <assert.h>
#include <malloc.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#define HEAD_INDEX 1
#define HEAD_CHECK -1

struct trie_node
{
  int base;
  int check;
  int prev;
  int next;
  int son;
  //void* attr;
};

struct trie_array
{
  int len;
  int max;
  int elem_size;
  tbyte* data;
};

struct trie_tree
{
  trie_array node_array;
  //int tail;
};

struct trie_input
{
  tchar* str;
  //void* attr;
};

struct trie_successor
{
  int base;
  int son;
  //void* attr;
  tchar c;
  tchar active;
};

static trie_array input_cache;
static trie_array successor_array;
static bool be_creating;
static bool be_inserting;
static int checking_index;

// array function 
static inline void* get_array_elem(trie_array* in_array, int index)
{
  assert(in_array);
  assert(in_array->max >= in_array->len);
  assert(index >= 0);
  assert(index < in_array->len);
  assert(in_array->elem_size > 0);
  return &in_array->data[index * in_array->elem_size];
}

static int append_array(trie_array* in_array, int elem_num)
{
  assert(in_array);
  assert(in_array->max >= in_array->len);
  assert(in_array->elem_size > 0);
  int index = in_array->len;
  if( (in_array->len += elem_num ) >= in_array->max )
  {
    in_array->max = in_array->len + 3 * in_array->len / 8 + 32;
    in_array->data = (tbyte*)realloc(in_array->data, in_array->max * in_array->elem_size);
  }
  return index;
}

static void init_array(trie_array* in_array, int in_elem_size)
{
  assert(in_array);
  assert(in_elem_size > 0);
  in_array->len = 0;
  in_array->max = 0;
  in_array->elem_size = in_elem_size;
  in_array->data = NULL;
}

static void realloc_array(trie_array* in_array)
{
  assert(in_array);
  in_array->data = (tbyte*)malloc(in_array->max * in_array->elem_size);
}

static void empty_array(trie_array* in_array)
{
  assert(in_array);
  assert(in_array->max >= in_array->len);
  assert(in_array->elem_size > 0);
  free(in_array->data);
  in_array->len = 0;
  in_array->max = 0;
  in_array->data = NULL;
}

// input function
void trie_tree_create_begin(int input_num)
{
  assert( !be_inserting );
  assert(input_num > 0);
  assert(input_cache.len==0);
  assert(successor_array.len==0);
  be_creating = true;
  init_array(&input_cache, sizeof(trie_input));
  append_array(&input_cache, input_num);
}

void trie_tree_set_input(int index, tchar* str)
{
  assert( str );
  assert( be_creating || be_inserting );
  trie_input* input_node = (trie_input*)get_array_elem(&input_cache, index);
  input_node->str = str;
  //input_node->attr = attr;
}

void trie_tree_free(trie_tree* tree)
{
  assert(tree);
  empty_array(&tree->node_array);
  free(tree);
}

static inline bool is_empty_trie_node(trie_node* node)
{
  if( node->check == 0)
  {
    assert( node->base == 0 );
    return true;
  }
  return false;
}

static int successors_cmp( const void *a , const void *b )
{
  return ((trie_successor*)a)->c > ((trie_successor*)b)->c ? 1 : -1;
}

static bool check_successors_unique(tchar c)
{
  for(int i = 0; i<successor_array.len; i++)
  {
    if(((trie_successor*)get_array_elem(&successor_array, i))->c == c)
      return false;
  }
  return true;
}

static void get_all_successors(tchar* str, int prefix_end, int search_begin_pos)
{
  assert( search_begin_pos >= 0);
  assert( search_begin_pos < input_cache.len );
  successor_array.len = 0;
  for(int i = search_begin_pos; i<input_cache.len; i++)
  {
    tchar* check_str = ((trie_input*)get_array_elem(&input_cache, i))->str;
    int check_ptr = 0;
    while( check_ptr < prefix_end && check_str[check_ptr] && check_str[check_ptr] == str[check_ptr] )
      check_ptr++;
    if( check_ptr == prefix_end && check_str[prefix_end] && check_successors_unique(check_str[prefix_end]))
    {
      int index = append_array(&successor_array, 1);
      trie_successor* succ = (trie_successor*)get_array_elem(&successor_array, index);
      succ->c = check_str[prefix_end];
      succ->active = false;
    }
  }
  if( successor_array.len > 1 && be_creating ) // 插入时的排序操作在reset_successor函数
    qsort(get_array_elem(&successor_array, 0), successor_array.len, sizeof(trie_successor), successors_cmp);
}

static void link_trie_node_next(trie_tree* tree, int base_index, int next_index)
{
  assert( tree );
  trie_node* base_node = (trie_node*)get_array_elem(&tree->node_array, base_index);
  trie_node* next_node = (trie_node*)get_array_elem(&tree->node_array, next_index);
  next_node->next = base_node->next;
  next_node->prev = base_index;
  trie_node* next_next_node = (trie_node*)get_array_elem(&tree->node_array, next_node->next);
  base_node->next = next_next_node->prev = next_index;
}

static void unlink_trie_node(trie_tree* tree, int index)
{
  assert( tree );
  assert( index > 0 );
  trie_node* node = (trie_node*)get_array_elem(&tree->node_array, index);
  trie_node* prev_node = (trie_node*)get_array_elem(&tree->node_array, node->prev);
  trie_node* next_node = (trie_node*)get_array_elem(&tree->node_array, node->next);
  prev_node->next = node->next;
  next_node->prev = node->prev;
  node->next = node->prev = index;
}

// link to empty_node list
// index 0 always empty;
static void empty_trie_node(trie_tree* tree, int node_index)
{
  assert( tree );
  assert( node_index >= 0);
  assert( node_index < tree->node_array.len );
  trie_node* node = (trie_node*)get_array_elem(&tree->node_array, node_index);
  //node->attr = 0;
  node->base = node->check = node->son = 0;
  int pre_index = node_index-1;
  while( pre_index >= 0 )
  {
    trie_node* prev_node = (trie_node*)get_array_elem(&tree->node_array, pre_index);
    if( is_empty_trie_node(prev_node) )
    {
      link_trie_node_next(tree, pre_index, node_index);
      break;
    }
    pre_index--;
  }
  assert(pre_index>=0);
}


static int append_empty_node(trie_tree* tree, int node_index)
{
  assert( tree );
  assert( node_index >= 0);
  assert( node_index < tree->node_array.len );
  trie_node* node = (trie_node*)get_array_elem(&tree->node_array, node_index);
  assert( is_empty_trie_node(node) );
  int empty_index = node->next;
  if(empty_index == 0)
  {
    int append_index = append_array(&tree->node_array, 1);
    empty_trie_node(tree, append_index);
    empty_index = append_index;
  }
  return empty_index;
}


static int find_base_index_by_successors(trie_tree* tree, int forbidden_index)
{
  assert(tree);
  assert(tree->node_array.len > 0);
  assert(successor_array.len > 0);
  tchar min_char = ((trie_successor*)get_array_elem(&successor_array, 0))->c;
  int base_index;
  int empty_index = 0;
  while( true )
  {
    empty_index = append_empty_node(tree, empty_index);
    //trie_node* node = (trie_node*)get_array_elem(&tree->node_array, empty_index);
    base_index = empty_index - (int)min_char;
    if( base_index > 0 && base_index != forbidden_index)
    {
      int succ_index = 0;
      for( ; succ_index<successor_array.len; succ_index++)
      {
        tchar check_char = ((trie_successor*)get_array_elem(&successor_array, succ_index))->c;
        int check_index = base_index + (int)check_char;
        if( check_index < tree->node_array.len )
        {
          trie_node* check_node = (trie_node*)get_array_elem(&tree->node_array, check_index);
          if( check_node->base != 0 )
          {
            assert(check_node->check != 0);
            break;
          }
        }
        else // 增加长度
        {
          tchar last_char = ((trie_successor*)get_array_elem(&successor_array, successor_array.len-1))->c;
          int append_len = base_index + (int)last_char - tree->node_array.len + 1;
          int tail_index = append_array(&tree->node_array, append_len);
          for(  ;tail_index < tree->node_array.len; tail_index++)
            empty_trie_node(tree, tail_index);
          succ_index = successor_array.len;
          break;
        }
      }
      if( succ_index == successor_array.len )
        break;
    }
  }
  return base_index;
}

static void change_son_check_index(trie_tree* tree, int node_index)
{
  assert(tree);
  assert(node_index>HEAD_INDEX);
  int first_index = ((trie_node*)get_array_elem(&tree->node_array, node_index))->son;
  int son_index = first_index;
  do
  {
    assert(son_index>0);
    trie_node* node = (trie_node*)get_array_elem(&tree->node_array, son_index);
    node->check = node_index;
    son_index = node->next;
  }
  while(son_index != first_index);

}

static void insert_successors(trie_tree* tree, int base_index, int check_index)
{
  assert(tree);
  assert(base_index >= 0);
  assert(check_index>0);
  trie_node* check_node = (trie_node*)get_array_elem(&tree->node_array, check_index);
  check_node->base = check_node->base < 0 ? -base_index : base_index;
  for(int succ_index=0; succ_index<successor_array.len; succ_index++)
  {
    trie_successor* succ = (trie_successor*)get_array_elem(&successor_array, succ_index);
    int insert_index = base_index + (int)succ->c;
    trie_node* node = (trie_node*)get_array_elem(&tree->node_array, insert_index);
    assert( is_empty_trie_node(node) );
    unlink_trie_node(tree, insert_index);
    node->check = check_index;
    node->base = insert_index;
    if( succ->active )
    {
      node->base = succ->base;
      node->son = succ->son;
      //node->attr = succ->attr;
      change_son_check_index(tree, insert_index);
    }
    if( check_node->son == 0 )
      check_node->son = insert_index;
    else
      link_trie_node_next(tree, check_node->son, insert_index);
  }
}

static void reset_successors(trie_tree* tree, int check_index)
{
  assert(tree);
  assert(check_index>0);
  trie_node* check_node = (trie_node*)get_array_elem(&tree->node_array, check_index);
  int son_index = check_node->son;
  if(son_index > 0)
  {
    int unlink_index;
    do 
    {
      int append_index = append_array(&successor_array, 1);
      trie_successor* succ = (trie_successor*)get_array_elem(&successor_array, append_index);
      trie_node* son_node = (trie_node*)get_array_elem(&tree->node_array, son_index);
      unlink_index = son_index;
      assert( son_index > abs(check_node->base) ) ;
      succ->active = true;
      succ->base = son_node->base;
      succ->son = son_node->son;
      succ->c = son_index - abs(check_node->base);
      //succ->attr = son_node->attr;
      assert(son_node->next>0);
      son_index = son_node->next;
      unlink_trie_node(tree, unlink_index);
      empty_trie_node(tree, unlink_index);
    }
    while(son_index != unlink_index);
    check_node->son = 0;
    if( successor_array.len > 1 )
      qsort(get_array_elem(&successor_array, 0), successor_array.len, sizeof(trie_successor), successors_cmp);
  }
}

static bool find_prefix(trie_tree* tree, tchar* str, int* out_prefix_index, int* out_node_index)
{
  assert(tree);
  assert(tree->node_array.len > HEAD_INDEX);
  assert(str);
  int prefix_index = 0;
  int check_index = HEAD_INDEX;
  while( str[prefix_index] )
  {
    trie_node* check_node = (trie_node*)get_array_elem(&tree->node_array, check_index);
    if( check_node->base == check_index )
      break;
    int next_index = abs(check_node->base) + (int)str[prefix_index];
    if( next_index >= tree->node_array.len )
      break;
    trie_node* next_node = (trie_node*)get_array_elem(&tree->node_array, next_index);
    if( next_node->check != check_index )
      break;
    check_index = next_index;
    prefix_index++;
  }
  *out_prefix_index = prefix_index;
  *out_node_index = check_index;
  if( str && str[prefix_index] == 0) // 已成词
  {
    assert(prefix_index>0);
    assert(check_index>HEAD_INDEX);
    return false;
  }
  return true;
}

static void mark_word_node(trie_tree* tree, int index)
{
  assert(tree);
  assert(tree->node_array.len > HEAD_INDEX);
  assert(index > HEAD_INDEX);
  trie_node* node = (trie_node*)get_array_elem(&tree->node_array, index);
  assert(node->check != 0);
  assert(node->base != 0);
  //assert(node->attr == 0);
  //node->attr = attr;
  if( node->base > 0 )
    node->base = -node->base;
}

// debug
static tchar out_char[256];
static void print_node(trie_tree* tree, int index, int tail)
{
  trie_node* node = (trie_node*)get_array_elem(&tree->node_array, index);
  if( node->check > 0)
  {
    trie_node* prev_node = (trie_node*)get_array_elem(&tree->node_array, node->check);
    tchar c = index - abs(prev_node->base);
    out_char[tail-1] = c;
  }
  if( node->base < 0 && tail>0)
  {
    out_char[tail] = 0;
    wprintf(L"%s; ", out_char);
  }
  int son_index = node->son;
  if( node->son > 0 )
  {
    do
    {
      print_node(tree, son_index, tail+1);
      son_index = ((trie_node*)get_array_elem(&tree->node_array, son_index))->next;
      assert(son_index>0);
    }
    while(son_index != node->son);
  }

}

trie_tree* trie_tree_create_end()
{
  assert(be_creating);
  assert(!be_inserting);
  trie_tree* tree = (trie_tree*)malloc(sizeof(trie_tree));
  init_array(&tree->node_array, sizeof(trie_node));
  init_array(&successor_array, sizeof(trie_successor));
  append_array(&tree->node_array, 2);
  memset(get_array_elem(&tree->node_array, 0), 0, sizeof(trie_node)*2);
  trie_node* head_node = (trie_node*)get_array_elem(&tree->node_array, HEAD_INDEX);
  head_node->check = HEAD_CHECK;
  head_node->base = HEAD_INDEX;
  for(int input_index = 0; input_index<input_cache.len; input_index++)
  {
    int prefix_index, node_index, base_index;
    trie_input* input_node = (trie_input*)get_array_elem(&input_cache, input_index);
    while( find_prefix(tree, input_node->str, &prefix_index, &node_index) )
    {
      get_all_successors(input_node->str, prefix_index, input_index);
      base_index = find_base_index_by_successors(tree, node_index);
      insert_successors(tree, base_index, node_index);
    }
    mark_word_node(tree, node_index);
  }
  empty_array(&input_cache);
  empty_array(&successor_array);
  be_creating = false;
  return tree;
}

TRIE_STATE trie_tree_check_state(trie_tree* tree, tchar c)
{
  assert(tree);
  assert(checking_index>0);
  trie_node* checking_node = (trie_node*)get_array_elem(&tree->node_array, checking_index);
  int next_index = abs(checking_node->base) + (int)c;
  TRIE_STATE state = STATE_NULL;
  if( next_index < tree->node_array.len )
  {
    trie_node* next_node = (trie_node*)get_array_elem(&tree->node_array, next_index);
    if( next_node->check == checking_index )
    {
      checking_index = next_index;
      assert( next_node->base != 0);
      if(next_node->base == -next_index)
        state = STATE_WORD;
      else if( next_node->base < 0 )
        state = STATE_WORD_PREFIX;
      else
      {
        assert(next_node->base != next_index);
        state = STATE_PREFIX;
      }
    }
  }
  return state;
}

void trie_tree_clear_state(trie_tree* tree)
{
  checking_index = HEAD_INDEX;
}

inline tchar to_lower(tchar c)
{
  static const tchar sub = 'a' - 'A';
  if( c >= 'A' && c <= 'Z' )
    return c + sub;
  return c;
}

bool trie_tree_check_string(trie_tree* tree, tchar* str)
{
  assert(tree);
  assert(str);
  int state_index, base_index = 0;
  bool is_replace = false;
  while( str[base_index] )
  {
    trie_tree_clear_state(tree);
    state_index = base_index;
    while( str[state_index] )
    {
      TRIE_STATE state = trie_tree_check_state(tree, to_lower(str[state_index]));
      if( state == STATE_NULL )
        break;
      if( state == STATE_WORD )
      {
        for(int replace_index = base_index; replace_index<=state_index; replace_index++)
          str[replace_index] = L'*';
        is_replace = true;
        base_index = state_index;
        break;
      }
      if( state == STATE_WORD_PREFIX)
      {
        for(int replace_index = base_index; replace_index<=state_index; replace_index++)
          str[replace_index] = L'*';
        is_replace = true;
      }
      state_index++;
    }
    base_index++;
  }
  return is_replace;
}

bool check_insert_successors(trie_tree* tree, int check_index)
{
  assert(tree);
  assert(check_index>=HEAD_INDEX);
  trie_node* check_node = (trie_node*)get_array_elem(&tree->node_array, check_index);
  int succ_index=0;
  for(; succ_index<successor_array.len; succ_index++)
  {
    trie_successor* succ = (trie_successor*)get_array_elem(&successor_array, succ_index);
    assert(!succ->active);
    int insert_index = abs(check_node->base) + (int)succ->c;
    if( insert_index >= tree->node_array.len )  // 此处暂时纳入重新设定base的流程，可优化
      break;
    trie_node* node = (trie_node*)get_array_elem(&tree->node_array, insert_index);
    if( !is_empty_trie_node(node) )
    {
      assert(node->check != check_index);
      break;
    }
  }
  return (succ_index == successor_array.len);
}

void delete_existing_successors(trie_tree* tree, int check_index)
{
  assert(tree);
  assert(check_index>=HEAD_INDEX);
  trie_node* check_node = (trie_node*)get_array_elem(&tree->node_array, check_index);
  int move_index = 0;
  for(int succ_index=0; succ_index<successor_array.len; succ_index++)
  {
    trie_successor* succ = (trie_successor*)get_array_elem(&successor_array, succ_index);
    bool existing = false;
    int son_index = check_node->son;
    if(son_index>0)
    {
      do
      {
        trie_node* son_node = (trie_node*)get_array_elem(&tree->node_array, son_index);
        assert(son_index > abs(check_node->base) );
        if( (son_index-abs(check_node->base)) == succ->c )
        {
          existing = true;
          break;
        }
        assert(son_node->next>0);
        son_index = son_node->next;
      }
      while(son_index != check_node->son);
    }
    if( !existing )
    {
      trie_successor* move_succ = (trie_successor*)get_array_elem(&successor_array, move_index);
      move_succ->c = succ->c;
      move_index += 1;
    }
  }
  successor_array.len = move_index;
}

void trie_tree_insert_begin(int input_num)
{
  assert(!be_creating);
  assert(input_num > 0);
  assert(input_cache.len==0);
  assert(successor_array.len==0);
  be_inserting = true;
  init_array(&input_cache, sizeof(trie_input));
  append_array(&input_cache, input_num);
}

void trie_tree_insert_end(trie_tree* tree)
{
  assert(!be_creating);
  assert(be_inserting);
  init_array(&successor_array, sizeof(trie_successor));
  for(int input_index = 0; input_index<input_cache.len; input_index++)
  {
    int prefix_index, node_index;
    trie_input* input_node = (trie_input*)get_array_elem(&input_cache, input_index);
    while( find_prefix(tree, input_node->str, &prefix_index, &node_index) )
    {
      int base_index = abs(((trie_node*)get_array_elem(&tree->node_array, node_index))->base);
      get_all_successors(input_node->str, prefix_index, input_index);
      delete_existing_successors(tree, node_index);
      if(base_index == node_index || !check_insert_successors(tree, node_index) )
      {
        reset_successors(tree, node_index);
        base_index = find_base_index_by_successors(tree, node_index);
      }
      insert_successors(tree, base_index, node_index);
    }
    mark_word_node(tree, node_index);
  }
  print_node(tree, 1, 0);
  empty_array(&input_cache);
  empty_array(&successor_array);
  be_inserting = false;
}

// serialize
int trie_tree_serialize_len(trie_tree* tree)
{
  int array_size = tree->node_array.max * tree->node_array.elem_size;
  return  array_size + sizeof(trie_tree);
}

void trie_tree_serialize(trie_tree* tree, tbyte* buf)
{
  int array_size = tree->node_array.max * tree->node_array.elem_size;
  memcpy(buf, tree, sizeof(trie_tree));
  memcpy(buf + sizeof(trie_tree), tree->node_array.data, array_size);
}

trie_tree* trie_tree_unserialize(tbyte* buf)
{
  trie_tree* tree = (trie_tree*)malloc(sizeof(trie_tree));
  memcpy(tree, buf, sizeof(trie_tree));
  int array_size = tree->node_array.max * tree->node_array.elem_size;
  realloc_array(&tree->node_array);
  memcpy(tree->node_array.data, buf + sizeof(trie_tree), array_size);
  return tree;
}
