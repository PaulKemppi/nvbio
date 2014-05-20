/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 *
 *
 *
 *
 *
 *
 *
 *
 */

#include "mem-search.h"
#include "options.h"
#include "pipeline.h"
#include "util.h"

#include <nvbio/basic/numbers.h>
#include <nvbio/basic/algorithms.h>
#include <nvbio/basic/timer.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/iterator/transform_iterator.h>
#include <thrust/sort.h>

using namespace nvbio;

static io::FMIndexData *load_genome(const char *name, uint32 flags)
{
    if (command_line_options.genome_use_mmap)
    {
        // try loading via mmap first
        io::FMIndexDataMMAP *mmap_loader = new io::FMIndexDataMMAP();
        if (mmap_loader->load(name))
        {
            return mmap_loader;
        }

        delete mmap_loader;
    }

    // fall back to file name
    io::FMIndexDataRAM *file_loader = new io::FMIndexDataRAM();
    if (!file_loader->load(name, flags))
    {
        log_error(stderr, "could not load genome %s\n");
        exit(1);
    }

    return file_loader;
}

void mem_init(struct pipeline_state *pipeline)
{
    // this specifies which portions of the FM index data to load
    const uint32 fm_flags = io::FMIndexData::GENOME  |
                            io::FMIndexData::FORWARD |
                            io::FMIndexData::REVERSE |
                            io::FMIndexData::SA;

    // load the genome on the host
    pipeline->mem.fmindex_data_host = load_genome(command_line_options.genome_file_name, fm_flags);
    // copy genome data to device
    pipeline->mem.fmindex_data_device = new io::FMIndexDataDevice(*pipeline->mem.fmindex_data_host, fm_flags);

    pipeline->mem.f_index = pipeline->mem.fmindex_data_device->index();
    pipeline->mem.r_index = pipeline->mem.fmindex_data_device->rindex();
}

// search MEMs for all reads in batch
void mem_search(struct pipeline_state *pipeline, const io::SequenceDataDevice *batch)
{
    ScopedTimer<float> timer( &pipeline->stats.search_time ); // keep track of the time spent here

    struct mem_state *mem = &pipeline->mem;

    const uint32 n_reads = batch->size();

    // reset the filter
    mem->mem_filter = mem_state::mem_filter_type();

    const io::SequenceDataAccess<DNA_N> read_access( *batch );

    // search for MEMs
    mem->mem_filter.rank(
        mem->f_index,
        mem->r_index,
        read_access.sequence_string_set(),
        command_line_options.min_intv,
        command_line_options.max_intv,
        command_line_options.min_seed_len,
        command_line_options.split_len,
        command_line_options.split_width);

    log_verbose(stderr, "%.1f average ranges\n", float(mem->mem_filter.n_ranges()) / float(n_reads));
    log_verbose(stderr, "%.1f average MEMs\n", float(mem->mem_filter.n_mems()) / float(n_reads));

    optional_device_synchronize();
    nvbio::cuda::check_error("mem-search kernel");
}

// given the first read in a chunk, determine a suitably sized chunk of reads
// (for which we can locate all MEMs in one go), updating pipeline::chunk
void fit_read_chunk(
    struct pipeline_state               *pipeline,
    const io::SequenceDataDevice        *batch,
    const uint32                        read_begin)     // first read in the chunk
{
    const ScopedTimer<float> timer( &pipeline->stats.search_time ); // keep track of the time spent here

    struct mem_state *mem = &pipeline->mem;

    const uint32 max_hits = command_line_options.mems_batch;

    //
    // use a binary search to locate the ending read forming a chunk with up to max_hits
    //

    pipeline->chunk.read_begin = read_begin;

    // skip pathological cases
    if (mem->mem_filter.n_ranges() == 0)
    {
        pipeline->chunk.read_end  = batch->size();
        pipeline->chunk.mem_begin = 0u;
        pipeline->chunk.mem_end   = 0u;
        return;
    }

    // determine the index of the first hit in the chunk
    pipeline->chunk.mem_begin = mem->mem_filter.first_hit( read_begin );

    // perform a binary search to find the end of our batch
    pipeline->chunk.read_end = nvbio::string_batch_bound( mem->mem_filter, pipeline->chunk.mem_begin + max_hits );

    // determine the index of the ending hit in the chunk
    pipeline->chunk.mem_end = mem->mem_filter.first_hit( pipeline->chunk.read_end );

    // check whether we couldn't produce a non-empty batch
    if (pipeline->chunk.mem_begin == pipeline->chunk.mem_end &&
        pipeline->chunk.mem_end < mem->mem_filter.n_mems())
    {
        const uint32 mem_count = mem->mem_filter.first_hit( read_begin+1 ) - pipeline->chunk.mem_begin;
        log_error(stderr,"read %u/%u has too many MEMs (%u), exceeding maximum batch size\n", read_begin, batch->size(), mem_count );
        exit(1);
    }
}

// a functor to extract the reference location from a mem
struct mem_loc_functor
{
    typedef mem_state::mem_type argument_type;
    typedef uint64                result_type;

    NVBIO_HOST_DEVICE
    uint64 operator() (const argument_type mem) const
    {
        return uint64( mem.index_pos() ) | (uint64( mem.string_id() ) << 32);
    }
};

// a functor to extract the reference left coordinate from a mem
struct mem_left_coord_functor
{
    typedef mem_state::mem_type argument_type;
    typedef uint64                result_type;

    NVBIO_HOST_DEVICE
    uint64 operator() (const argument_type mem) const
    {
        return uint64( mem.span().x ) | (uint64( mem.string_id() ) << 32);
    }
};

// locate all mems in the range defined by pipeline::chunk
void mem_locate(struct pipeline_state *pipeline, const io::SequenceDataDevice *batch)
{
    const ScopedTimer<float> timer( &pipeline->stats.locate_time ); // keep track of the time spent here

    struct mem_state                *mem = &pipeline->mem;
    struct chains_state<device_tag> *chn = &pipeline->chn;

    if (chn->mems.size() < command_line_options.mems_batch)
    {
        chn->mems.resize( command_line_options.mems_batch );
        chn->mems_index.resize( command_line_options.mems_batch );
        chn->mems_chain.resize( command_line_options.mems_batch );
    }

    const uint32 n_mems = pipeline->chunk.mem_end - pipeline->chunk.mem_begin;

    // skip pathological cases
    if (n_mems == 0u)
        return;

    chn->n_mems = n_mems;

    mem->mem_filter.locate(
        pipeline->chunk.mem_begin,
        pipeline->chunk.mem_end,
        chn->mems.begin() );

    // sort the mems by reference location
    nvbio::vector<device_tag,uint64> loc( n_mems );

    thrust::transform(
        chn->mems.begin(),
        chn->mems.begin() + n_mems,
        loc.begin(),
        mem_left_coord_functor() );
        //mem_loc_functor() );

    thrust::copy(
        thrust::make_counting_iterator<uint32>(0u),
        thrust::make_counting_iterator<uint32>(0u) + n_mems,
        chn->mems_index.begin() );

    // TODO: this is slow, switch to nvbio::cuda::SortEnactor
    thrust::sort_by_key(
        loc.begin(),
        loc.begin() + n_mems,
        chn->mems_index.begin() );

    optional_device_synchronize();
    nvbio::cuda::check_error("mem-locate kernel");

    pipeline->stats.n_mems += n_mems; // keep track of the number of mems produced
}
