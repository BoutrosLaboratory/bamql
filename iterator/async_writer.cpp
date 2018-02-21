#include "bamql-iterator.hpp"

static void async_write(
    std::shared_ptr<bamql::channel<bamql::read_info>> write_channel,
    std::shared_ptr<htsFile> output) {
  bamql::read_info info;
  while (*write_channel >> info) {
    sam_write1(output.get(), info.first.get(), info.second.get());
  }
}

bamql::AsyncBamWriter::AsyncBamWriter(std::shared_ptr<htsFile> &output_)
    : output(output_), write_channel(new bamql::channel<read_info>(10)),
      write_thread(async_write, write_channel, output_) {}
bamql::AsyncBamWriter::~AsyncBamWriter() {
  write_channel->close();
  write_thread.join();
}

size_t bamql::AsyncBamWriter::count() { return seq_count; }

void bamql::AsyncBamWriter::ingestHeader(std::shared_ptr<bam_hdr_t> &header,
                                         std::string &name,
                                         std::string &id_str,
                                         std::string &version,
                                         std::string &query) {
  auto copy =
      bamql::appendProgramToHeader(header.get(), name, id_str, version, query);
  ingestHeader(copy);
}
void bamql::AsyncBamWriter::ingestHeader(std::shared_ptr<bam_hdr_t> &header) {
  if (output) {
    sam_hdr_write(output.get(), header.get());
  }
}

void bamql::AsyncBamWriter::write(std::shared_ptr<bam_hdr_t> &header,
                                  std::shared_ptr<bam1_t> &read) {
  seq_count++;
  if (output) {
    read_info info(header, read);
    *write_channel << info;
  }
}
