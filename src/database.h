/******************************************************************************
 *
 * RmapAlign3N - 3N Read Mapping and Alignment Tool
 *
 * Copyright (C) 2024 André Müller (muellan@uni-mainz.de)
 *                       
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef RMA_SKETCH_DATABASE_H_
#define RMA_SKETCH_DATABASE_H_

#include <climits>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <type_traits>
#include <cstdint>
#include <string>
#include <limits>
#include <memory>
#include <future>
#include <chrono>
#include <sstream>

#include "version.h"
#include "config.h"
#include "io_error.h"
#include "io_options.h"
#include "stat_combined.h"
#include "hash_multimap.h"
#include "dna_encoding.h"
#include "typename.h"
#include "sequence_io.h"

#include "batch_processing.h"

namespace mc {

/*************************************************************************//**
 *
 * @brief  maps 'features' (e.g. hash values obtained by min-hashing)
 *         to 'locations' = positions in targets/reference sequences
 *
 *         not copyable, but movable
 *
 * @details terminology
 *  target          reference sequence whose sketches are stored in the DB
 *
 *  query           sequence (usually short reads) that shall be matched against
 *                  the reference targets
 *
 *  window_id       window index (starting with 0) within a target
 *
 *  location        (window_id, target id) = "window within a target sequence"
 *
 *  sketcher        function object type, that maps reference sequence
 *                      windows (= sequence interval) to
 *                      sketches (= collection of features of the same type)
 *
 *  feature         single part of a sketch
 *
 *  feature_hash    hash function for (feature -> location) map
 *
 *  target id        internal target (reference sequence) identification

 *  window id        target window identification
 *
 *  loclist_size_t   bucket (location list) size tracking type
 *
 *****************************************************************************/
class database
{
public:
    //---------------------------------------------------------------
    // from global config
    using sequence         = mc::sequence;
    using sketcher         = mc::sketcher;
    using feature_hash     = mc::feature_hash;
    using target_id        = mc::target_id;
    using window_id        = mc::window_id;
    using bucket_size_type = mc::loclist_size_t;

    //---------------------------------------------------------------
    using target_name = std::string;

    //---------------------------------------------------------------
    using match_count_type = std::uint16_t;

    //---------------------------------------------------------------
    enum class scope { sketches, metadata_only , everything};


    //-----------------------------------------------------
    class target_limit_exceeded_error : public std::runtime_error {
    public:
        target_limit_exceeded_error():
            std::runtime_error{"target count limit exceeded"}
        {}
    };

    //-----------------------------------------------------
    /** @brief internal location representation = (window index, target index)
     *         these are stored in the in-memory database and on disk
     */
    #pragma pack(push, 1)
    //avoid padding bits
    struct location
    {
        window_id win;
        target_id tgt;

        friend bool
        operator == (const location& a, const location& b) noexcept {
            return (a.tgt == b.tgt) && (a.win == b.win);
        }

        friend bool
        operator < (const location& a, const location& b) noexcept {
            if (a.tgt < b.tgt) return true;
            if (a.tgt > b.tgt) return false;
            return (a.win < b.win);
        }
    };
    //avoid padding bits
    #pragma pack(pop)

    using match_locations = std::vector<location>;



    /************************************************************
     *
     * @brief target metadata
     *
     ************************************************************/
    class target {
        friend class database;
    public:
        //-----------------------------------------------------
        struct file_source {
            using index_t   = std::uint_least64_t;
            using window_id = std::uint_least64_t;

            file_source() = default;

            explicit
            file_source(std::string filename, index_t index,
                        window_id numWindows)
            :
                filename{std::move(filename)}, windows{numWindows}, index{index}
            {}

            std::string filename;
            window_id windows;
            index_t index;
        };

        target() = default;

        explicit
        target(std::string targetName,
              file_source source)
        :
            name_{std::move(targetName)},
            source_{std::move(source)}
        {}

        const target_name& name() const noexcept { return name_; }
        const file_source& source() const noexcept { return source_; }
        const std::string& header() const noexcept {return header_;}
        const sequence& seq() const noexcept {return seq_;}

        //-----------------------------------------------------
        friend
        void read_binary(std::istream& is, target& t) {
            read_binary(is, t.name_);
            read_binary(is, t.source_.filename);
            read_binary(is, t.source_.index);
            read_binary(is, t.source_.windows);
        }

        //-----------------------------------------------------
        friend
        void write_binary(std::ostream& os, const target& t) {
            write_binary(os, t.name_);
            write_binary(os, t.source_.filename);
            write_binary(os, t.source_.index);
            write_binary(os, t.source_.windows);
        }

        //-----------------------------------------------------
    private:
        target_name name_;
        file_source source_;
        
        // only used in alignment mode
        std::string header_;
        sequence seq_;
    };

    using file_source = target::file_source;

    const target& get_target(target_id id) const noexcept { return targets_[id]; }


    //-----------------------------------------------------
    using sketch  = typename sketcher::sketch_type;  //range of features
    using feature = typename sketch::value_type;


private:

    using target_store = std::vector<target>;

    friend
    void write_binary(std::ostream& os, const target_store& t) {
        write_binary(os, t.size());
        for (const target& tgt: t)
            write_binary(os, tgt);
    }

    friend
    void read_binary(std::istream& is, target_store& t) {
        uint64_t n;
        read_binary(is, n);
        t.resize(n);
        for (target& tgt: t)
            read_binary(is, tgt);
    }

    //-----------------------------------------------------
    /// @brief "heart of the database": maps features to target locations
    using feature_store = hash_multimap<feature,location, //key, value
                              feature_hash,               //key hasher
                              std::equal_to<feature>,     //key comparator
                              chunk_allocator<location>,  //value allocator
                              std::allocator<feature>,    //bucket+key allocator
                              bucket_size_type ,           //location list size
                              linear_probing>;

    //-----------------------------------------------------
    /// @brief needed for batched, asynchonous insertion into feature_store
    struct window_sketch
    {
        window_sketch() = default;

        window_sketch(target_id tgt, window_id win, sketch sk) :
            tgt{tgt}, win{win}, sk{std::move(sk)} {};

        target_id tgt;
        window_id win;
        sketch sk;
    };

    using sketch_batch = std::vector<window_sketch>;

    void reread_targets() {
        using indexed_targets = std::unordered_map<size_t, target_id>;
        using catalogue = std::pair<std::unique_ptr<sequence_reader>, indexed_targets>;
        
        std::unordered_map<std::string, catalogue> catalogues;

        for (target_id tgt = 0; tgt < target_count(); ++tgt) {
            const auto& src = get_target(tgt).source();
            if (!catalogues.count(src.filename))
                catalogues.emplace(src.filename, catalogue{make_sequence_reader(src.filename), indexed_targets()});
            catalogues[src.filename].second.emplace(src.index, tgt);
        }

        targets_.resize(target_count());
        for (auto& i: catalogues) {
            auto& cat = i.second;
            auto& reader = cat.first;
            auto& targets = cat.second;
            while (reader->has_next()) {
                auto idx = reader->index();
                if (targets.count(idx)) {
                    auto seq = reader->next();
                    targets_[targets[idx]].header_ = std::move(seq.header);
                    targets_[targets[idx]].seq_ = std::move(seq.data);
                } else {
                    reader->skip(1);
                }
            }
        }
    }

public:

    void show_sam_header(std::ostream& os) const {
        os << "@HD\tVN:1.0 SO:unsorted\n";
        for (const auto& tgt: targets_)
            os << "@SQ\tSN:" << tgt.header_ << "\tLN:" << tgt.seq_.size() << '\n';
        os << "@PG\tID:rnaache\tPN:rnaache\tVN:" << RMA_VERSION_STRING << '\n';
    }

    std::string get_sam_header() const {
        std::ostringstream tmp;
        show_sam_header(tmp);
        return tmp.str();
    }


    //---------------------------------------------------------------
    using feature_count_type = typename feature_store::size_type;


    //---------------------------------------------------------------
    /** @brief used for query result storage/accumulation
     */
    class matches_sorter {
        friend class database;

    public:
        void sort() {
            merge_sort(locs_, offsets_, temp_);
        }

        void clear() {
            locs_.clear();
            offsets_.clear();
            offsets_.resize(1, 0);
        }

        bool empty() const noexcept { return locs_.empty(); }
        auto size()  const noexcept { return locs_.size(); }

        auto begin() const noexcept { return locs_.begin(); }
        auto end()   const noexcept { return locs_.end(); }

        const match_locations&
        locations() const noexcept { return locs_; }

    private:
        static void
        merge_sort(match_locations& inout,
                   std::vector<size_t>& offsets,
                   match_locations& temp)
        {
            if (offsets.size() < 3) return;
            temp.resize(inout.size());

            int numChunks = offsets.size()-1;
            for (int s = 1; s < numChunks; s *= 2) {
                for (int i = 0; i < numChunks; i += 2*s) {
                    auto begin = offsets[i];
                    auto mid = i + s <= numChunks ? offsets[i + s] : offsets[numChunks];
                    auto end = i + 2*s <= numChunks ? offsets[i + 2*s] : offsets[numChunks];
                    std::merge(inout.begin()+begin, inout.begin()+mid,
                               inout.begin()+mid, inout.begin()+end,
                               temp.begin()+begin);
                }
                std::swap(inout, temp);
            }
        }

        match_locations locs_; // match locations from hashmap
        std::vector<std::size_t> offsets_;  // bucket sizes for merge sort
        match_locations temp_; // temp buffer for merge sort
    };


    //---------------------------------------------------------------
    explicit
    database(sketcher targetSketcher = sketcher{}) :
        database{targetSketcher, targetSketcher}
    {}
    //-----------------------------------------------------
    explicit
    database(sketcher targetSketcher, sketcher querySketcher) :
        targetSketcher_{std::move(targetSketcher)},
        querySketcher_{std::move(querySketcher)},
        maxLocsPerFeature_(max_supported_locations_per_feature()),
        features_{},
        targets_{},
        name2tax_{},
        inserter_{}
    {
        features_.max_load_factor(default_max_load_factor());
    }

    database(const database&) = delete;
    database(database&& other) :
        targetSketcher_{std::move(other.targetSketcher_)},
        querySketcher_{std::move(other.querySketcher_)},
        maxLocsPerFeature_(other.maxLocsPerFeature_),
        features_{std::move(other.features_)},
        targets_{std::move(other.targets_)},
        name2tax_{std::move(other.name2tax_)},
        inserter_{std::move(other.inserter_)}
    {}

    database& operator = (const database&) = delete;
    database& operator = (database&&)      = default;

    ~database() {
        wait_until_add_target_complete();
    }


    //---------------------------------------------------------------
    /**
     * @return const ref to the object that transforms target sequence
     *         snippets into a collection of features
     */
    const sketcher&
    target_sketcher() const noexcept {
        return targetSketcher_;
    }
    //-----------------------------------------------------
    /**
     * @return const ref to the object that transforms query sequence
     *         snippets into a collection of features
     */
    const sketcher&
    query_sketcher() const noexcept {
        return querySketcher_;
    }
    //-----------------------------------------------------
    /**
     * @brief sets the object that transforms query sequence
     *        snippets into a collection of features
     */
    void query_sketcher(const sketcher& s) {
        querySketcher_ = s;
    }
    void query_sketcher(sketcher&& s) {
        querySketcher_ = std::move(s);
    }


    //---------------------------------------------------------------
    void max_locations_per_feature(bucket_size_type);

    //-----------------------------------------------------
    bucket_size_type
    max_locations_per_feature() const noexcept {
        return maxLocsPerFeature_;
    }
    //-----------------------------------------------------
    static bucket_size_type
    max_supported_locations_per_feature() noexcept {
        return (feature_store::max_bucket_size() - 1);
    }

    //-----------------------------------------------------
    feature_count_type
    remove_features_with_more_locations_than(bucket_size_type);


    //---------------------------------------------------------------
    /**
     * @brief  removes features that appear in more than 'maxambig' different
     *         targets
     *
     * @return number of features (hash table buckets) that were removed
     */
    feature_count_type
    remove_ambiguous_features(bucket_size_type maxambig);


    //---------------------------------------------------------------
    bool add_target(const sequence& seq, target_name sid,
                    file_source source = file_source{});


    //---------------------------------------------------------------
    std::uint64_t
    target_count() const noexcept {
        return targets_.size();
    }
    static constexpr std::uint64_t
    max_target_count() noexcept {
        return std::numeric_limits<target_id>::max();
    }
    static constexpr std::uint64_t
    max_windows_per_target() noexcept {
        return std::numeric_limits<window_id>::max();
    }


    //---------------------------------------------------------------
    void clear();

    /**
     * @brief very dangerous! clears feature map without memory deallocation
     */
    void clear_without_deallocation();


    static constexpr target_id nulltgt = std::numeric_limits<target_id>::max();

    //-----------------------------------------------------
    /**
     * @brief finds exact target names
     */
    target_id
    target_with_name(const target_name& name) const noexcept {
        if (name.empty()) return nulltgt;
        auto i = name2tax_.find(name);
        if (i == name2tax_.end()) return nulltgt;
        return i->second;
    }
    //-----------------------------------------------------
    /**
     * @brief will find target names with different versions
     */
    target_id
    target_with_similar_name(const target_name& name) const noexcept {
        if (name.empty()) return nulltgt;
        auto i = name2tax_.upper_bound(name);
        if (i == name2tax_.end()) return nulltgt;
        const auto s = name.size();
        if (0 != i->first.compare(0,s,name)) return nulltgt;
        return i->second;
    }

    //---------------------------------------------------------------
    template<class InputIterator>
    void
    accumulate_matches(InputIterator queryBegin, InputIterator queryEnd,
                       matches_sorter& res) const
    {
        querySketcher_.for_each_sketch(queryBegin, queryEnd,
            [this, &res] (const auto& sk) {
                 res.offsets_.reserve(res.offsets_.size() + sk.size());

                for (auto f : sk) {
                    auto locs = features_.find(f);
                    if (locs != features_.end() && locs->size() > 0) {
                        res.locs_.insert(res.locs_.end(), locs->begin(), locs->end());
                        res.offsets_.emplace_back(res.locs_.size());
                    }
                }
            });
    }

    //---------------------------------------------------------------
    void
    accumulate_matches(const sequence& query,
                       matches_sorter& res) const
    {
        using std::begin;
        using std::end;
        accumulate_matches(begin(query), end(query), res);
    }


    //---------------------------------------------------------------
    void max_load_factor(float lf) {
        features_.max_load_factor(lf);
    }
    //-----------------------------------------------------
    float max_load_factor() const noexcept {
        return features_.max_load_factor();
    }
    //-----------------------------------------------------
    static constexpr float default_max_load_factor() noexcept {
        return 0.8f;
    }

    /**
     * @brief   read database from binary file
     * @details Note that the map is not just de-serialized but
     *          rebuilt by inserting individual keys and values
     *          This should make DB files more robust against changes in the
     *          internal mapping structure.
     */
    void read(const std::string& filename, scope what = scope::sketches);
    /**
     * @brief   write database to binary file
     */
    void write(const std::string& filename) const;

    //---------------------------------------------------------------
    std::uint64_t bucket_count() const noexcept {
        return features_.bucket_count();
    }
    //---------------------------------------------------------------
    std::uint64_t feature_count() const noexcept {
        return features_.key_count();
    }
    //---------------------------------------------------------------
    std::uint64_t dead_feature_count() const noexcept {
        return features_.key_count() - features_.non_empty_bucket_count();
    }
    //---------------------------------------------------------------
    std::uint64_t location_count() const noexcept {
        return features_.value_count();
    }


    //---------------------------------------------------------------
    statistics_accumulator
    location_list_size_statistics() const {
        auto priSize = statistics_accumulator{};

        for (const auto& bucket : features_) {
            if (!bucket.empty()) {
                priSize += bucket.size();
            }
        }

        return priSize;
    }


    //---------------------------------------------------------------
    void print_feature_map(std::ostream& os) const {
        for (const auto& bucket : features_) {
            if (!bucket.empty()) {
                os << std::int_least64_t(bucket.key()) << " -> ";
                for (location p : bucket) {
                    os << '(' << std::int_least64_t(p.tgt)
                       << ',' << std::int_least64_t(p.win) << ')';
                }
                os << '\n';
            }
        }
    }


    //---------------------------------------------------------------
    void print_feature_counts(std::ostream& os) const {
        for (const auto& bucket : features_) {
            if (!bucket.empty()) {
                os << std::int_least64_t(bucket.key()) << " -> "
                   << std::int_least64_t(bucket.size()) << '\n';
            }
        }
    }


    //---------------------------------------------------------------
    void wait_until_add_target_complete() {
        // destroy inserter
        inserter_ = nullptr;
    }


    //---------------------------------------------------------------
    bool add_target_failed() {
        return (inserter_) && (!inserter_->valid());
    }


private:
    //---------------------------------------------------------------
    window_id add_all_window_sketches(const sequence& seq, target_id tgt) {
        if (!inserter_) make_sketch_inserter();

        window_id win = 0;
        targetSketcher_.for_each_sketch(seq,
            [&, this] (auto&& sk) {
                if (inserter_->valid()) {
                    //insert sketch into batch
                    auto& sketch = inserter_->next_item();
                    sketch.tgt = tgt;
                    sketch.win = win;
                    sketch.sk = std::move(sk);
                }
                ++win;
            });
        return win;
    }


    //---------------------------------------------------------------
    void add_sketch_batch(const sketch_batch& batch) {
        for (const auto& windowSketch : batch) {
            //insert features from sketch into database
            for (const auto& f : windowSketch.sk) {
                auto it = features_.insert(
                    f, location{windowSketch.win, windowSketch.tgt});
                if (it->size() > maxLocsPerFeature_) {
                    features_.shrink(it, maxLocsPerFeature_);
                }
            }
        }
    }


    //---------------------------------------------------------------
    void make_sketch_inserter() {
        batch_processing_options execOpt;
        execOpt.batch_size(1000);
        execOpt.queue_size(100);
        execOpt.concurrency(1);

        inserter_ = std::make_unique<batch_executor<window_sketch>>( execOpt,
            [&,this](int, const auto& batch) {
                this->add_sketch_batch(batch);
            });
    }


private:


    //---------------------------------------------------------------
    sketcher targetSketcher_;
    sketcher querySketcher_;
    std::uint64_t maxLocsPerFeature_;
    feature_store features_;
    target_store targets_; // target metadata
    std::map<target_name,target_id> name2tax_;
    std::unique_ptr<batch_executor<window_sketch>> inserter_;

};


/*************************************************************************//**
 *
 * @brief pull some types into global namespace
 *
 *****************************************************************************/
using match_locations = database::match_locations;
using target = database::target;




/*************************************************************************//**
 *
 * @brief reads database from file
 *
 *****************************************************************************/
database
make_database(const std::string& filename,
              database::scope = database::scope::sketches,
              info_level = info_level::moderate);



} // namespace mc

#endif
