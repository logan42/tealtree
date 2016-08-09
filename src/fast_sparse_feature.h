#ifndef __tealtree__FAST_SPARSE_feature__
#define __tealtree__FAST_SPARSE_feature__

#include "buffer.h"
#include "sparse_feature.h"
#include "thread_specific_ptr.h"

#include <mutex>
#include <stdio.h>


struct FastSparseFeatureBuffer
{
    typedef VarIntBuffer<DOC_ID, uint8_t, DOC_ID> VIB;
    Buffer buffer;
    VIB vib;
};

struct FastShardMapping
{
    typedef uint32_t SHARD_ID_TYPE;
    static const SHARD_ID_TYPE NULL_SHARD = std::numeric_limits<SHARD_ID_TYPE>::max();
    static const SHARD_ID_TYPE FINAL_FAKE_SHARD = 1;
    DOC_ID fixed_tail_size = 16;
    float_t initial_tail_size = (float_t)0.05; // Overridden by --initial_tail_size command line argument.
    bool sparse_v1; // controlled by --sparse_feature_version command line argument.

    // Maps tree node id to shard id
    std::vector<SHARD_ID_TYPE> nodes_to_shards;

    // Maps shard ID to tree node.
    std::vector<TreeNode *> shards_to_nodes;

    std::vector<SHARD_ID_TYPE> next_shard, previous_shard;

    thread_specific_ptr<FastSparseFeatureBuffer> tss_buffer;

    FastShardMapping()
    {
    }

    static inline FastShardMapping & get_instance()
    {
        return instance;
    }

    void split_tree_node(const TreeNode * node)
    {
        uint32_t parent_id = node->node_id;
        uint32_t left_id = node->left->node_id;
        uint32_t right_id = node->right->node_id;
        assert(right_id == left_id + 1);
        // Make sure we haven't heard of the children nodes here before.
        assert(left_id >= this->nodes_to_shards.size());
        // Make sure that we've heard of all the other nodes prior to left_id.
        //assert(left_id == this->nodes_to_shards.size());
                    
                    SHARD_ID_TYPE old_shard = this->nodes_to_shards[parent_id];
                    assert(this->next_shard.size() == this->previous_shard.size());
                    assert(this->next_shard.size() == this->shards_to_nodes.size());
                    SHARD_ID_TYPE new_shard = this->next_shard.size();
                    this->nodes_to_shards.resize(right_id + 1, NULL_SHARD);
                    this->nodes_to_shards[left_id] = old_shard;
                    this->nodes_to_shards[right_id] = new_shard;
                    this->shards_to_nodes[old_shard] = node->left;
                    assert(new_shard == this->shards_to_nodes.size());
                    this->shards_to_nodes.push_back(node->right);

                    SHARD_ID_TYPE following_shard = this->next_shard[old_shard];
                    // Inserting new_shard into the linked list between old_shard and following_shard.
                    this->next_shard.push_back(following_shard);
                    this->next_shard[old_shard] = new_shard;
                    this->previous_shard[following_shard] = new_shard;
                    this->previous_shard.push_back(old_shard);
    }

    void on_start_new_tree(TreeNode * root)
    {
        assert(this->nodes_to_shards.empty());
        assert(this->shards_to_nodes.empty());
        assert(this->previous_shard.empty());
        assert(this->next_shard.empty());
        this->nodes_to_shards.push_back(0);
        this->shards_to_nodes.push_back(root);
        this->shards_to_nodes.push_back(nullptr);
        this->next_shard.push_back(1);
        this->next_shard.push_back(NULL_SHARD);
        this->previous_shard.push_back(NULL_SHARD);
        this->previous_shard.push_back(0);
    }

    void on_finalize_tree()
    {
        nodes_to_shards.clear();
        shards_to_nodes.clear();
        previous_shard.clear();
        next_shard.clear();
    }

inline FastSparseFeatureBuffer * get_buffer()
    {
        FastSparseFeatureBuffer * buffer = tss_buffer.get();
        if (buffer == nullptr) {
            buffer = new FastSparseFeatureBuffer();
            tss_buffer.reset(buffer);
        }
        return buffer;
    }
private:
    static FastShardMapping instance;
};


struct Shard
{
    DOC_ID v_ptr;
    DOC_ID o_ptr;
        DOC_ID tail;

        Shard(DOC_ID v_ptr, DOC_ID o_ptr, DOC_ID tail)
            :v_ptr(v_ptr),
            o_ptr(o_ptr),
            tail(tail)
        {}
};

//#define FAST_SPARSE_FEATURE_DEBUG

template<const uint8_t BITS>
class FastSparseFeatureImpl : public SparseFeatureImpl<BITS>
{
protected:
    typedef  typename FastShardMapping::SHARD_ID_TYPE SHARD_ID_TYPE;
    typedef CompactVector<BITS> CV;
    typedef typename CV::ValueType ValueType;
    typedef VarIntBuffer<DOC_ID, uint8_t, DOC_ID> VIB;
    std::vector<Shard> shards;
    FastShardMapping * map;
#ifndef NDEBUG
    std::string values_md5;
    std::string offsets_md5;
#endif
#ifdef FAST_SPARSE_FEATURE_DEBUG
    std::vector<ValueType> debug_values;
    std::vector<DOC_ID> debug_doc_ids;
    void validate_shards();
#else
    inline void validate_shards() {}
#endif

    struct ShardCursor
    {
        typename CV::Iterator  value_it;
        VIB::Iterator offset_it;
        const std::vector<DOC_ID> * node_doc_ids;
        DOC_ID n_docs_left;
        DOC_ID current_relative_id;
        DOC_ID current_doc_id;
        ValueType current_value;

inline ShardCursor(CV & cv, VIB & offsets, Shard & shard, DOC_ID n_docs, TreeNode * node)
    : value_it(cv.iterator(shard.v_ptr)),
    offset_it(offsets.iterator(shard.o_ptr)),
    node_doc_ids(&node->doc_ids),
    n_docs_left(n_docs + 1),
    current_relative_id(0)
{
    this->next();
}

inline void next()
{
    assert(this->n_docs_left > 0);
    if (0 == --this->n_docs_left) {
        return;
    }
    this->current_relative_id += this->offset_it.next();
    this->current_doc_id = (*this->node_doc_ids)[current_relative_id];
    this->current_value = this->value_it.next();
}
    };

    struct ShardCursorCompare
    {
        inline bool operator()
            (const ShardCursor * pc1, const ShardCursor * pc2) const
        {
            const ShardCursor & c1 = *pc1;
            const ShardCursor & c2 = *pc2;
            const bool t = false;
            if (c1.n_docs_left == 0) {
                return t;
            }
            if (c2.n_docs_left == 0) {
                return !t;
            }
            if (c1.current_doc_id > c2.current_doc_id) {
                return !t;
            }
            return t;
        }
    };

public:
    FastSparseFeatureImpl()
        : SparseFeatureImpl<BITS>()
    {
        map = &FastShardMapping::get_instance();
    }
    virtual ~FastSparseFeatureImpl() {};
    void virtual init_from_raw_histogram(const RawFeatureHistogram * hist);
    virtual std::unique_ptr<SplitSignature> get_split_signature(TreeNode * leaf, Split * split);
    virtual std::unique_ptr<Histogram> compute_histogram(const TreeNode * leaf, bool newton_step);
    template<const bool NEWTON_STEP>
    inline std::unique_ptr<Histogram> compute_histogram_impl(const TreeNode * leaf);
    virtual void on_finalize_tree();
private:
    DOC_ID rearrange_shards(SHARD_ID_TYPE  shard, SHARD_ID_TYPE left_neighbor, SHARD_ID_TYPE right_neighbor, DOC_ID required_space);
    inline void resize_offsets(DOC_ID shortage);
    inline DOC_ID shard_size(SHARD_ID_TYPE shard, bool with_tail);
};

#endif /* defined(__tealtree__FAST_SPARSE_feature__) */
