#include "fast_sparse_feature.h"

#include "log_trivial.h"
#include "trainer_data.h"

#include <gheap.hpp>

const FastShardMapping::SHARD_ID_TYPE FastShardMapping::NULL_SHARD;
const FastShardMapping::SHARD_ID_TYPE FastShardMapping::FINAL_FAKE_SHARD;
FastShardMapping FastShardMapping ::instance;

template<const uint8_t BITS>
void FastSparseFeatureImpl <BITS>::init_from_raw_histogram(const RawFeatureHistogram * hist)
{
    SparseFeatureImpl<BITS>::init_from_raw_histogram(hist);
    if (map->sparse_v1) {
        return;
    }
    assert(this->shards.size() == 0);
    DOC_ID initial_tail = (DOC_ID)(map->initial_tail_size * this->offsets.size());
    initial_tail = std::max(initial_tail, map->fixed_tail_size);
    SparseFeatureImpl<BITS>::offsets.append_tail(initial_tail);
    shards.push_back(Shard(0, 0, initial_tail));
    shards.push_back(Shard(this->cv.size(), this->offsets.size(), 0));

#ifndef NDEBUG
    this->values_md5 = this->cv.md5();
    this->offsets_md5 = this->offsets.md5(0, this->shards[1].o_ptr - this->shards[0].tail);
#endif
#ifdef FAST_SPARSE_FEATURE_DEBUG
    DOC_ID n_docs = this->cv.size();
    this->debug_doc_ids.reserve(n_docs);
    this->debug_values.reserve(n_docs);
    typename CV::Iterator values_it = this->cv.iterator();
    VIB::Iterator offsets_it = this->offsets.iterator();
    DOC_ID doc_id = 0;
    for (DOC_ID i = 0; i < n_docs; i++) {
        ValueType value = values_it.next();
        doc_id += offsets_it.next();
        this->debug_values.push_back(value);
        debug_doc_ids.push_back(doc_id);
    }
#endif
}

template<const uint8_t BITS>
std::unique_ptr<SplitSignature> FastSparseFeatureImpl <BITS>::get_split_signature(TreeNode * leaf, Split * split)
{
    if (map->sparse_v1) {
        return SparseFeatureImpl<BITS>::get_split_signature(leaf, split);
    }
    assert(split->feature == this);
    std::vector<DOC_ID> & doc_ids = leaf->doc_ids;
    std::unique_ptr<SplitSignature> split_signature(new SplitSignature(doc_ids.size(), this->default_value >= split->threshold));

    SHARD_ID_TYPE shard = map->nodes_to_shards[leaf->node_id];
    DOC_ID n_docs = this->shards[map->next_shard[shard]].v_ptr - this->shards[shard].v_ptr;
    typename CV::Iterator values_it = this->cv.iterator(this->shards[shard].v_ptr);
    VIB::Iterator offsets_it = this->offsets.iterator(this->shards[shard].o_ptr);;
    DOC_ID relative_id = 0;
    for (DOC_ID i = 0; i < n_docs; i++) {
        relative_id += offsets_it.next();
        ValueType value = values_it.next();
        assert(relative_id < doc_ids.size());
        split_signature->set(relative_id, value >= split->threshold);
    }

    if (split->inverse) {
        split_signature->invert();
    }
    return split_signature;
}


template<const uint8_t BITS>
std::unique_ptr<Histogram> FastSparseFeatureImpl <BITS>::compute_histogram(const TreeNode * leaf, bool newton_step)
{
    if ((map->sparse_v1) || (leaf->parent == nullptr)) {
        return SparseFeatureImpl<BITS>::compute_histogram(leaf, newton_step);
    }
    if (newton_step) {
        return this->compute_histogram_impl<true>(leaf);
    }
    else {
        return this->compute_histogram_impl<false>(leaf);
    }
}

template<const uint8_t BITS>
template<const bool NEWTON_STEP>
inline std::unique_ptr<Histogram> FastSparseFeatureImpl <BITS>::compute_histogram_impl(const TreeNode * leaf)
{
    typedef HistGetter<NEWTON_STEP> HG;
    std::unique_ptr<Histogram> result(new Histogram(this->n_buckets));
    HistogramItem fake;
    HistogramItem * hist_by_leaf[2] = {
&fake,
&(result->data[0])
    };
    assert(leaf->parent != nullptr);
    assert(leaf->parent->right == leaf);
    SHARD_ID_TYPE new_shard = map->nodes_to_shards[leaf->node_id];
    SHARD_ID_TYPE old_shard = map->previous_shard[new_shard];
    SHARD_ID_TYPE following_shard = map->next_shard[new_shard];
    assert(new_shard == this->shards.size());

    TreeNode * parent = leaf->parent;
    assert(parent->split_signature != nullptr);
    assert(parent->split_mapping != nullptr);
    SplitSignature & signature = *parent->split_signature;
    std::vector<DOC_ID> & mapping = *parent->split_mapping;

    FastSparseFeatureBuffer * buffer = map->get_buffer();

    DOC_ID n_docs = this->shards[following_shard].v_ptr - this->shards[old_shard].v_ptr;
    DOC_ID max_vib_size = this->shards[following_shard].o_ptr - this->shards[old_shard].o_ptr - this->shards[old_shard].tail;
    buffer->vib.resize(max_vib_size);
    auto pair = this->offsets.iterator_and_writer(this->shards[old_shard].o_ptr, max_vib_size);
    VIB::Iterator offset_it = pair.first;
    VIB::Writer offset_writers[2] = {
pair.second,
buffer->vib.writer(0)
    };

    CV temp_cv(&buffer->buffer, n_docs);
    typename CV::Iterator value_it = this->cv.iterator(this->shards[old_shard].v_ptr);
    typename CV::Writer value_writers[2] = {
        this->cv.writer(this->shards[old_shard].v_ptr),
        temp_cv.writer(0)
    };

    DOC_ID relative_id = 0;
    DOC_ID last_relative_ids[2] = { 0, 0 };
    for (DOC_ID i = 0; i < n_docs; i++) {
        typename CV::ValueType value = value_it.next();
        relative_id += offset_it.next();
        DOC_ID doc_id = parent->doc_ids[relative_id];
        uint8_t direction = signature[relative_id];
        assert(direction <= 1);

        HistogramItem & item = hist_by_leaf[direction][direction * value];
        Document & document = this->trainer_data->documents[doc_id];
        item.gradient += document.gradient;
        HG::get_weight(item) += HG::get_document_weight(document);

        value_writers[direction].write(value);
        DOC_ID new_relative_id = mapping[relative_id];
        assert(new_relative_id >= last_relative_ids[direction]);
        offset_writers[direction].write(new_relative_id - last_relative_ids[direction]);
        last_relative_ids[direction] = new_relative_id;
     }

    value_writers[0].flush();
    value_writers[1].flush();
    offset_writers[0].flush();
    offset_writers[1].flush();

    DOC_ID left_shard_size = offset_writers[0].get_ptr() - this->shards[old_shard].o_ptr;
    DOC_ID right_shard_size = offset_writers[1].get_ptr();
    DOC_ID available_space = this->shards[following_shard].o_ptr - this->shards[old_shard].o_ptr;
    if (available_space < left_shard_size + right_shard_size) {
        // Adding new_shard.
        this->shards.push_back(Shard(0, this->shards[old_shard].o_ptr + left_shard_size, 0));
        this->shards[old_shard].tail = 0;
        DOC_ID shortage = this->rearrange_shards(new_shard, old_shard, following_shard, right_shard_size);
        if (shortage > 0) {
            DOC_ID increment = shortage + map->fixed_tail_size;
            this->resize_offsets(increment);
            shortage = this->rearrange_shards(new_shard, old_shard, following_shard, right_shard_size);
            assert(shortage == 0);
        }
    }
    else {
        DOC_ID tail_to_split = available_space - (left_shard_size + right_shard_size);
        DOC_ID right_tail = tail_to_split / 2;
        DOC_ID left_tail = tail_to_split - right_tail;
        this->shards[old_shard].tail = left_tail;
        // Adding new_shard.
        this->shards.push_back(Shard(0, this->shards[old_shard].o_ptr + this->shards[old_shard].tail + left_shard_size, right_tail));
    }
    assert(this->shard_size(old_shard, false) == left_shard_size);
    assert(this->shard_size(new_shard, false) == right_shard_size);
    this->offsets.copy(buffer->vib, 0, this->shards[new_shard].o_ptr, right_shard_size);

    this->shards[new_shard].v_ptr = value_writers[0].get_ptr();
    this->cv.copy(temp_cv, 0, this->shards[new_shard].v_ptr, value_writers[1].get_ptr());
    buffer->vib.clear();
    SparseFeatureImpl<BITS>::template fix_histogram<NEWTON_STEP>(leaf, result.get());
    this->validate_shards();
    return result;
}

template<const uint8_t BITS>
DOC_ID FastSparseFeatureImpl <BITS>::rearrange_shards(SHARD_ID_TYPE  shard, SHARD_ID_TYPE left_neighbor, SHARD_ID_TYPE right_neighbor, DOC_ID required_space)
{
    assert(map->previous_shard[shard] == left_neighbor);
    assert(map->next_shard[shard] == right_neighbor);
    SHARD_ID_TYPE left = map->previous_shard[left_neighbor], right = right_neighbor; 
    SHARD_ID_TYPE left2 = FastShardMapping::NULL_SHARD, right2 = FastShardMapping::NULL_SHARD;
    DOC_ID left_moved = 0, right_moved = 0;
    DOC_ID available_space = this->shard_size(shard, true);
    DOC_ID gained_space = 0;
    if(available_space >= required_space) {
        this->shards[shard].tail = available_space - required_space;
        return 0;
    }
    while (available_space + gained_space < required_space) {
        if ((left == FastShardMapping::NULL_SHARD) && (right == FastShardMapping::FINAL_FAKE_SHARD)) {
            //throw std::runtime_error("FastSparseFeature::rearrange_shards() ran out of memory.");
            return required_space - (available_space + gained_space);
        }
        bool expand_right;
        if (left == FastShardMapping::NULL_SHARD) {
            expand_right = true;
        }
        else if (right == FastShardMapping::FINAL_FAKE_SHARD) {
            expand_right = false;
        }
        else {
            DOC_ID left_to_move = this->shard_size(map->next_shard[left], false);
            DOC_ID right_to_move = this->shard_size(right, false);
            if (left_moved + left_to_move <= right_moved + right_to_move) {
                expand_right = false;
            }
            else {
                expand_right = true;
            }
        }

        if (!expand_right) {
            gained_space += this->shards[left].tail;
            left_moved += this->shard_size(map->next_shard[left], false);
            left2 = left;
            left = map->previous_shard[left];
        }
        else {
            gained_space += this->shards[right].tail;
            right_moved += this->shard_size(right, false);
            right2 = right;
                right= map->next_shard[right];
        }
    }

    // We need to remember shard sizes.
    std::vector<DOC_ID> shard_sizes;
    shard_sizes.reserve(this->shards.size());
    for (size_t i = 0; i < this->shards.size(); i++) {
        if (i == FastShardMapping::FINAL_FAKE_SHARD) {
            shard_sizes.push_back(0);
        }
        else {
            shard_sizes.push_back(this->shard_size(i, false));
        }
    }

    // Now actually move the shards.
    DOC_ID left_shift = 0;
    if (left2 != FastShardMapping::NULL_SHARD) {
        for (; left2 != shard; left2 = map->next_shard[left2]) {
            if (left_shift > 0){
                this->offsets.move(this->shards[left2].o_ptr, this->shards[left2].o_ptr - left_shift, shard_sizes[left2]);
                this->shards[left2].o_ptr -= left_shift;
            }
            left_shift += this->shards[left2].tail;
            this->shards[left2].tail = 0;
        }
    }

    DOC_ID right_shift = 0;
    if (right2 != FastShardMapping::NULL_SHARD) {
        for (; right2!= shard; right2 = map->previous_shard[right2]) {
            right_shift += this->shards[right2].tail;
            if (right_shift > 0) {
                this->offsets.move(this->shards[right2].o_ptr, this->shards[right2].o_ptr + right_shift, shard_sizes[right2]);
                this->shards[right2].o_ptr += right_shift;
                this->shards[right2].tail = 0;
            }
        }
    }

    assert(left_shift + right_shift == gained_space);
        this->shards[shard].o_ptr -= left_shift;
        this->shards[shard].tail = available_space + gained_space - required_space;
        assert(this->shard_size(shard, true) >= required_space);
        return 0;
}

template<const uint8_t BITS>
inline void FastSparseFeatureImpl <BITS>::resize_offsets(DOC_ID shortage)
{
    assert(this->shards[FastShardMapping::FINAL_FAKE_SHARD].o_ptr == this->offsets.size());
    this->offsets.resize(this->offsets.size() + shortage);
    this->shards[FastShardMapping::FINAL_FAKE_SHARD].o_ptr += shortage;
    this->shards[map->previous_shard[FastShardMapping::FINAL_FAKE_SHARD]].tail += shortage;
        BOOST_LOG_TRIVIAL(warning)
        << "Resizing offsets buffer for feature '" << this->get_name()
        << "'. If this message appears often, consider increasing --initial_tail_size flag value.";
}

template<const uint8_t BITS>
inline DOC_ID FastSparseFeatureImpl <BITS>::shard_size(SHARD_ID_TYPE shard, bool with_tail)
{
    assert(shard < this->shards.size());
    assert(shard != FastShardMapping::FINAL_FAKE_SHARD);
    DOC_ID result = this->shards[map->next_shard[shard]].o_ptr - this->shards[shard].o_ptr;
    if (!with_tail) {
        result -= this->shards[shard].tail;
    }
    return  result;
}

template<const uint8_t BITS>
void FastSparseFeatureImpl <BITS>::on_finalize_tree()
{
    if (map->sparse_v1) {
        // parent member function does nothing, but for the sake of consistency, still call it.
        SparseFeatureImpl<BITS>::on_finalize_tree();
        return;
    }

    typedef gheap<> gh;
    ShardCursorCompare compare;

    FastSparseFeatureBuffer * buffer = map->get_buffer();
    VIB & tmp_offsets= buffer->vib;
    tmp_offsets.resize(this->offsets.size());
    VIB::Writer tmp_offsets_writer = tmp_offsets.writer(0);
    CV tmp_values(&buffer->buffer, this->cv.size());
    typename CV::Writer tmp_values_writer = tmp_values.writer(0);
    DOC_ID last_doc_id = 0;

    size_t n_cursors = this->shards.size() - 1;
    std::vector<ShardCursor> cursors;
    cursors.reserve(n_cursors);
    std::vector<ShardCursor *> heap;
    heap.reserve(n_cursors);
    for (size_t i = 0; i < this->shards.size(); i++) {
        if (map->next_shard[i] == FastShardMapping::NULL_SHARD) {
            continue;
        }
        DOC_ID n_docs = this->shards[map->next_shard[i]].v_ptr - this->shards[i].v_ptr;
        size_t index = cursors.size();
        cursors.push_back(ShardCursor(this->cv, this->offsets, this->shards[i], n_docs, map->shards_to_nodes[i]));
        heap.push_back(&cursors[index]);
        gh::push_heap(heap.begin(), heap.end(), compare);
    }
    while (heap.size() > 0) {
        ShardCursor * sc = heap[0];
        if (sc->n_docs_left == 0) {
            gh::pop_heap(heap.begin(), heap.end(), compare);
            heap.pop_back();
            continue;
        }
        assert(sc->current_doc_id >= last_doc_id);
        DOC_ID offset = sc->current_doc_id - last_doc_id;
        last_doc_id = sc->current_doc_id;
        tmp_offsets_writer.write(offset);
        tmp_values_writer.write(sc->current_value);
        sc->next();
        gh::restore_heap_after_item_decrease(heap.begin(), heap.begin(), heap.end(), compare);
    }
    tmp_offsets_writer.flush();
    tmp_values_writer.flush();
    assert(tmp_values_writer.get_ptr() == this->cv.size());
    assert(tmp_offsets_writer.get_ptr() < this->offsets.size());
    this->offsets.copy(tmp_offsets, 0, 0, tmp_offsets_writer.get_ptr());
    this->cv.copy(tmp_values, 0, 0, tmp_values_writer.get_ptr());
    
    // Shrinking the shards vector. The second argument is needed since Shard doesn't have a default constructor.
    this->shards.resize(2, Shard(0, 0, 0));
    this->shards[0] = Shard(0, 0, this->offsets.size() - tmp_offsets_writer.get_ptr());

    tmp_offsets.clear();

    assert(this->values_md5 == this->cv.md5());
    assert(this->offsets_md5 == this->offsets.md5(0, this->shards[1].o_ptr - this->shards[0].tail));
}

#ifdef FAST_SPARSE_FEATURE_DEBUG
template<const uint8_t BITS>
void FastSparseFeatureImpl <BITS>::validate_shards()
{
    DOC_ID total_n_docs = 0;
    std::vector<bool> doc_id_found(this->debug_doc_ids.size(), false);
    for (size_t i = 0; i < this->shards.size(); i++) {
        if (i == FastShardMapping::FINAL_FAKE_SHARD) {
            continue;
        }
        Shard & shard = this->shards[i];
        DOC_ID n_docs = this->shards[map->next_shard[i]].v_ptr - this->shards[i].v_ptr;
        total_n_docs += n_docs;
        const std::vector<DOC_ID> & leaf_doc_ids = map->shards_to_nodes[i]->doc_ids;
        typename CV::Iterator values_it = this->cv.iterator(shard.v_ptr);
        VIB::Iterator offsets_it = this->offsets.iterator(shard.o_ptr);
        DOC_ID relative_id = 0;
        for (DOC_ID j = 0; j < n_docs; j++) {
            relative_id += offsets_it.next();
            DOC_ID doc_id = leaf_doc_ids[relative_id];
            ValueType value = values_it.next();

            auto pair = std::equal_range(this->debug_doc_ids.begin(), this->debug_doc_ids.end(), doc_id);
            size_t index = pair.first - this->debug_doc_ids.begin();
            assert(index < this->debug_doc_ids.size());
            assert(this->debug_doc_ids[index] == doc_id);
            assert(this->debug_values[index] == value);
            assert(doc_id_found[index] == false);
            doc_id_found[index] = true;
        }
        assert(*offsets_it.get_ptr_internal() + this->shards[i].tail == this->shards[map->next_shard[i]].o_ptr);
    }
    assert(total_n_docs == this->debug_doc_ids.size());
    assert(total_n_docs == this->debug_values.size());
    assert(std::find(doc_id_found.begin(), doc_id_found.end(), false) == doc_id_found.end());
}
#endif

template class FastSparseFeatureImpl <1>;
template class FastSparseFeatureImpl <2>;
template class FastSparseFeatureImpl <4>;
template class FastSparseFeatureImpl <8>;
template class FastSparseFeatureImpl <16>;
