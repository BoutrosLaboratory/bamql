/*
 * Copyright 2015 Paul Boutros. For details, see COPYING. Our lawyer cats sez:
 *
 * OICR makes no representations whatsoever as to the SOFTWARE contained
 * herein.  It is experimental in nature and is provided WITHOUT WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE OR ANY OTHER WARRANTY,
 * EXPRESS OR IMPLIED. OICR MAKES NO REPRESENTATION OR WARRANTY THAT THE USE OF
 * THIS SOFTWARE WILL NOT INFRINGE ANY PATENT OR OTHER PROPRIETARY RIGHT.  By
 * downloading this SOFTWARE, your Institution hereby indemnifies OICR against
 * any loss, claim, damage or liability, of whatsoever kind or nature, which
 * may arise from your Institution's respective use, handling or storage of the
 * SOFTWARE. If publications result from research using this SOFTWARE, we ask
 * that the Ontario Institute for Cancer Research be acknowledged and/or
 * credit be given to OICR scientists, as scientifically appropriate.
 */

#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <memory>

#define BAMQL_ITERARTOR_API_VERSION 2
namespace bamql {

/**
 * The error handler type.
 */
typedef void (*ErrorHandler)(const char *str, void *context);

/**
 * The run-time type of a filter.
 */
typedef bool (*FilterFunction)(bam_hdr_t *, bam1_t *, ErrorHandler, void *);

/**
 * The run-time type of an index checker.
 */
typedef bool (*IndexFunction)(bam_hdr_t *, uint32_t, ErrorHandler, void *);

/**
 * Iterator over all the reads in a BAM file, using an index if possible.
 */
class ReadIterator {
public:
  ReadIterator();
  /**
   * Should the reads on this chromosome be examined?
   */
  virtual bool wantChromosome(std::shared_ptr<bam_hdr_t> &header,
                              uint32_t tid) = 0;
  /**
   * Examine a read.
   */
  virtual void processRead(std::shared_ptr<bam_hdr_t> &header,
                           std::shared_ptr<bam1_t> &read) = 0;
  /**
   * Examine the header of a new file.
   */
  virtual void ingestHeader(std::shared_ptr<bam_hdr_t> &header) = 0;
  /**
   * Process the reads in the supplied file.
   * @param file_name: The path to the BAM/SAM file.
   * @param binary: Is the file BAM (true) or SAM (false).
   * @param ignore_index: Do not use the index even if one is found.
   */
  bool processFile(const char *file_name, bool binary, bool ignore_index);

private:
  bool wantAll(std::shared_ptr<bam_hdr_t> &header);
};

template <typename T> class channel {
public:
  channel(size_t max_in_flight_)
      : max_in_flight(max_in_flight_), closed(false) {}
  void close() {
    std::unique_lock<std::mutex> lock(m);
    closed = true;
    drain_cv.notify_one();
  }
  void operator<<(const T &target) {
    std::unique_lock<std::mutex> lock(m);
    if (closed)
      throw std::logic_error("Attempt to write to closed channel.");
    fill_cv.wait(lock, [&] { return q.size() < max_in_flight; });
    q.push(target);
    drain_cv.notify_one();
  }
  bool operator>>(T &target) {
    std::unique_lock<std::mutex> lock(m);
    drain_cv.wait(lock, [&] { return !q.empty() || closed; });
    if (q.size() >= max_in_flight) {
      fill_cv.notify_one();
    }
    if (!q.empty()) {
      target = q.front();
      q.pop();
      return true;
    }
    return false;
  }

private:
  bool closed;
  size_t max_in_flight;
  std::queue<T> q;
  std::mutex m;
  std::condition_variable fill_cv;
  std::condition_variable drain_cv;
};

typedef std::pair<std::shared_ptr<bam_hdr_t>, std::shared_ptr<bam1_t>>
    read_info;

class AsyncBamWriter {
public:
  AsyncBamWriter(std::shared_ptr<htsFile> &output);
  ~AsyncBamWriter();
  size_t count();
  void ingestHeader(std::shared_ptr<bam_hdr_t> &header,
                    std::string &name,
                    std::string &id_str,
                    std::string &version,
                    std::string &query);
  void ingestHeader(std::shared_ptr<bam_hdr_t> &header);
  void write(std::shared_ptr<bam_hdr_t> &header, std::shared_ptr<bam1_t> &read);

private:
  std::shared_ptr<htsFile> output;
  size_t seq_count = 0;
  std::shared_ptr<bamql::channel<read_info>> write_channel;
  std::thread write_thread;
};

/**
 * Craft a new BAM header appending information about the manipulations done.
 * @param name: the name of the program doing the manipulation.
 * @param id: a unique ID.
 * @param version: the version of the program doing the manipulation.
 * @param args: the command line arguments passed to this program.
 */
std::shared_ptr<bam_hdr_t> appendProgramToHeader(const bam_hdr_t *original,
                                                 const std::string &name,
                                                 const std::string &id,
                                                 const std::string &version,
                                                 const std::string &args);

std::shared_ptr<htsFile> open(const char *filename, const char *mode);

std::shared_ptr<bam1_t> allocateRead();
}
