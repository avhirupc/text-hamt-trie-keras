#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <queue>
#include <bitset>
#include <fstream>
#include <bits/stdc++.h>
#include <algorithm>
#include <stdlib.h>     /* srand, rand */
#include <time.h>
#include <random>
#include <thread>
#include <mutex>

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <ctime>
#endif

#define MAX_BITS 32
#define MAX_LEFT_SHIFT_BITS 31
#define UINT32_MAX_BIT_SET 2147483648
#define UINT32_MAX_BIT_SET_COMPLEMENT 2147483647

using namespace std;

typedef uint32_t byte_8;

/* GLOBALS */

typedef long long int64; typedef unsigned long long uint64;
long long extra_byte_count = 0;
//TODO: testing: track byte allocation
const bool LIMIT_WORD_LIST = false;
const bool MUTATE_WORD_LIST = true;
const bool RUN_HASH_TRIE_BENCHMARK = true;
const bool RUN_ADAPTIVE_BENCHMARK = true;
const bool RUN_LOCALIZED_BENCHMARK = true;
const bool RUN_LOCALIZED_RD_BENCHMARK = true;
const bool RUN_STD_HAMT_BENCHMARK = true;
const bool RUN_LOCALIZED_CORRECTNESS_TEST = false; // needs adaptive benchmark set to true, decouple adaptive construction from benchmark; needs hash trie benchmark set to true

// efficient trie
struct node {
    byte_8 map;
    vector <node> next_v;
};

unordered_map<int, int> valToBitCount;
uint64_t total_node_count = 1;
mutex add_mutex;
mutex adaptive_add_mutex;
mutex adaptive_search_mutex;

/* Dynamic adaptive hash trie */

struct AdaptiveTrieNode {
    AdaptiveTrieNode* parent;
    uint32_t map;
    uint8_t child_n;
    uint32_t accesses;
    vector<AdaptiveTrieNode*> next_v;
};

struct AdaptiveTrieNodeComp{
    bool operator()(const AdaptiveTrieNode* a,const AdaptiveTrieNode* b) const{
        return a->accesses < b->accesses;
    }
};

struct PairAdaptiveTrieNodeCompG{
    bool operator()(const pair<AdaptiveTrieNode*, uint32_t> &a,const pair<AdaptiveTrieNode*, uint32_t> &b) const{
        return a.first->accesses > b.first->accesses;
    }
};

struct PairAdaptiveTrieNodeCompL{
    bool operator()(const pair<AdaptiveTrieNode*, uint32_t> &a,const pair<AdaptiveTrieNode*, uint32_t> &b) const{
        return a.first->accesses < b.first->accesses;
        // TODO: check max heap comparator correctness (opposite of max heap: correction)
    }
};

int AdaptiveTrieNode_nodeCount = 0;
map<AdaptiveTrieNode*, int> parentIndex;
vector<AdaptiveTrieNode*> global_list;

uint32_t* g_test_local = nullptr;
vector<uint32_t> g_test_hist;

// TODO: autocorrect: sorted_list (i, j boundaries)
// memory efficient / vs complexity efficient

struct Trie {
    bool isEndOfWord;
    unordered_map<char, Trie*> map;
};

int trie_node_count = 0;

/* GLOBALS end */

/* FUNC declarations */

// timing utility
uint64 GetTimeMs64();

// HAMT constuctions utilites
inline uint8_t get_set_bits_r_mapped(byte_8 &map, uint8_t n);
inline uint8_t get_set_bits_r(byte_8 &map, uint8_t n);
inline void expand_arr(node* &root, byte_8 &map, uint8_t &index);
inline void expand_custom_arr(node* &root, byte_8 &map, uint8_t &index);
void add_word(node *root, const char * word);

// HAMT algorithm variations
node* check_word(node *root, const char *word);
node* check_word_mapped(node *root, const char *word);
inline bool check_word_builtin(node *root, const char *word);

// base hashmap based trie algorithm
Trie* getNewTrieNode();
void insert(Trie*& root, const string& str);
bool search(Trie* root, const string& str);

// tree flattening algorithm
uint32_t* TrieUnrolling(uint32_t *trie_node_arr, node* root);
inline bool unrolledSearch(uint32_t* arr, const char *word);

// adaptive algorithm
AdaptiveTrieNode* constructAdaptiveTrie(node* root);
inline bool AdaptiveSearch(AdaptiveTrieNode* root, const char *word);
uint32_t* AdaptiveTrieFlattening(uint32_t* trie_node_arr, AdaptiveTrieNode* root);

// localized adaptive algorithms - (best performance currently)
uint32_t* ConstructLocalized(AdaptiveTrieNode* root, bool greater, bool perform_sort);
inline bool localizedAdaptiveUnrolledSearch(uint32_t* localized_arr, const char *word);
inline bool localizedAdaptiveUnrolledSearch64Bit(const char *word);
inline bool localizedAdaptiveUnrolledSearchPointRelative(uint32_t* localized_arr, const char *word);
// research implementations
inline bool localizedAdaptiveUnrolledQuantumOffsetSearch(const char *word);
inline bool localizedAdaptiveUnrolledSeparatedSearch(const char *word);
inline bool localizedAdaptiveUnrolledOffsetSearch(const char *word);

// word mutator
vector<string> mutateList(vector<string> &list);

// multithreaded routines
void searchRoutine(node* root, vector<string> *list, int st, int en);
void HashSearchRoutine(Trie* root, vector<string> *list, int st, int en);
void unrolledSearchRoutine(uint32_t *arr, vector<string> *list, int st, int en);
void addRoutine(node* root, vector<string> *list, int st, int en);
void adaptiveSearchRoutine(uint32_t *arr, vector<string> *list, int st, int en);
AdaptiveTrieNode* adaptiveAddRoutine(node* root, vector<string> &list, int &lim);
void localizedAdaptiveSearchRoutine(uint32_t* localized_arr, vector<string> *list, int st, int en, int type);

/* FUNC declarations end */

/* Returns the amount of milliseconds elapsed since the UNIX epoch. Works on both
 * windows and linux. */
uint64 GetTimeMs64(){
    #ifdef _WIN32
     /* Windows */
     FILETIME ft;
     LARGE_INTEGER li;
     /* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (UTC) and copy it
      * to a LARGE_INTEGER structure. */
     GetSystemTimeAsFileTime(&ft);
     li.LowPart = ft.dwLowDateTime;
     li.HighPart = ft.dwHighDateTime;

     uint64 ret = li.QuadPart;
     ret -= 116444736000000000LL; /* Convert from file time to UNIX epoch time. */
     ret /= 10000; /* From 100 nano seconds (10^-7) to 1 millisecond (10^-3) intervals */
     return ret;
    #else
     /* Linux */
     struct timeval tv;
     gettimeofday(&tv, NULL);
     uint64 ret = tv.tv_usec;
     /* Convert from micro seconds (10^-6) to milliseconds (10^-3) */
     ret /= 1000;
     /* Adds the seconds (10^0) after converting them to milliseconds (10^-3) */
     ret += (tv.tv_sec * 1000000);
     return ret;
    #endif
}

inline uint8_t get_set_bits_r_mapped(byte_8 &map, uint8_t n){
    return valToBitCount[map & ((1<<n) - 1)];
}

inline uint8_t get_set_bits_r(byte_8 &map, uint8_t n){
    // nth bit already set, find set bits to the right of nth bit
    // hash alternative
    byte_8 temp = map & ((1<<n) - 1);
    std::unordered_map<int, int>::iterator it = valToBitCount.find(temp);
    if (it != valToBitCount.end()){
        return it->second;
    }
    byte_8 test = (1 << n) - 1;
    uint8_t index = 0;
    while(temp){
        temp &= (temp - 1);
        ++index;
    }
    valToBitCount[temp] = index;
    return index;
}

// TODO: optimize
inline void expand_arr(node* &root, byte_8 &map, uint8_t &index){
    //TODO: testing: remove
    extra_byte_count += sizeof(node);
    root->next_v.insert(root->next_v.begin() + index, node({0, vector<node>()}));
    // instead of iteratively expanding array, double it?
}

inline void expand_custom_arr(node* &root, byte_8 &map, uint8_t &index){ // TODO: deprecated
    // remove testing block, replace MAX_BITS with (8*4)-1
    // uint8_t len = get_set_bits_r(map, MAX_BITS);
    // if (!len){
    //         root->next = new node*[1];
    //         return;
    //     }
    //     node * n_next[len+1];
    // for (uint8_t i=0; i<index; ++i){
    //     n_next[i] = root->next[i];
    // }
    // // merge loops with conditional?
    // for (uint8_t i=index+1; i<len+1; ++i){
    //     n_next[i] = root->next[i-1];
    // }
    // // delete[] root->next;
    // root->next = n_next;
}

void add_word(node *root, const char * word){
    int count = 0;
    add_mutex.lock();
    for(const char * ch=word; *ch!='\0'; ++ch, ++count){
        if (!((*ch) >= 'a' && (*ch) <= 'z')){
            cout << "Alert! char not in lowercase range" << endl;
        }
        uint8_t index = __builtin_popcount(root->map & ((1 <<((*ch) - 'a')) - 1));
        if (!(root->map & (1 << ((*ch) - 'a')))){
            // std::lock_guard<std::mutex> guard(g_pages_mutex);
            expand_arr(root, root->map, index);
            ++total_node_count;
            root->map |= 1 << ((*ch) - 'a');
        }
        root = &(root->next_v[index]);
    }
    // marking word ending by setting max bit
    root->map = root->map | UINT32_MAX_BIT_SET;
    // cout << bitset<MAX_BITS>(UINT32_MAX_BIT_SET) << endl;
    // cout << bitset<MAX_BITS>(root->map) << endl;
    add_mutex.unlock();
}

// inline it and don't overwrite root, create ans pointer node
node* check_word(node *root, const char *word){
    for (const char *ch=word; *ch!='\0'; ++ch){
        // optimize char subtraction
        if ( !(root->map & (1 << ((*ch) - 'a')))){
            return nullptr;
        }
        uint8_t index = get_set_bits_r(root->map, ((*ch) - 'a'));
        root = &(root->next_v[index]);
    }
    return root;
}

node* check_word_mapped(node *root, const char *word){
    for (const char *ch=word; *ch!='\0'; ++ch){
        if ( !(root->map & (1 << ((*ch) - 'a')))){
            return nullptr;
        }
        uint8_t index = get_set_bits_r_mapped(root->map, ((*ch) - 'a'));
        root = &(root->next_v[index]);
    }
    return root;
}

inline bool check_word_builtin(node *root, const char *word){
    for (const char *ch=word; *ch!='\0'; ++ch){
        // optimize char subtraction
        uint32_t bitmask = (1 << ((*ch) - 'a'));
        if ( !(root->map & bitmask)){
            return false;
        }
        root = &(root->next_v[__builtin_popcount(root->map & (bitmask - 1))]);
    }
    if (root->map & UINT32_MAX_BIT_SET)
        return true; // word ends here
    return false;
}

Trie* getNewTrieNode(){
    trie_node_count++;
    Trie* node = new Trie;
    node->isEndOfWord = false;
    return node;
}

// TODO: rename generic trie routines
void insert(Trie*& root, const string& str){
    if (root == nullptr)
        root = getNewTrieNode();

    Trie* temp = root;
    for (int i = 0; i < str.length(); i++) {
        char x = str[i];

        if (temp->map.find(x) == temp->map.end())
            temp->map[x] = getNewTrieNode();
        temp = temp->map[x];
    }
    temp->isEndOfWord = true;
}

bool search(Trie* root, const string& str){
    if (root == nullptr)
        return false;
    Trie* temp = root;
    std::unordered_map<char, Trie*>::iterator nxt = temp->map.begin();
    for (int i = 0; i < str.length(); i++) {
        nxt = temp->map.find(str[i]);
        if (nxt == temp->map.end()){
            return false;
        } temp = nxt->second;
    }
    return temp->isEndOfWord;
}

//cache miss friendly implementation, convert nodes into a continuous array
uint32_t* TrieUnrolling(uint32_t *trie_node_arr, node* root){
    int arr_size = total_node_count + (total_node_count - 1);
    cout << "Making array of size: " << arr_size << endl;
    trie_node_arr = new uint32_t[arr_size];

    // TODO: remove ALPHA signatures
    g_test_local = new uint32_t[arr_size];

    queue<node*> trie_nodes;
    queue<int> parent;
    trie_nodes.push(root);
    parent.push(-1);
    int index = 0;
    int cur_parent = 0;
    int cur_child_count = 0;

    int g_test_child_node_count = 0;
    int g_test_index = 0;

    while (!trie_nodes.empty()){
        node* cur = trie_nodes.front(); trie_nodes.pop();
        int parent_index = parent.front(); parent.pop();
        trie_node_arr[index] = cur->map;
        if (parent_index >= 0){
            if (parent_index == cur_parent){
                cur_child_count += 1;
            }else{
                cur_child_count = 1;
                cur_parent = parent_index;
            }
            trie_node_arr[parent_index + cur_child_count] = index;
        }

        int n_children = __builtin_popcount(cur->map & UINT32_MAX_BIT_SET_COMPLEMENT);
        // cout << bitset<MAX_BITS>(cur->map) << endl;
        // cout << bitset<MAX_BITS>(trie_node_arr[index]) << endl;
        // cout << bitset<MAX_BITS>(cur->map & UINT32_MAX_BIT_SET_COMPLEMENT) << endl;
        // TODO: remove ALPHA signatures
        g_test_local[g_test_index] = cur->map;
        if (n_children){
            g_test_local[g_test_index+1] = (g_test_index/2+1 + trie_nodes.size())*2;
        }
        g_test_child_node_count += n_children;
        g_test_index += 2;


        for(int i=0; i<n_children; ++i){
            trie_nodes.push(&cur->next_v[i]);
            parent.push(index);
        }
        index+=n_children+1;
    }
    return trie_node_arr;
}


int sep_offset = 0;
int quantum_width = 1000; // TODO: source of error for nodes < 1000
uint32_t *arr_sep = nullptr;
uint32_t *arr_sep_nodes = nullptr;
uint32_t *arr_sep_children = nullptr;
uint32_t *arr_sep_quant = nullptr;
uint64_t *arr64 = nullptr;
uint32_t* ConstructLocalized(AdaptiveTrieNode* root, bool greater=false, bool perform_sort=false){
    queue<pair<AdaptiveTrieNode*, uint32_t>>* cur_l;
    queue<pair<AdaptiveTrieNode*, uint32_t>>* next_l;

    PairAdaptiveTrieNodeCompG compG = PairAdaptiveTrieNodeCompG();
    PairAdaptiveTrieNodeCompL compL = PairAdaptiveTrieNodeCompL();

    int print_iterations = 10;
    int arr_size = total_node_count + (total_node_count - 1);
    uint32_t *arr = new uint32_t[arr_size];

    queue<uint32_t>* sep_par = new queue<uint32_t>();
    sep_par->push(-1);
    // globals
    arr_sep = new uint32_t[arr_size];
    arr_sep_nodes = new uint32_t[total_node_count];
    arr_sep_children = new uint32_t[total_node_count-1];
    int index_sep = 0;
    int par_index_sep = -1;
    // global
    sep_offset = total_node_count;

    queue<uint32_t>* sep_quant_par = new queue<uint32_t>();
    sep_quant_par->push(-1);
    arr_sep_quant = new uint32_t[arr_size];
    int index_sep_quant = 0;
    int par_index_sep_quant = -1;

    arr64 = new uint64_t[total_node_count];
    for(int i=0; i<total_node_count; ++i){
      arr64[i] = 0;
    }

    next_l = new queue<pair<AdaptiveTrieNode*, uint32_t>>();
    cur_l = new queue<pair<AdaptiveTrieNode*, uint32_t>>();
    cur_l->push(pair<AdaptiveTrieNode*, uint32_t>({root, -1}));
    int index = 0;
    while(!cur_l->empty()){
        pair<AdaptiveTrieNode*, uint32_t> cur_p = cur_l->front();
        AdaptiveTrieNode* cur = cur_p.first;
        int par_index = cur_p.second;
        cur_l->pop();
        // TODO: implement level sorting
        arr[index] = cur->map;

        par_index_sep = sep_par->front(); sep_par->pop();
        par_index_sep_quant = sep_quant_par->front(); sep_quant_par->pop();

        arr_sep[index_sep] = cur->map;
        arr_sep_nodes[index_sep] = cur->map;
        arr64[index_sep] |= cur->map;
        
        // TODO: memory bug - malloc() memory corruption -> error overflow for arr_sep_quant
        // arr_sep_quant[index_sep_quant] = cur->map;

        if (par_index >= 0){
            // store index of child relative to parent, i.e. difference between parent and child indexes for future pointer optimization
            //arr[par_index+1] = index - par_index;
            arr[par_index+1] = index;

            arr_sep[par_index_sep + sep_offset] = index_sep;
            arr_sep_children[par_index_sep] = index_sep;
            arr64[par_index_sep] |= (uint64_t(index_sep) << MAX_BITS);
            // if (uint32_t(arr64[par_index_sep] >> MAX_BITS) != index_sep){
            //     cout << "\n\n\n\n@#$ anomally in child index, please note!" << endl << endl << endl << endl;
            // }
            // arr_sep_quant[par_index_sep_quant + quantum_width] = index_sep_quant;

        }

        int countc = 0;
        for(auto &a : cur->next_v){
            next_l->push(pair<AdaptiveTrieNode*, uint32_t>({a, countc++?-1:index}));

            // if (push_index>=0){
            sep_par->push(index_sep);
            // }else{
            //     sep_par->push(-1);
            // }
            sep_quant_par->push(index_sep_quant);

        }
        index += 2;
        index_sep += 1;
        // index_sep_quant += index_sep_quant%quantum_width?1:(quantum_width+1);

        if (cur_l->empty()){
            queue<pair<AdaptiveTrieNode*, uint32_t>>* temp = cur_l;

            if(perform_sort){
                // sorting next elements
                vector <pair<AdaptiveTrieNode*, uint32_t>> sort_buffer;
                while(!next_l->empty()){
                    sort_buffer.push_back(next_l->front());
                    next_l->pop();
                }
                if(greater)
                    sort(sort_buffer.begin(), sort_buffer.end(), compG);
                else
                    sort(sort_buffer.begin(), sort_buffer.end(), compL);

                // if (sort_buffer.size() > 1 && print_iterations)
                //     cout << "testing sorted next level: " << sort_buffer[0].first->accesses << " vs " << sort_buffer[1].first->accesses << endl;
                while(!sort_buffer.empty()){
                    // TODO: push_back and pop after ascending sort
                    next_l->push(sort_buffer.back());
                    // if(print_iterations){
                    //     cout << "testing sorting by accesses: " << sort_buffer.back().first->accesses << endl;
                    //     print_iterations--;
                    // }
                    sort_buffer.pop_back();
                }
            }
            cur_l = next_l;
            next_l = temp;
        }
    }
    return arr;
}

bool testCompareArr(uint32_t* localized_arr, uint32_t* arr){
    /*
    int quantum_width = 1000; // TODO: source of error for nodes < 1000
    uint32_t *arr_sep_quant = nullptr;
    */
    int indp = 0;
    int indn = 0;
    int ind_sepq = 0;
    uint32_t * indnl = localized_arr;
    for (int i=0; i<total_node_count; i++){
        // cout << "64 bit map:   " << bitset<MAX_BITS>(uint32_t(arr64[ind_sepq] & uint64_t((uint64_t(1)<<MAX_BITS) - 1))) << endl;
        // cout << "expected map: " << bitset<MAX_BITS>(arr[indp]) << endl;
        // cout << "exp map loc:  " << bitset<MAX_BITS>(*indnl) << endl;
        // cout << "64 child index BIT: " << bitset<MAX_BITS>(uint32_t(arr64[(arr64[ind_sepq] >> MAX_BITS)] & uint64_t((uint64_t(1)<<MAX_BITS) - 1))) << endl;
        // cout << "child index:        " << bitset<MAX_BITS>(arr[indp+1]<(total_node_count + total_node_count-1)?arr[arr[indp+1]]:-1) << endl;
        // cout << "exp map loc:        " << bitset<MAX_BITS>(*(indnl + (*(indnl + 1))))<< endl;
        // cout << "64 child : " << (arr64[ind_sepq] >> MAX_BITS) << endl;
        // cout << "loc child: " << indn + 1 << endl;
        // cout << endl;

        if (arr[indp] != (*indnl)){
            cout << "@# anomally in ordering - test!" << endl;
            cout << arr[indp] << " vs local: " << (*indnl) << endl;
        }if (arr[indp] != uint32_t(arr64[ind_sepq] & uint64_t((uint64_t(1)<<MAX_BITS) - 1))){
            cout << "@# anomally in ordering - 64 BIT test!" << endl;
            cout << arr[indp] << " vs local: " << arr64[ind_sepq] << endl;
            cout << (arr64[ind_sepq] & uint64_t((uint64_t(1)<<MAX_BITS) - 1)) << endl;
        }
        int n_child = __builtin_popcount(arr[indp] & UINT32_MAX_BIT_SET_COMPLEMENT);
    
        for (int j=0; j<n_child; ++j){
            if ((*(localized_arr + (*(indnl+1) + 2*j))) != arr[arr[indp+j+1]] ){
                cout << "@# anomally in children - test!" << endl;
            }
            int nxtind = (arr64[ind_sepq] >> MAX_BITS) + j;
            // if (!(nxtind % quantum_width)){
            //   nxtind += quantum_width;
            // }
            if ((arr64[nxtind]& uint64_t((uint64_t(1)<<MAX_BITS) - 1)) != arr[arr[indp+j+1]] ){
                cout << "@#@#@#@# anomally in children 64 BIT - Separated test!" << endl;
                cout << nxtind << endl;
                cout << (*(indnl+1) + 2*j)/2 << endl;
            }
        }
        indp += n_child + 1;
        indnl += 2;
        indn += 2;
        // ind_sepq += ind_sepq%quantum_width?1:(quantum_width+1);
        ind_sepq += 1;
    }
}

inline bool unrolledSearch(uint32_t* arr, const char *word){
    int index = 0;
    uint32_t _charIndexMask= 0;
    for (const char *ch=word; *ch != '\0'; ++ch){
        uint32_t &_map = arr[index];
        _charIndexMask = (1 << (*ch - 'a'));
        // cout << "letter : " << *ch << endl;
        // cout << "map:             " << bitset<MAX_BITS>(_map) << endl;
        // cout << "char index mask: " << bitset<MAX_BITS>(_charIndexMask) << endl;
        // cout << "cur index: " << index << endl;
        // cout << "n_prev children: " << __builtin_popcount(_map & (_charIndexMask - 1 )) << endl;
        // cout << "next index: " << arr[index + __builtin_popcount(_map & (_charIndexMask - 1 )) + 1] << endl;
        if (_map & _charIndexMask){
            index = arr[index + __builtin_popcount(_map & (_charIndexMask - 1 )) + 1];
        }else {
            // cout << "returning false pre word ending" << endl;
            return false;
        }
    }
    // cout << " --- word ending !!!! ---- " << endl;
    if (arr[index] & UINT32_MAX_BIT_SET)
        return true; // word ends here
    // cout << "returning false post!!!" << endl;
    return false;
}

inline bool localizedAdaptiveUnrolledSearch(uint32_t* localized_arr, const char *word){
    int index = 0;
    uint32_t* _map = localized_arr;
    // cout << "searching localized : " << word << endl;
    uint32_t _charIndexMask= 0;
    for (const char *ch=word; *ch != '\0'; ++ch){
        // cout << "looping ... ";
        // uint32_t &_map = localized_arr[index];
        // _map = localized_arr + index;
        _charIndexMask = (1 << (*ch - 'a'));
        // cout << "letter : " << *ch << endl;
        // cout << "map:             " << bitset<MAX_BITS>(*_map) << endl;
        // cout << "char index mask: " << bitset<MAX_BITS>(_charIndexMask) << endl;
        // // cout << "cur index: " << index << endl;
        // cout << "n_prev children: " << __builtin_popcount((*_map) & (_charIndexMask - 1 )) << endl;
        // cout << "n_prev children * 2 bitwise " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1) << endl;
        // cout << "n_prev children * 2 " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) * 2) << endl;
        // cout << "left child index: " << *(_map+1) << endl;
        // cout << "next index: " << ((*(_map+1)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1)) << endl;
        // TODO: pointer optimization: storing difference from index instead of child index to directly increment pointer
        if ((*_map) & _charIndexMask){
            _map = localized_arr + (*(_map+1)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1);
        }else {
            // cout << "returning false pre word ending" << endl;
            return false;
        }
    }
    // cout << " --- word ending !!!! ---- " << endl;
    // cout << "map:             " << bitset<MAX_BITS*2>(*_map) << endl;
    // cout << "map:             " << bitset<MAX_BITS*2>(UINT32_MAX_BIT_SET) << endl;
    // cout << "map:             " << bitset<MAX_BITS*2>(((*_map) & UINT32_MAX_BIT_SET)) << endl;
    // cout << " --- word ending !!!! ---- " << endl;
    if ((*_map) & UINT32_MAX_BIT_SET)
        return true; // word ends here
    // cout << "returning false post!!!" << endl;
    return false;
}

inline bool localizedAdaptiveUnrolledSearch64Bit(const char *word){
    int index = 0;
    uint64_t* _map = arr64;
    // cout << "searching (64 BIT) : " << word << endl;
    uint64_t _charIndexMask= 0;
    for (const char *ch=word; *ch != '\0'; ++ch){
        // cout << "looping ... ";
        // uint32_t &_map = localized_arr[index];
        // _map = localized_arr + index;
        _charIndexMask = (1 << (*ch - 'a'));
        // cout << "letter : " << *ch << endl;
        // cout << "map:             " << bitset<MAX_BITS*2>(*_map) << endl;
        // cout << "char index mask: " << bitset<MAX_BITS*2>(_charIndexMask) << endl;
        // // cout << "cur index: " << index << endl;
        // cout << "n_prev children: " << __builtin_popcount((*_map) & (_charIndexMask - 1 )) << endl;
        // cout << "n_prev children * 2 bitwise " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1) << endl;
        // cout << "n_prev children * 2 " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) * 2) << endl;
        // cout << "left child index: " << (*(_map) >> MAX_BITS) << endl;
        // cout << "next index: " << ((*(_map) >> MAX_BITS) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1)) << endl;
        // TODO: pointer optimization: storing difference from index instead of child index to directly increment pointer
        if ((*_map) & _charIndexMask){
            _map = arr64 + ((*(_map)) >> MAX_BITS) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )));
        }else {
            // cout << "returning false pre word ending" << endl;
            return false;
        }
    }
    // cout << " --- word ending !!!! ---- " << endl;
    // cout << "map:             " << bitset<MAX_BITS*2>(*_map) << endl;
    // cout << "map:             " << bitset<MAX_BITS*2>(UINT32_MAX_BIT_SET) << endl;
    // cout << "map:             " << bitset<MAX_BITS*2>(((*_map) & UINT32_MAX_BIT_SET)) << endl;
    // cout << "map:             " << bitset<MAX_BITS*2>(((*_map) & uint64_t(UINT32_MAX_BIT_SET))) << endl;
    if ((*_map) & UINT32_MAX_BIT_SET)
        return true; // word ends here
    // cout << "returning false post!!!" << endl;
    return false;
}

/// ******************* RESEARCH IMPLEMENTATIONS *********************** ///
// uint32_t* arr_sep,
inline bool localizedAdaptiveUnrolledOffsetSearch(const char *word){
    int index = 0;
    uint32_t* _map = arr_sep;
    /*
    uint32_t *arr_sep = nullptr;
    int sep_offset = 0;
    */
    uint32_t _charIndexMask= 0;
    for (const char *ch=word; *ch != '\0'; ++ch){
        // cout << "looping ... ";
        // uint32_t &_map = localized_arr[index];
        // _map = localized_arr + index;
        _charIndexMask = (1 << (*ch - 'a'));
        // cout << "letter : " << *ch << endl;
        // cout << "map:             " << bitset<MAX_BITS>(*_map) << endl;
        // cout << "char index mask: " << bitset<MAX_BITS>(_charIndexMask) << endl;
        // // cout << "cur index: " << index << endl;
        // cout << "n_prev children: " << __builtin_popcount((*_map) & (_charIndexMask - 1 )) << endl;
        // cout << "n_prev children * 2 bitwise " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1) << endl;
        // cout << "n_prev children * 2 " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) * 2) << endl;
        // cout << "left child index: " << *(_map+sep_offset) << endl;
        // cout << "next index: " << ((*(_map+sep_offset)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1)) << endl;
        // TODO: pointer optimization: storing difference from index instead of child index to directly increment pointer
        if ((*_map) & _charIndexMask){
            _map = arr_sep + (*(_map+sep_offset)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )));
        }else {
            // cout << "returning false pre word ending" << endl;
            return false;
        }
    }
    // cout << " --- word ending !!!! ---- " << endl;
    if ((*_map) & UINT32_MAX_BIT_SET)
        return true; // word ends here
    // cout << "returning false post!!!" << endl;
    return false;
}

// uint32_t* arr_sep_nodes, uint32_t* arr_sep_children,
inline bool localizedAdaptiveUnrolledSeparatedSearch(const char *word){
    int index = 0;
    uint32_t* _map = arr_sep_nodes;
    uint32_t* _child = arr_sep_children;
    int offset = 0;
    /*
    uint32_t *arr_sep_nodes = nullptr;
    uint32_t *arr_sep_children = nullptr;
    */
    uint32_t _charIndexMask= 0;
    for (const char *ch=word; *ch != '\0'; ++ch){
        // cout << "looping ... ";
        // uint32_t &_map = localized_arr[index];
        // _map = localized_arr + index;
        _charIndexMask = (1 << (*ch - 'a'));
        // cout << "letter : " << *ch << endl;
        // cout << "map:             " << bitset<MAX_BITS>(*_map) << endl;
        // cout << "char index mask: " << bitset<MAX_BITS>(_charIndexMask) << endl;
        // // cout << "cur index: " << index << endl;
        // cout << "n_prev children: " << __builtin_popcount((*_map) & (_charIndexMask - 1 )) << endl;
        // cout << "n_prev children * 2 bitwise " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1) << endl;
        // cout << "n_prev children * 2 " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) * 2) << endl;
        // cout << "left child index: " << *(_map+1) << endl;
        // cout << "next index: " << ((*(_map+1)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1)) << endl;
        // TODO: pointer optimization: storing difference from index instead of child index to directly increment pointer
        if ((*_map) & _charIndexMask){
            offset = (*(_child)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )));
            _map = arr_sep_nodes + offset;
            _child = arr_sep_children + offset;
        }else {
            // cout << "returning false pre word ending" << endl;
            return false;
        }
    }
    // cout << " --- word ending !!!! ---- " << endl;
    if ((*_map) & UINT32_MAX_BIT_SET)
        return true; // word ends here
    // cout << "returning false post!!!" << endl;
    return false;
}

//uint32_t* arr_sep_quant,
inline bool localizedAdaptiveUnrolledQuantumOffsetSearch(const char *word){
    int index = 0;
    uint32_t* _map = arr_sep_quant;
    /*
    int quantum_width = 1000; // TODO: source of error for nodes < 1000
    uint32_t *arr_sep_quant = nullptr;
    */
    int offset = 0;
    int n_child = 0;
    uint32_t _charIndexMask= 0;
    for (const char *ch=word; *ch != '\0'; ++ch){
        // cout << "looping ... ";
        // uint32_t &_map = localized_arr[index];
        // _map = localized_arr + index;
        _charIndexMask = (1 << (*ch - 'a'));
        // cout << "letter : " << *ch << endl;
        // cout << "map:             " << bitset<MAX_BITS>(*_map) << endl;
        // cout << "char index mask: " << bitset<MAX_BITS>(_charIndexMask) << endl;
        // // cout << "cur index: " << index << endl;
        // cout << "n_prev children: " << __builtin_popcount((*_map) & (_charIndexMask - 1 )) << endl;
        // cout << "n_prev children * 2 bitwise " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1) << endl;
        // cout << "n_prev children * 2 " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) * 2) << endl;
        // cout << "left child index: " << *(_map+1) << endl;
        // cout << "next index: " << ((*(_map+1)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1)) << endl;
        // TODO: pointer optimization: storing difference from index instead of child index to directly increment pointer
        if ((*_map) & _charIndexMask){
            n_child = (__builtin_popcount((*_map) & (_charIndexMask - 1 )));
            offset = (*(_map+quantum_width));
            _map = arr_sep_quant + (((offset)%quantum_width>(offset+n_child)%quantum_width)?(offset+n_child+quantum_width):(offset+n_child));
        }else {
            // cout << "returning false pre word ending" << endl;
            return false;
        }
    }
    // cout << " --- word ending !!!! ---- " << endl;
    if ((*_map) & UINT32_MAX_BIT_SET)
        return true; // word ends here
    // cout << "returning false post!!!" << endl;
    return false;
}

inline bool localizedAdaptiveUnrolledSearchPointRelative(uint32_t* localized_arr, const char *word){
    int index = 0;
    uint32_t* _map = localized_arr;
    /*
    Requires parent to store relative distance to child for it to work
    Post pointer math optimizations:
    */
    uint32_t _charIndexMask= 0;
    for (const char *ch=word; *ch != '\0'; ++ch){
        _charIndexMask = (1 << (*ch - 'a'));
        // cout << "letter : " << *ch << endl;
        // cout << "map:             " << bitset<MAX_BITS>(*_map) << endl;
        // cout << "char index mask: " << bitset<MAX_BITS>(_charIndexMask) << endl;
        // // cout << "cur index: " << index << endl;
        // cout << "n_prev children: " << __builtin_popcount((*_map) & (_charIndexMask - 1 )) << endl;
        // cout << "n_prev children * 2 bitwise " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1) << endl;
        // cout << "n_prev children * 2 " << (__builtin_popcount((*_map) & (_charIndexMask - 1 )) * 2) << endl;
        // cout << "left child index: " << *(_map+1) << endl;
        // cout << "next index: " << ((*(_map+1)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1)) << endl;
        // TODO: pointer optimization: storing difference from index instead of child index to directly increment pointer
        if ((*_map) & _charIndexMask){
            _map += (*(_map+1)) + (__builtin_popcount((*_map) & (_charIndexMask - 1 )) << 1);
        }else {
            return false;
        }
    }
    if ((*_map) & UINT32_MAX_BIT_SET)
        return true; // word ends here
    return false;
}

// toposort (to ensure parent is processed before child) with max heap sort based on number of times accessed
// do custom sorting routine instead of heapsort to achieve order stability, preserve order while neglecting low level fluctuations in accesses
// ngram optimization: cluster most frequent accessed ngrams paths
AdaptiveTrieNode* constructAdaptiveTrie(node* root){
    queue<node*> list;
    queue<pair<AdaptiveTrieNode*, int>> parent;
    AdaptiveTrieNode* newRoot = nullptr;
    list.push(root);
    parent.push(pair<AdaptiveTrieNode*, int>({nullptr, 0}));
    while(!list.empty()){
        node* cur = list.front();
        pair<AdaptiveTrieNode*, int> parentInf = parent.front();
        AdaptiveTrieNode* par = parentInf.first;
        uint8_t child_n = parentInf.second;
        list.pop(); parent.pop();

        AdaptiveTrieNode* newNode = new AdaptiveTrieNode({par, cur->map, child_n, 0, vector<AdaptiveTrieNode*>()});
        AdaptiveTrieNode_nodeCount++;
        global_list.push_back(newNode);

        if (!newRoot){
            newRoot = newNode;
        }
        if (par){
            par->next_v.push_back(newNode);
        }
        int child_count = 0;
        for(vector<node>::iterator it = cur->next_v.begin(); it != cur->next_v.end(); ++it, ++child_count){
            list.push(&*it);
            parent.push(pair<AdaptiveTrieNode*, int>({newNode, child_count}));
        }
    }
    return newRoot;
}

inline bool AdaptiveSearch(AdaptiveTrieNode* root, const char *word){
    for (const char *ch=word; *ch!='\0'; ++ch){
        // optimize char subtraction
        uint32_t bitmask = (1 << ((*ch) - 'a'));
        adaptive_search_mutex.lock();
        root->accesses += 1;
        adaptive_search_mutex.unlock();

        if ( !(root->map & bitmask)){
            return false;
        }
        root = (root->next_v[__builtin_popcount(root->map & (bitmask - 1))]);
    }
    if (root->map & UINT32_MAX_BIT_SET)
        return true; // word ends here
    return false;
}

uint32_t* AdaptiveTrieFlattening(uint32_t* trie_node_arr, AdaptiveTrieNode* root){
    AdaptiveTrieNodeComp comp = AdaptiveTrieNodeComp();
    make_heap(global_list.begin(), global_list.end(), comp);
    // // remove testing
    // for(int i=0; i<10; i++){
    //     AdaptiveTrieNode* maxNode = global_list.front();
    //     cout << " Max elem accesses : " << maxNode->accesses << endl;
    //     pop_heap(global_list.begin(), global_list.end(), AdaptiveTrieNodeComp()); global_list.pop_back();
    // }
    int arr_size = total_node_count + (total_node_count - 1);
    cout << "Making array of size: " << arr_size << endl;
    trie_node_arr = new uint32_t[arr_size];

    vector<AdaptiveTrieNode*> heap;
    heap.push_back(root);
    int index = 0;
    while (!heap.empty()){
        AdaptiveTrieNode* cur = heap.front();
        pop_heap(heap.begin(), heap.end(), comp); heap.pop_back();

        trie_node_arr[index] = cur->map;
        if (cur->accesses > 500000)
            cout << " Accesses value : " << cur->accesses << endl;

        parentIndex[cur] = index;
        if(cur->parent){
            trie_node_arr[parentIndex[cur->parent] + (cur->child_n + 1)] = index;
            if (!parentIndex[cur->parent]){
            }
        } else {
        }
        for (vector<AdaptiveTrieNode*>::iterator it=cur->next_v.begin(); it!= cur->next_v.end(); ++it){
            heap.push_back(*it); push_heap(heap.begin(), heap.end(), comp);
        }
        index += cur->next_v.size() + 1;
    }
    return trie_node_arr;
}

// TODO: implement, address of child in 2nd half of array
// TODO: implement, localized search sorted children by accesses
uint64* AdaptiveTrieFlattening_64bit(){}

vector<string> mutateList(vector<string> &list){
    vector<string> newlist;
    srand (time(NULL));
    int n_mutations = 1;
    int mx_acc_duplicates = 100;
    int mx_inc_duplicates = 20;
    std::default_random_engine generator;
    std::normal_distribution<double> acc_dist(0, mx_acc_duplicates);
    std::normal_distribution<double> inc_dist(0, mx_inc_duplicates);
    for (auto &a : list){
        newlist.push_back(a);
        int ori_duplicate_count = int(int(acc_dist(generator)) % mx_acc_duplicates);
        for (int i=0; i<ori_duplicate_count; ++i){
            newlist.push_back(a);
        }

        int mutate_duplicate_count = int(int(inc_dist(generator)) % mx_inc_duplicates);
        for(int i=0; i<mutate_duplicate_count; ++i){
            string _m(a.c_str());
            for (int j=0; j<n_mutations; j++){
                int ind = 0;
                if (_m.length() > 1)
                    ind = int(rand() % (_m.length() - 1));

                _m[ind] = char((((_m[ind] - 'a') + int(rand() % 26)) % 26) + 'a');
                if (!(_m[ind] >= 'a' && _m[ind] <= 'z')){
                    cout << "Alert while MUTATING! : " << _m[ind] << endl;
                }
            }
            newlist.push_back(_m);
        }
    }
    random_shuffle(newlist.begin(), newlist.end());
    return newlist;
}

void searchRoutine(node* root, vector<string> *list, int st, int en){
    for (int i=st; i<en; ++i){
        check_word_builtin(root, (*list)[i].c_str());
    }
}

void HashSearchRoutine(Trie* root, vector<string> *list, int st, int en){
    for (int i=st; i<en; ++i){
        search(root, (*list)[i]);
    }
}

void unrolledSearchRoutine(uint32_t *arr, vector<string> *list, int st, int en){
    for (int i=st; i<en; ++i){
        unrolledSearch(arr, (*list)[i].c_str());
    }
}

void addRoutine(node* root, vector<string> *list, int st, int en){
    vector <string> &vec = *list;
    for (int i=st; i<=en; ++i){
        add_word(root, vec[i].c_str());
    }
}

void adaptiveSearchRoutine(uint32_t *arr, vector<string> *list, int st, int en){
    for (int i=st; i<en; ++i){
        unrolledSearch(arr, (*list)[i].c_str());
    }
}

void localizedAdaptiveSearchRoutine(uint32_t* localized_arr, vector<string> *list, int st, int en, int type){
    if (type==0)
        for (int i=st; i<en; ++i){
            localizedAdaptiveUnrolledSearch(localized_arr, (*list)[i].c_str());
        }
    else if (type==1)
        for (int i=st; i<en; ++i){
            localizedAdaptiveUnrolledSearch64Bit((*list)[i].c_str());
            // localizedAdaptiveUnrolledSearchPointRelative(localized_arr, (*list)[i].c_str());
        }
}


/*
localizedAdaptiveUnrolledOffsetSearch(const char *word)
localizedAdaptiveUnrolledSeparatedSearch
localizedAdaptiveUnrolledQuantumOffsetSearch
*/
void localizedAdaptiveUnrolledOffsetRoutines(vector<string> *list, int st, int en, int type){
    // cout << " start offset routines" << endl;
    if (type==0){
        for (int i=st; i<en; ++i){
            localizedAdaptiveUnrolledOffsetSearch((*list)[i].c_str());
        }
    }else if (type==1){
        for (int i=st; i<en; ++i){
            localizedAdaptiveUnrolledSeparatedSearch((*list)[i].c_str());
        }
    }
    // cout << " end offset routines" << endl;
    // else {
    //     for (int i=st; i<en; ++i){
    //         localizedAdaptiveUnrolledQuantumOffsetSearch((*list)[i].c_str());
    //     }
    // }
}

// void localizedAdaptiveSearchRoutine64BitRoutine(vector<string> *list, int st, int en)

AdaptiveTrieNode* adaptiveAddRoutine(node* root, vector<string> &list, int &lim){
    AdaptiveTrieNode* a_root = constructAdaptiveTrie(root);

    for (int i=0; i<lim; ++i){
        AdaptiveSearch(a_root, list[i].c_str());
    }
    return a_root;
}

/*Driver function*/
int main () {
    vector <string> list;
    string line;
    int word_count = 0;
    ifstream myfile ("words_alpha_370k.txt");
    int word_limit = 10;
    if(LIMIT_WORD_LIST)
        cout << " Limiting word list to: " << word_limit << endl;
    if (myfile.is_open()){
        while ( getline (myfile,line) ){
            if(LIMIT_WORD_LIST && !--word_limit)
            {
                break;
            }
            char word[line.length()];
            strcpy(word, line.c_str());
            word[line.length()-1] = '\0';

            list.push_back(word);

            ++word_count;

            char * st = word;
             int count = 0;
             for (char* st = word; *st != '\0'; ++st, ++count){
                if (!((*st) >= 'a' && (*st) <= 'z')){
                    cout << "Alert while LOADING! : " << *st << endl;
                }
             }
        }
        myfile.close();
    } else cout << "Unable to open file";

    cout << "Word Count : " << word_count << endl;

    int lim = list.size();
    node start = {0, vector<node>()};
    Trie* root = nullptr;

    uint64 t_add_1_s = GetTimeMs64();
    for (int i=0; i<lim; ++i){
        add_word(&start, list[i].c_str());
    }
    uint64 t_add_1_e = GetTimeMs64();

    cout << "@@ time taken (single_core - HAMT construction) : " << t_add_1_e - t_add_1_s << endl;

    if (RUN_HASH_TRIE_BENCHMARK){
        uint64 t_add_2_s = GetTimeMs64();
        for (int i=0; i<lim; ++i){
            insert(root, list[i]);
        }
        uint64 t_add_2_e = GetTimeMs64();
        cout << "@@ time taken (single_core - HASH TRIE construction) : " << t_add_2_e - t_add_2_s << endl;
    }

    if (MUTATE_WORD_LIST){
        list = mutateList(list);
        cout << "Word Count after mutation : " << list.size() << endl;
        lim = list.size();
    }
    cout << "CREATED node count: " << total_node_count << endl;

    uint64 start_thread_t = GetTimeMs64();
    int n_threads = 11;
    thread* t_store[n_threads];
    int chunk_size = (lim + n_threads) / n_threads;
    uint64 std_hamt_time = 1;
    uint64 end_thread_t = 2;

    if (RUN_STD_HAMT_BENCHMARK){
        for (int i=0; i<n_threads; ++i){
            int st = (i * chunk_size);
            int end = ((i + 1) * chunk_size)-1;
            if (end >= lim)
                end = lim-1;
            if (st >= lim)
                st = lim-1;
            t_store[i] = new thread(searchRoutine, &start, &list, st, end);
        }
        for (int i=0; i<n_threads; ++i){
            t_store[i]->join();
            delete t_store[i];
            t_store[i] = nullptr;
        }

        end_thread_t = GetTimeMs64();
        std_hamt_time = end_thread_t - start_thread_t;
        cout << "@@ time taken - std hamt search (multithreaded) : " << std_hamt_time << endl;
    }

    uint32_t *arr = nullptr;

    uint64 t_u_s = GetTimeMs64();
    arr = TrieUnrolling(arr, &start);
    uint64 t_u_e = GetTimeMs64();
    cout << "finished trie unrolled construction" << endl;
    start_thread_t = GetTimeMs64();

    for (int i=0; i<n_threads; ++i){
        int st = (i * chunk_size);
        int end = ((i + 1) * chunk_size)-1;
        if (end >= lim)
            end = lim-1;
        if (st >= lim)
            st = lim-1;
        t_store[i] = new thread(unrolledSearchRoutine, arr, &list, st, end);
    }
    for (int i=0; i<n_threads; ++i){
        t_store[i]->join();
        delete t_store[i];
        t_store[i] = nullptr;
    }

    end_thread_t = GetTimeMs64();
    uint64 unrolled_hamt_time = end_thread_t - start_thread_t;
    cout << "@@ time taken - unrolled search (multithreaded) : " << unrolled_hamt_time << endl;
    cout << "@@ GAIN unrolled (multicore) over std hamt : " << double(unrolled_hamt_time) / double(std_hamt_time) * 100 << "%, and ratio: " << double(std_hamt_time) / double(unrolled_hamt_time) << endl;

    double hamt_bytes = double((total_node_count - 1) * 8 + 4 * total_node_count);
    int hamt_final_bytes = 4 * (total_node_count + (total_node_count - 1));
    int hash_bytes = trie_node_count + (trie_node_count - 1) * (1 + 8) + sizeof(new map<char, Trie*>()) * trie_node_count;

    cout << "Space Eff.: " << double(hash_bytes - hamt_final_bytes) / double(hash_bytes) * 100 << "% base, " << (hamt_bytes - hamt_final_bytes) / hamt_bytes * 100 << "% std hamt" << endl;

    AdaptiveTrieNode* a_root = nullptr;
    if (RUN_ADAPTIVE_BENCHMARK){
        a_root = adaptiveAddRoutine(&start, list, lim);
        uint32_t *adapt_arr = nullptr;
        uint64 t_am_s_s = GetTimeMs64();
        adapt_arr = AdaptiveTrieFlattening(adapt_arr, a_root);
        uint64 t_am_s_e = GetTimeMs64();
        cout << "Done Adaptive flatening!! time: " << t_am_s_e - t_am_s_s << endl;

        start_thread_t = GetTimeMs64();
        for (int i=0; i<n_threads; ++i){
            int st = (i * chunk_size);
            int end = ((i + 1) * chunk_size)-1;
            if (end >= lim)
                end = lim-1;
            if (st >= lim)
                st = lim-1;
            t_store[i] = new thread(adaptiveSearchRoutine, adapt_arr, &list, st, end);
        }
        for (int i=0; i<n_threads; ++i){
            t_store[i]->join();
            delete t_store[i];
            t_store[i] = nullptr;
        }

        end_thread_t = GetTimeMs64();
        double adaptive_hamt_time = double(end_thread_t - start_thread_t);
        cout << "@@ time taken - adaptive search (multithreaded) : " << adaptive_hamt_time << endl;
        cout << "@@ GAIN adaptive (multicore) over unrolled hamt : " << double(adaptive_hamt_time) / double(unrolled_hamt_time) * 100 << "%, and ratio: " << double(unrolled_hamt_time) / double(adaptive_hamt_time) << endl;
    }

    // single core localized search
    // TODO: PRIORITY affirm sorted clustering correctness
    // uint32_t* localized_arrG = ConstructLocalized(a_root, true, true);
    // uint32_t* localized_arrL = ConstructLocalized(a_root, false, true);
    uint32_t* localized_arr = ConstructLocalized(a_root, false, false);
    uint32_t* localized_iters[3];
    localized_iters[0] = localized_arr;
    // localized_iters[1] = localized_arr;
    // localized_iters[2] = localized_arr;

    if (RUN_LOCALIZED_CORRECTNESS_TEST){
        testCompareArr(localized_arr, arr);
        cout << endl << endl << "Running tests, lim: " << lim << endl << endl;
        int test_quant = lim/5;
        for (int i=0; i<lim; ++i){
            if (i%test_quant == 0){
                cout << "$$ 1M test run! check result..." << endl;
            }
            if (search(root, list[i]) != unrolledSearch(arr, list[i].c_str()))
             {cout << "@# baseline test failed! unrolled search!" << endl;
            }else if (i%test_quant == 0){
                cout << "$$   unrolled search baseline passed!" << endl;
            }
            //AdaptiveSearch(AdaptiveTrieNode* root, const char *word)
            if (search(root, list[i]) != AdaptiveSearch(a_root, list[i].c_str()))
            {cout << "@# baseline test failed! adaptive search!" << endl;
            }else if (i%test_quant == 0){
                cout << "$$   adaptive search baseline passed!" << endl;
            }
            if ( search(root, list[i]) != localizedAdaptiveUnrolledSearch(localized_arr, list[i].c_str())){
                cout << "@# test localizedAdaptiveUnrolledSearch failed!!!!" << endl;
            }else if(i%test_quant == 0){
                cout << "$$ 1M test passed! localizedAdaptiveUnrolledSearch" << endl;
            }
            if ( search(root, list[i]) != localizedAdaptiveUnrolledSearch64Bit(list[i].c_str())){
                cout << "@# test localizedAdaptiveUnrolledSearch  (64Bit) failed!!!!" << endl;
            }else if(i%test_quant == 0){
                cout << "$$ 1M test passed! localizedAdaptiveUnrolledSearch (64Bit)" << endl;
            }
            // if ( search(root, list[i]) != localizedAdaptiveUnrolledOffsetSearch(list[i].c_str())){
            //     cout << "@# test localizedAdaptiveUnrolledOffsetSearch RD failed!!!!" << endl;
            // }else if(i%test_quant == 0){
            //     cout << "$$ 1M test passed! localizedAdaptiveUnrolledOffsetSearch RD" << endl;
            // }
            // if ( search(root, list[i]) != localizedAdaptiveUnrolledSeparatedSearch(list[i].c_str())){
            //     cout << "@# test localizedAdaptiveUnrolledSeparatedSearch RD failed!!!!" << endl;
            // }else if(i%test_quant == 0){
            //     cout << "$$ 1M test passed! localizedAdaptiveUnrolledSeparatedSearch RD" << endl;
            // }
            // if ( search(root, list[i]) != localizedAdaptiveUnrolledQuantumOffsetSearch(list[i].c_str())){
            //     cout << "@# test localizedAdaptiveUnrolledQuantumOffsetSearch RD failed!!!!" << endl;
            // }else if(i%test_quant == 0){
            //     cout << "$$ 1M test passed! localizedAdaptiveUnrolledQuantumOffsetSearch RD" << endl;
            // }

        }
        cout << " ..... test ended ......" << endl << endl;
    }
    if (RUN_LOCALIZED_BENCHMARK){
        // multicore localized adaptive search
        cout << "# LOCALIZED 32 and 64 BIT order: greater, less, none" << endl;
        int localized_count = 2;
        for (int l=0; l<localized_count; ++l){
            // uint32_t* cur = localized_iters[0];
            start_thread_t = GetTimeMs64();
            for (int i=0; i<n_threads; ++i){
                int st = (i * chunk_size);
                int end = ((i + 1) * chunk_size)-1;
                if (end >= lim)
                    end = lim-1;
                if (st >= lim)
                    st = lim-1;
                t_store[i] = new thread(localizedAdaptiveSearchRoutine, localized_arr, &list, st, end, l);
            }
            for (int i=0; i<n_threads; ++i){
                t_store[i]->join();
                delete t_store[i];
                t_store[i] = nullptr;
            }
            end_thread_t = GetTimeMs64();
            double localized_hamt_time = double(end_thread_t - start_thread_t);
            cout << "@@ time taken - localized search (multithreaded) : " << localized_hamt_time << endl;
            cout << "@@ GAIN localized (multicore) over unrolled hamt : " << double(localized_hamt_time) / double(unrolled_hamt_time) * 100 << "%, and ratio: " << double(unrolled_hamt_time) / double(localized_hamt_time) << endl;
            cout << "@@ GAIN localized (multicore) over standard hamt : " << double(localized_hamt_time) / double(std_hamt_time) * 100 << "%, and ratio: " << double(std_hamt_time) / double(localized_hamt_time) << endl;
        }
    }

    if (RUN_LOCALIZED_RD_BENCHMARK){
        // multicore localized adaptive search
        cout << "# Localized RD order: offset, separated, quantum" << endl;
        int localized_count = 2;
        for (int l=0; l<localized_count; ++l){
            start_thread_t = GetTimeMs64();
            for (int i=0; i<n_threads; ++i){
                int st = (i * chunk_size);
                int end = ((i + 1) * chunk_size)-1;
                if (end >= lim)
                    end = lim-1;
                if (st >= lim)
                    st = lim-1;
                t_store[i] = new thread(localizedAdaptiveUnrolledOffsetRoutines, &list, st, end, l);
            }
            for (int i=0; i<n_threads; ++i){
                t_store[i]->join();
            }
            end_thread_t = GetTimeMs64();
            double localized_hamt_time = double(end_thread_t - start_thread_t);
            cout << "@@ time taken - localized search RD (multithreaded) : " << localized_hamt_time << endl;
            cout << "@@ GAIN localized RD (multicore) over unrolled hamt : " << double(localized_hamt_time) / double(unrolled_hamt_time) * 100 << "%, and ratio: " << double(unrolled_hamt_time) / double(localized_hamt_time) << endl;
            cout << "@@ GAIN localized RD (multicore) over standard hamt : " << double(localized_hamt_time) / double(std_hamt_time) * 100 << "%, and ratio: " << double(std_hamt_time) / double(localized_hamt_time) << endl;
        }
    }

    if (RUN_HASH_TRIE_BENCHMARK){
        start_thread_t = GetTimeMs64();
        for (int i=0; i<n_threads; ++i){
            int st = (i * chunk_size);
            int end = ((i + 1) * chunk_size)-1;
            if (end >= lim)
                end = lim-1;
            if (st >= lim)
                st = lim-1;
            t_store[i] = new thread(HashSearchRoutine, root, &list, st, end);
        }

        for (int i=0; i<n_threads; ++i){
            t_store[i]->join();
        }

        end_thread_t = GetTimeMs64();
        double hash_trie = double(end_thread_t - start_thread_t);
        cout << "@@ time taken - hash Trie search (multithreaded) : " << hash_trie << endl;
        cout << "@@ GAIN unrolled hamt trie (multicore) over hash Trie : " << double(unrolled_hamt_time) / double(hash_trie) * 100 << "%, and ratio: " << double(hash_trie) / double(unrolled_hamt_time) << endl;
    }

    return 0;
}

/*
    377 K words in dictionary
    9.76 M word queries in 30ms with localized adaptive static flattened HAMT (hexacore)


    Results: consistent 75% / 4.5 times gains over standard HAMT and 15-20 times over hashmap based Trie

    n_threads = 1

    @@ time taken - adaptive search (multithreaded) : 1.61972e+06
    @@ GAIN adaptive (multicore) over unrolled hamt : 84.4923%, and ratio: 1.18354
    # order: greater, less, none
    @@ time taken - localized search (multithreaded) : 1.60001e+06
    (Reference)
    @@ GAIN localized (multicore) over unrolled hamt : 83.464%, and ratio: 1.19812
    @@ GAIN localized (multicore) over standard hamt : 70.2865%, and ratio: 1.42275
    @@ time taken - localized search (multithreaded) : 1.61413e+06
    (Int)
    @@ GAIN localized (multicore) over unrolled hamt : 84.2006%, and ratio: 1.18764
    @@ GAIN localized (multicore) over standard hamt : 70.9068%, and ratio: 1.4103
    @@ time taken - localized search (multithreaded) : 1.6373e+06
    (Pointer)
    @@ GAIN localized (multicore) over unrolled hamt : 85.4091%, and ratio: 1.17084
    @@ GAIN localized (multicore) over standard hamt : 71.9245%, and ratio: 1.39035
    @@ time taken - hash Trie search (multithreaded) : 1.29708e+07
    @@ GAIN unrolled hamt trie (multicore) over hash Trie : 14.7795%, and ratio: 6.76615

    n_threads = 11

    @@ time taken - adaptive search (multithreaded) : 242833
    @@ GAIN adaptive (multicore) over unrolled hamt : 90.8568%, and ratio: 1.10063
    # order: greater, less, none
    @@ time taken - localized search (multithreaded) : 297410
    (Reference)
    @@ GAIN localized (multicore) over unrolled hamt : 111.277%, and ratio: 0.898658
    @@ GAIN localized (multicore) over standard hamt : 68.9006%, and ratio: 1.45137
    @@ time taken - localized search (multithreaded) : 306181
    (Int)
    @@ GAIN localized (multicore) over unrolled hamt : 114.559%, and ratio: 0.872915
    @@ GAIN localized (multicore) over standard hamt : 70.9325%, and ratio: 1.40979
    @@ time taken - localized search (multithreaded) : 382846
    (Pointer)
    @@ GAIN localized (multicore) over unrolled hamt : 143.243%, and ratio: 0.698114
    @@ GAIN localized (multicore) over standard hamt : 88.6934%, and ratio: 1.12748
    @@ time taken - hash Trie search (multithreaded) : 2.20277e+06
    @@ GAIN unrolled hamt trie (multicore) over hash Trie : 12.1334%, and ratio: 8.24174

*/
