## optimized compressed/adaptive multicore hamt (Hash Array Mapped Trie) implementation in cpp
## keras RNN (GRU) and CNN for text analysis

Compressed Flattening of hamt provides multicore search speed gains of 4-5 times and reduces memory overhead by 33% over standard struct-based hamt implementation and provides speed gains of 15-20 times, while taking up 1/2 the space over hash-map based tries. 
https://en.wikipedia.org/wiki/Hash_array_mapped_trie

The benchmarks are run on a hexacore with 370k unique word dict which is mutated via Gaussian distributions to 9M+ words.

HAMT Hash array mapped tries use bitmaps to reference children instead of standard hashmaps. We basically use hamming distance to find index of child in array which is implemented by low level CPU instructions (popcount in GCC) and is very fast. It trades linear add times (as array need to be expanded everytime) vs speed and memory gains and is ideal for lookups over persistent and static structures like autocorrects, etc. This implementation optimizes it by compressing the struct based hamt into a primitive array for reducing cache misses which speeds it up by a factor of 4-5 (the search is a multicore implementation). Please note that the cpp code is pretty messy as of now and cleaning it is a todo.

Also contained are experimentations with an adaptive flattening implementation which clusters nodes in the flattened 1D array via a priority queue on the basis of number of expected accesses to see if it could further optimize cache misses by clustering most frequently used tree paths together. Developing the right heuristic for the same is a work in progress.



RNN (GRU) sentiment analysis uses IMDB data which can be downloaded here:
http://ai.stanford.edu/~amaas/data/sentiment/aclImdb_v1.tar.gz
