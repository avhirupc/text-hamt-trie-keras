#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <queue>
// remove post testing
#include <bitset>
#include <fstream>
#include <bits/stdc++.h>
#include <algorithm>
#include <stdlib.h>     /* srand, rand */
#include <time.h> 
#include <random>
#include <thread>
#include <mutex>

using namespace std;

#define MAX_BITS (8*4)-1

typedef uint32_t byte_8; 

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <ctime>
#endif

/* Remove if already defined */
typedef long long int64; typedef unsigned long long uint64;

/* Returns the amount of milliseconds elapsed since the UNIX epoch. Works on both
 * windows and linux. */


uint64 GetTimeMs64()
{
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
     ret /= 1;

     /* Adds the seconds (10^0) after converting them to milliseconds (10^-3) */
     ret += (tv.tv_sec * 1000000);

     return ret;
    #endif
}



long long extra_byte_count = 0;
//TODO: testing: track byte allocation

// memory efficient trie
struct node {
    byte_8 map;
    // try int
    // node *next;
    //vector<char*> words;
    
    node ** next;
    vector <node> next_v;
    int count;  //TODO: count for testing only, remove

};

unordered_map<int, int> valToBitCount;
uint64_t total_node_count = 1;
mutex add_mutex;
mutex adaptive_add_mutex;
mutex adaptive_search_mutex;

struct node_h {
    //hash map

};

struct node_bh {
    // binary hash map

};

inline uint8_t get_set_bits_r_mapped(byte_8 &map, uint8_t n){
    return valToBitCount[map & ((1<<n) - 1)];
}

inline uint8_t get_set_bits_r(byte_8 &map, uint8_t n){
    //nth bit already set, find set bits to the right of nth bit
    // hash alternative
    byte_8 temp = map & ((1<<n) - 1);
    std::unordered_map<int, int>::iterator it = valToBitCount.find(temp);
    if (it != valToBitCount.end()){
        return it->second;
    }

    byte_8 test = (1 << n) - 1;
    // cout << "map : " << bitset<32>(map) << " , n: " << int(n) << " ; 1<<n : " << bitset<32>(test) << endl;
    // cout << "byte_8 : " << bitset<32>(temp)<<endl;

    uint8_t index = 0;
    while(temp){
        temp &= (temp - 1);
        ++index;
    }

    valToBitCount[temp] = index;

    return index;
}

//optimize
inline void expand_arr(node* &root, byte_8 &map, uint8_t &index){
    //TODO: testing: remove
    extra_byte_count += sizeof(node);
    // instead of iteratively expanding array, double it? 
    
    // // remove testing block, replace MAX_BITS with (8*4)-1
    // uint8_t len = get_set_bits_r(map, MAX_BITS);
    // cout << endl << endl << "settign len = " << int(len) << endl << endl;
    // if (!len){
    //         root->next = new node*[1];
    //         return; // return in inline statement?
    //     }
    //     node * n_next[len+1];
    // for (uint8_t i=0; i<index; ++i){
    //     cout << endl << endl << "setting i < index :" << int(i) << endl << endl;
    //     n_next[i] = root->next[i];
    // }
    // // merge loops with conditional?
    // for (uint8_t i=index+1; i<len+1; ++i){
    //     cout << endl << endl << "settign i = " << int(i) << "; value: " << root->next[i-1] << endl << endl;
    //     n_next[i] = root->next[i-1];
    // }
    // // delete[] root->next;
    // root->next = n_next;
    
    root->next_v.insert(root->next_v.begin() + index, node({0, nullptr, vector<node>(), 0}));
}

void add_word(node *root, const char * word){
    // transform word into unsigned char? change for loop for end
    // change pointers to references, avoid copying
    // check case
    // reject nullptr root
    // cout << " trie <1>: " << word << endl;

    // const char * st = word;
    //          int count = 0;
    //          for (const char* st = word; *st != '\0'; ++st, ++count){
    //             if (!((*st) >= 'a' && (*st) <= 'z')){
    //                 cout << "Alert! : " << *st << endl;
    //                 cout << *st - 'a' << endl;
    //                 cout << int(*st) << endl;
    //             }
    //          }

    // cout << "check done!" << endl;
    int count = 0;
    add_mutex.lock();
    for(const char * ch=word; *ch!='\0'; ++ch, ++count){
        if (!((*ch) >= 'a' && (*ch) <= 'z')){
            cout << endl << "# : " << (*ch) << " , Alert! char not in lowercase range" << endl;

        }
        // cout << "##### loop count : " << count << endl;
        // cout << " trie <1 char ASCII relative 'a'> : " << ((*ch) - 'a') << endl; 
        // cout << ", rootmap : " << bitset<32>(root->map) << endl; 
        // cout << " monsier: " << (root->map & (1 <<((*ch) - 'a')))  << endl; 
        // cout << " , popcount : " << __builtin_popcount(root->map & ((1 <<((*ch) - 'a')) - 1)) << endl;
        // optimize char subtraction
        
        // cout << "lol! 1" << endl;
        uint8_t index = __builtin_popcount(root->map & ((1 <<((*ch) - 'a')) - 1));//get_set_bits_r(root->map, ((*ch) - 'a'));
        if (!(root->map & (1 << ((*ch) - 'a')))){
            // cout << " trie <1 expanding>";
            // std::lock_guard<std::mutex> guard(g_pages_mutex);
            
            expand_arr(root, root->map, index);
            ++total_node_count;
            root->map |= 1 << ((*ch) - 'a');
            
            // cout << " trie <1 done>";
            // root->next[index] = new node({0, nullptr});
        }
        
        // cout << " trie <1 next loop>" << endl;
        root = &(root->next_v[index]);
        // cout << "lol! 2" << endl;
        
    }
    add_mutex.unlock();
    // cout << "exit" << endl;
    // root->count++;
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
        // optimize char subtraction
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
    return true;
}



// sorted_list (i, j boundaries)

// memory efficient / vs complexity efficient

struct Trie {
 
    // isEndOfWord is true if the node
    // represents end of a word
    bool isEndOfWord;
 
    /* nodes store a map to child node */
    unordered_map<char, Trie*> map;
};

int trie_node_count = 0;
 
/*function to make a new trie*/
Trie* getNewTrieNode()
{   trie_node_count++;
    Trie* node = new Trie;
    node->isEndOfWord = false;
    return node;
}
 
/*function to insert in trie*/
void insert(Trie*& root, const string& str)
{
    if (root == nullptr)
        root = getNewTrieNode();
 
    Trie* temp = root;
    for (int i = 0; i < str.length(); i++) {
        char x = str[i];
 
        /* make a new node if there is no path */
        if (temp->map.find(x) == temp->map.end())
            temp->map[x] = getNewTrieNode();
 
        temp = temp->map[x];
    }
 
    temp->isEndOfWord = true;
}
 
/*function to search in trie*/
bool search(Trie* root, const string& str)
{
    /*return false if Trie is empty*/
    if (root == nullptr)
        return false;
 
    Trie* temp = root;
    for (int i = 0; i < str.length(); i++) {
 
        /* go to next node*/
        temp = temp->map[str[i]];
 
        if (temp == nullptr)
            return false;
    }
 
    return temp->isEndOfWord;
}
 

//cache miss friendly implementation, convert nodes into a continuous array
uint32_t* TrieUnrolling(uint32_t *trie_node_arr, node* root){
    int arr_size = total_node_count + (total_node_count - 1);
    cout << "Making array of size: " << arr_size<< endl;
    trie_node_arr = new uint32_t[arr_size];
    queue<node*> trie_nodes;
    queue<int> parent;
    trie_nodes.push(root);
    parent.push(-1);
    int index = 0;
    int cur_parent = 0;
    int cur_child_count = 0;
    while (!trie_nodes.empty()){
        node* cur = trie_nodes.front(); trie_nodes.pop();
        int parent_index = parent.front(); parent.pop();
        trie_node_arr[index] = cur->map;
        if (parent_index > 0){
            if (parent_index == cur_parent){
                cur_child_count += 1;
            }else{
                cur_child_count = 1;
                cur_parent = parent_index;
            }
            trie_node_arr[parent_index + cur_child_count] = index; 
        }
        int n_children = __builtin_popcount(cur->map);
        for(int i=0; i<n_children; ++i){
            trie_nodes.push(&cur->next_v[i]);
            parent.push(index);
        }
        index+=n_children+1;
    }
    return trie_node_arr;
}

inline bool unrolledSearch(uint32_t* arr, const char *word){
    int index = 0;
    for (const char *ch=word; *ch != '\0'; ++ch){
        uint32_t &_map = arr[index];
        uint32_t _charIndexMask = (1 << (*ch - 'a'));
        if (_map & _charIndexMask){
            index = arr[index + __builtin_popcount(_map & (_charIndexMask - 1 )) + 1];
        }else {
            return false;
        }
    }
    return true;
}


////////////////////////// Dynamic adaptive hash trie
int AdaptiveTrieNode_nodeCount = 0;

struct AdaptiveTrieNode {
    AdaptiveTrieNode* parent;
    uint32_t map;
    uint8_t child_n;
    uint32_t accesses;
    vector<AdaptiveTrieNode*> next_v;
    // AdaptiveTrieNode(const node* root){

    // }
};
map<AdaptiveTrieNode*, int> parentIndex;
struct AdaptiveTrieNodeComp{
    bool operator()(const AdaptiveTrieNode* a,const AdaptiveTrieNode* b) const{
        // if (a->accesses == b->accesses){
        //     return parentIndex[a->parent] > parentIndex[b->parent];
        // }
        return a->accesses > b->accesses;
    }
};

vector<AdaptiveTrieNode*> global_list;

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
        
        // if (root->accesses)
        //     cout << "yeah! : " << root->accesses <<  endl;
        
        root = (root->next_v[__builtin_popcount(root->map & (bitmask - 1))]);
    }
    return true;
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
    cout << "root children size : " << root->next_v.size() << endl;

    int arr_size = total_node_count + (total_node_count - 1);
    cout << "Making array of size: " << arr_size<< endl;
    trie_node_arr = new uint32_t[arr_size];
    
    vector<AdaptiveTrieNode*> heap;
    heap.push_back(root);

    int index = 0;
    while (!heap.empty()){
        /*
            AdaptiveTrieNode* parent;
            uint32_t map;
            uint8_t child_n;
            uint32_t accesses;
            vector<AdaptiveTrieNode*> next_v;
        */
        AdaptiveTrieNode* cur = heap.front(); 
        pop_heap(heap.begin(), heap.end(), comp); heap.pop_back();

        trie_node_arr[index] = cur->map;
    
        if (cur->accesses > 500000)
            cout << " Accesses value : " << cur->accesses << endl;
        
        parentIndex[cur] = index;
        if(cur->parent){
            trie_node_arr[parentIndex[cur->parent] + (cur->child_n + 1)] = index;
            if (!parentIndex[cur->parent]){
                // cout << "parentIndex not found? " << parentIndex[cur->parent]<<endl; 
            }
        } else {
            // cout << index << " is root? " << (root == cur) << endl;
        }
        for (vector<AdaptiveTrieNode*>::iterator it=cur->next_v.begin(); it!= cur->next_v.end(); ++it){
            heap.push_back(*it); push_heap(heap.begin(), heap.end(), comp);
        }

        index += cur->next_v.size() + 1;

    }
    return trie_node_arr;

}

//////////////////////////

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
                    cout << _m[ind] - 'a' << endl;
                }
            }
            newlist.push_back(_m);
        }        
        // if (newlist.size() < 10){
        //     cout << "mutated string : " << _m << endl;
        //     cout << "lst char : " << _m[_m.length()-1] << endl;
        // }
    }

    random_shuffle(newlist.begin(), newlist.end());
    return newlist;
}

void searchRoutine(node* root, vector<string> *list, int st, int en){
    // uint64 start = GetTimeMs64();
    for (int i=st; i<en; ++i){
        // check_word(&start, list[i]);
        check_word_builtin(root, (*list)[i].c_str());
    }
    // uint64 end = GetTimeMs64();
    // appendSearchTime(double(end - start));
}

void unrolledSearchRoutine(uint32_t *arr, vector<string> *list, int st, int en){
    // uint64 start = GetTimeMs64();
    for (int i=st; i<en; ++i){
        // check_word(&start, list[i]);
        unrolledSearch(arr, (*list)[i].c_str());
    }
    // uint64 end = GetTimeMs64();
    // appendUnrolledSearchTime(double(end - start));
}

void addRoutine(node* root, vector<string> *list, int st, int en){
    /*
        mutex adaptive_add_mutex;
        mutex adaptive_search_mutex;
    */
    // for convenience
    vector <string> &vec = *list;
    // uint64 start = GetTimeMs64();
    // cout << "thread starting ... " << st << ", " << en << endl;
    for (int i=st; i<=en; ++i){
        add_word(root, vec[i].c_str());
    }
    // uint64 end = GetTimeMs64();
    // appendAddTime(double(end - start));
}

void adaptiveSearchRoutine(uint32_t *arr, vector<string> *list, int st, int en){
    // uint64 start = GetTimeMs64();
    for (int i=st; i<en; ++i){
        // check_word(&start, list[i]);
        unrolledSearch(arr, (*list)[i].c_str());
    }
    // uint64 end = GetTimeMs64();
    // appendAdaptiveSearchTime(double(end - start));
}

AdaptiveTrieNode* adaptiveAddRoutine(node* root, vector<string> &list, int &lim){
    /*
        mutex adaptive_add_mutex;
        mutex adaptive_search_mutex;
    */
    // uint64 start = GetTimeMs64();
    AdaptiveTrieNode* a_root = constructAdaptiveTrie(root);

    for (int i=0; i<lim; ++i){
        AdaptiveSearch(a_root, list[i].c_str());
    }
    // uint64 end = GetTimeMs64();
    // appendAdaptiveAddTime(double(end - start));
    return a_root;
}

/*Driver function*/
int main () {
    vector <string> list;
    // std::thread thread_object(callable)

        string line;
        int word_count = 0;
      ifstream myfile ("words_alpha_370k.txt");
      if (myfile.is_open())
      {
        while ( getline (myfile,line) )
        {   char word[line.length()];
            strcpy(word, line.c_str()); 
            word[line.length()-1] = '\0';
            list.push_back(word);
            ++word_count;


            char * st = word;
             int count = 0;
             for (char* st = word; *st != '\0'; ++st, ++count){
                if (!((*st) >= 'a' && (*st) <= 'z')){
                    cout << "Alert while LOADING! : " << *st << endl;
                    cout << *st - 'a' << endl;
                    cout << int(*st) << endl;
                }
             }

        }
        myfile.close();
      }

      else cout << "Unable to open file"; 

    cout << "Word Count : " << word_count << endl;
    list = mutateList(list);
    cout << "Word Count after mutation : " << list.size() << endl;

    // char ** listed = new char *[] ({new char[]("tester"), new char[]("tester")});
    // char * list[] = {"tester", "tested", "geeks", "for", "geekk", "gee", "science", }; 
    int lim = list.size();

    node start = {0, nullptr};
    Trie* root = nullptr;
    


    start = {0, nullptr};
    uint64 t_add_1_s = GetTimeMs64();
    for (int i=0; i<lim; ++i){
        // cout << " adding word: " << list[i] << endl;
        add_word(&start, list[i].c_str());
    }
    
    uint64 t_add_1_e = GetTimeMs64();

    cout << "time taken (single_core) : " << t_add_1_e - t_add_1_s << endl;
    cout << total_node_count << endl;
    

/////////////////////////////////////


uint64 start_thread_t = GetTimeMs64();


    int n_threads = 11; 
    thread* t_store[n_threads];

    // int adders = 0; // thread protected
    // int readyToAdaptiveExtension = false; // thread protected

    
    int chunk_size = (lim + n_threads) / n_threads;
    cout << chunk_size << endl;
    // int i =0;
    // addRoutine(&start, list, (i * chunk_size), (((i + 1) * chunk_size)%lim));
    // addRoutineDum(start);
    // cout << list[9765964] << endl;
    for (int i=0; i<n_threads; ++i){
        // addRoutine(node* root, vector<string> &list, int st, int en)
        // searchRoutine(node* root, vector<string> &list, int st, int en)
        int st = (i * chunk_size);
        int end = ((i + 1) * chunk_size)-1;
        // cout << st << ", " << end << endl;
        if (end >= lim)
            end = lim-1;
        if (st >= lim)
            st = lim-1;
        // cout << st << ", " << end << endl;
        t_store[i] = new thread(searchRoutine, &start, &list, st, end);
        // t_store[i] = new thread(addRoutineDum, &start, &list);
    }
    for (int i=0; i<n_threads; ++i){
        t_store[i]->join();
    }

    uint64 end_thread_t = GetTimeMs64();
    uint64 std_hamt_time = end_thread_t - start_thread_t;
    cout << "time taken - std hamt search (multithreaded) : " << std_hamt_time << endl;
    cout << total_node_count << endl;



    //////////////////////////


/////////////////////////////////////

    uint32_t *arr = nullptr;

    uint64 t_u_s = GetTimeMs64();
    arr = TrieUnrolling(arr, &start);
    uint64 t_u_e = GetTimeMs64();


    start_thread_t = GetTimeMs64();


    // int n_threads = 11; 
    // thread* t_store[n_threads];

    // int adders = 0; // thread protected
    // int readyToAdaptiveExtension = false; // thread protected

    
    chunk_size = (lim + n_threads) / n_threads;
    cout << chunk_size << endl;
    // int i =0;
    // addRoutine(&start, list, (i * chunk_size), (((i + 1) * chunk_size)%lim));
    // addRoutineDum(start);
    // cout << list[9765964] << endl;
    for (int i=0; i<n_threads; ++i){
        // unrolledSearchRoutine(uint32_t *arr, vector<string> *list, int st, int en){
        int st = (i * chunk_size);
        int end = ((i + 1) * chunk_size)-1;
        // cout << st << ", " << end << endl;
        if (end >= lim)
            end = lim-1;
        if (st >= lim)
            st = lim-1;
        // cout << st << ", " << end << endl;
        t_store[i] = new thread(unrolledSearchRoutine, arr, &list, st, end);
        // t_store[i] = new thread(addRoutineDum, &start, &list);
    }
    for (int i=0; i<n_threads; ++i){
        t_store[i]->join();
    }

    end_thread_t = GetTimeMs64();
    uint64 unrolled_hamt_time = end_thread_t - start_thread_t;
    cout << "time taken - unrolled search (multithreaded) : " << unrolled_hamt_time << endl;
    cout << total_node_count << endl;
    cout << "\n\n@@ GAIN unrolled (multicore) : " << double(std_hamt_time - unrolled_hamt_time) / double(std_hamt_time) * 100 << "%, and ratio: " << double(std_hamt_time) / double(unrolled_hamt_time) << "\n\n" << endl;



    //////////////////////////


    double hamt_bytes = double((total_node_count - 1) * 8 + 4 * total_node_count);
    int hamt_final_bytes = 4 * (total_node_count + (total_node_count - 1));
    int hash_bytes = trie_node_count + (trie_node_count - 1) * (1 + 8) + sizeof(new map<char, Trie*>()) * trie_node_count;

    cout << "Space Efficiency: " << double(hash_bytes - hamt_final_bytes) / double(hash_bytes) * 100 << "%% over base, " << (hamt_bytes - hamt_final_bytes) / hamt_bytes * 100 << "%% over std hamt" << endl;


    //////////////////// adaptive tries experiments



    AdaptiveTrieNode* a_root = adaptiveAddRoutine(&start, list, lim);

    cout << "Adaptive Node Count : " << AdaptiveTrieNode_nodeCount << endl;

    uint32_t *adapt_arr = nullptr; 

    uint64 t_am_s_s = GetTimeMs64();
    adapt_arr = AdaptiveTrieFlattening(adapt_arr, a_root);
    uint64 t_am_s_e = GetTimeMs64();
    cout << "Done Adaptive flatening!! time: " << t_am_s_e - t_am_s_s << endl;
    


/////////////////////////////////////


    start_thread_t = GetTimeMs64();


    // int n_threads = 11; 
    // thread* t_store[n_threads];

    // int adders = 0; // thread protected
    // int readyToAdaptiveExtension = false; // thread protected

    
    chunk_size = (lim + n_threads) / n_threads;
    cout << chunk_size << endl;
    // int i =0;
    // addRoutine(&start, list, (i * chunk_size), (((i + 1) * chunk_size)%lim));
    // addRoutineDum(start);
    // cout << list[9765964] << endl;
    for (int i=0; i<n_threads; ++i){
        // adaptiveSearchRoutine(uint32_t *arr, vector<string> *list, int st, int en)
        int st = (i * chunk_size);
        int end = ((i + 1) * chunk_size)-1;
        // cout << st << ", " << end << endl;
        if (end >= lim)
            end = lim-1;
        if (st >= lim)
            st = lim-1;
        // cout << st << ", " << end << endl;
        t_store[i] = new thread(adaptiveSearchRoutine, adapt_arr, &list, st, end);
        // t_store[i] = new thread(addRoutineDum, &start, &list);
    }
    for (int i=0; i<n_threads; ++i){
        t_store[i]->join();
    }

    end_thread_t = GetTimeMs64();
    double adaptive_hamt_time = double(end_thread_t - start_thread_t);
    cout << "time taken - adaptive search (multithreaded) : " << adaptive_hamt_time << endl;
    cout << total_node_count << endl;
    cout << "\n\n@@ GAIN adaptive (multicore) : " << (double(unrolled_hamt_time) - adaptive_hamt_time) / double(unrolled_hamt_time) * 100 << "%, and ratio: " << double(unrolled_hamt_time) / double(adaptive_hamt_time) << "\n\n" << endl;



    //////////////////////////    Single Core Code

    // uint64 t_add_2_s = GetTimeMs64();
    // for (int i=0; i<lim; ++i){
    //     // insert(root, list[i]);
    // }
    // uint64 t_add_2_e = GetTimeMs64();

    // // uint64 t_s_1_s = GetTimeMs64();
    // // for (int i=0; i<lim; ++i){
    // //     // check_word(&start, list[i].c_str());
    // //     //check_word_mapped
    // // }
    // // uint64 t_s_1_e = GetTimeMs64();

    // uint64 t_s_m_s = GetTimeMs64();
    // for (int i=0; i<lim; ++i){
    //     // check_word(&start, list[i]);
    //     // check_word_mapped(&start, list[i].c_str());
    // }
    // uint64 t_s_m_e = GetTimeMs64();


    // uint64 t_s_b_s = GetTimeMs64();
    // for (int i=0; i<lim; ++i){
    //     // check_word(&start, list[i]);
    //     // check_word_builtin(&start, list[i].c_str());
    // }
    // uint64 t_s_b_e = GetTimeMs64();


    // cout << "time taken (single_core) : " << t_s_b_e - t_s_b_s << endl;
    // cout << total_node_count << endl;




    // uint64 t_s_2_s = GetTimeMs64();
    // for (int i=0; i<lim; ++i){
    //     // search(root, list[i]);
    // }
    // uint64 t_s_2_e = GetTimeMs64();


    // uint64 t_u_s_s = GetTimeMs64();
    // for (int i=0; i<lim; ++i){
    //     // unrolledSearch(arr, list[i].c_str());
    // }
    // uint64 t_u_s_e = GetTimeMs64();


    // cout << "time taken - unrolled search (single_core) : " << t_u_s_e - t_u_s_s << endl;
    // cout << total_node_count << endl;




    // cout << "Add times @Custom: " << t_add_1_e - t_add_1_s;
    // cout << ", @Hash: " << t_add_2_e - t_add_2_s << endl;
    // // cout << " @Custom: " << t_s_1_e - t_s_1_s; 
    // cout << "Search times @Hash: " << t_s_2_e - t_s_2_s << endl;
    // cout << "Search times @Custom_Mapped: " << t_s_m_e - t_s_m_s << ", @Custom_BuiltIn: " << t_s_b_e - t_s_b_s << endl;
    // cout << "Total Node Count: "<< total_node_count << ", byte count: " << extra_byte_count << ", Hashed trie node count: " << trie_node_count << endl;

    // cout << "Trie Unrolling time: " << t_u_e - t_u_s << ", Search time: " << t_u_s_e - t_u_s_s << endl;

    // double base = double(t_s_2_e - t_s_2_s);
    // double hamt = double(t_s_b_e - t_s_b_s);
    // double hamt_unrolled = double(t_u_s_e - t_u_s_s);

    // cout << "%% search gain over base : " << (base -hamt_unrolled)/base * 100 << "%, over std hamt: " <<  (hamt -hamt_unrolled)/hamt * 100 << "% " << endl;


    // uint64 t_aa_s_s = GetTimeMs64();
    // for (int i=0; i<lim; ++i){
    //     // unrolledSearch(adapt_arr, list[i].c_str());
        
    //     // checks for correctness
    //     // if ((unrolledSearch(arr, list[i].c_str()) !=  unrolledSearch(adapt_arr, list[i].c_str()))){
    //     //     cout << "@error! " << endl;
    //     // }
    // }
    // uint64 t_aa_s_e = GetTimeMs64();

    // double adaptive_hamt = double(t_aa_s_e - t_aa_s_s);

    // cout << "time taken - adaptive search (single_core) : " << t_aa_s_e - t_aa_s_s << endl;
    // cout << total_node_count << endl;



    // cout << "Adaptive Search times: " << t_aa_s_e - t_aa_s_s << endl;
    // cout << "gain :: " << (hamt_unrolled - adaptive_hamt) / hamt_unrolled * 100 << "%%" << endl;
    // cout << "@Net gain : " << (hamt - adaptive_hamt)/hamt * 100 << endl;

    return 0; 
}

/*   Results: consistent 75% / 4.5 times gains over standard HAMT and 92%+ over hashmap based Trie

@@ GAIN unrolled (multicore) : 75.7913%, and ratio: 4.13074
@@ GAIN adaptive (multicore) : -149.631%, and ratio: 0.400591


Word Count : 370099
Word Count after mutation : 9765965
Making array of size: 13441935
Add times @Custom: 6408142, @Hash: 24613700
Search times @Hash: 14804479
Search times @Custom_Mapped: 23075, @Custom_BuiltIn: 3659892
Total Node Count: 6720968, byte count: 322606416, Hashed trie node count: 6720968
Trie Unrolling time: 1106207, Search time: 920972
%% search gain over base : 93.7791%, over std hamt: 74.8361% 
Space Efficiency: 55.5556%% over base, 33.3333%% over std hamt
Adaptive Node Count : 6720968
root children size : 26
Making array of size: 13441935
0 is root? 1
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
parentIndex not found? 0
Done Adaptive flatening!! time: 17055127
Adaptive Search times: 2260474
gain :: -145.444%%
@Net gain : 38.2366



*/