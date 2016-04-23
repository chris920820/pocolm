// lm-state.cc

// Copyright     2016  Johns Hopkins University (Author: Daniel Povey)

// See ../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABILITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include <cassert>
#include "lm-state.h"

namespace pocolm {


void IntLmState::Print(std::ostream &os) const {
  os << " [ ";
  for (int32 i = 0; i < history.size(); i++)
    os << history[i] << " ";
  os << "]: ";
  for (int32 i = 0; i < counts.size(); i++)
    os << counts[i].first << "->" << counts[i].second << " ";
  os << "\n";
  Check();
}


void IntLmState::Write(std::ostream &os) const {
  if (rand() % 2 == 0)
    Check();
  int32 history_size = history.size(),
      num_counts = counts.size(),
      buffer_size = (2 + history_size + 2 * num_counts);
  assert(num_counts > 0);
  int32 *buffer = new int32[buffer_size];
  buffer[0] = history_size;
  buffer[1] = num_counts;
  for (int32 i = 0; i < history_size; i++)
    buffer[2 + i] = history[i];
  for (int32 i = 0; i < num_counts; i++) {
    buffer[2 + history_size + 2 * i] = counts[i].first;
    buffer[2 + history_size + 2 * i + 1] = counts[i].second;
  }
  os.write(reinterpret_cast<const char*>(buffer),
           sizeof(int32) * size_t(buffer_size));
  if (!os.good()) {
    std::cerr << "Failure writing IntLmState to stream\n";
    exit(1);
  }
  delete[] buffer;
}

void IntLmState::Read(std::istream &is) {
  // this will store history_size and num_counts; then we'll
  // read the rest.
  int32 partial_buffer[2];
  size_t bytes_read = is.read(reinterpret_cast<char*>(partial_buffer),
                              sizeof(int32) * 2).gcount();
  if (bytes_read != sizeof(int32) * 2) {
    std::cerr << "Failure reading IntLmState, expected 8 bytes, got "
              << bytes_read;
    exit(1);
  }
  int32 history_size = partial_buffer[0], num_counts = partial_buffer[1];
  assert(history_size >= 0 && num_counts > 0);
  history.resize(history_size);
  if (history_size > 0) {
    size_t expected_bytes = sizeof(int32) * history_size;
    bytes_read = is.read(reinterpret_cast<char*>(&(history[0])),
                         expected_bytes).gcount();
    if (bytes_read != expected_bytes) {
      std::cerr << "Failure reading IntLmState history, expected "
                << expected_bytes << " bytes, got "
                << bytes_read;
      exit(1);
    }
  }
  counts.resize(num_counts);
  assert(sizeof(std::pair<int32,int32>) == 8);

  size_t expected_bytes = sizeof(int32) * 2 * num_counts;
  bytes_read = is.read(reinterpret_cast<char*>(&(counts[0])),
                       expected_bytes).gcount();
  if (bytes_read != expected_bytes) {
    std::cerr << "Failure reading IntLmState counts, expected "
              << expected_bytes << " bytes, got "
              << bytes_read;
    exit(1);
  }
  if (rand() % 10 == 0)
    Check();
}

void IntLmState::Check() const {
  for (size_t i = 0; i < history.size(); i++)
    assert(history[i] > 0 && history[i] != static_cast<int32>(kEosSymbol));
  assert(counts.size() > 0);
  for (size_t i = 0; i < counts.size(); i++) {
    assert(counts[i].first > 0 && counts[i].first != kBosSymbol);
    assert(counts[i].second > 0);
    if (i + 1 < counts.size())
      assert(counts[i].first < counts[i+1].first);
  }
}

void FloatLmState::Write(std::ostream &os) const {
  int32 history_size = history.size(), num_counts = counts.size();
  assert(num_counts > 0);
  os.write(reinterpret_cast<const char*>(&history_size), sizeof(int32));
  os.write(reinterpret_cast<const char*>(&num_counts), sizeof(int32));
  os.write(reinterpret_cast<const char*>(&total), sizeof(float));
  os.write(reinterpret_cast<const char*>(&discount), sizeof(float));
  if (history_size > 0) {
    os.write(reinterpret_cast<const char*>(&(history[0])),
             sizeof(int32) * history_size);
  }
  os.write(reinterpret_cast<const char*>(&(counts[0])),
           sizeof(std::pair<int32, float>) * num_counts);
  if (!os.good()) {
    std::cerr << "Failure writing FloatLmState to stream\n";
    exit(1);
  }
}

void FloatLmState::Read(std::istream &is) {
  int32 history_size, num_counts;
  is.read(reinterpret_cast<char*>(&history_size), sizeof(int32));
  is.read(reinterpret_cast<char*>(&num_counts), sizeof(int32));
  if (!is.good() || is.eof()) {
    std::cerr << "Failure reading FloatLmState from stream\n";
    exit(1);
  }
  if (history_size < 0 || history_size > 10000 || num_counts <= 0) {
    std::cerr << "Failure reading FloatLmState from stream: "
        "got implausible data (wrong input?)\n";
    exit(1);
  }
  is.read(reinterpret_cast<char*>(&total), sizeof(float));
  is.read(reinterpret_cast<char*>(&discount), sizeof(float));
  history.resize(history_size);
  counts.resize(num_counts);
  if (history_size > 0) {
    is.read(reinterpret_cast<char*>(&(history[0])),
            sizeof(int32) * history_size);
  }
  is.read(reinterpret_cast<char*>(&(counts[0])),
          sizeof(std::pair<int32, float>) * num_counts);
  if (!is.good()) {
    std::cerr << "Failure reading FloatLmState from stream\n";
    exit(1);
  }
  if (rand() % 10 == 0)
    Check();
}

void FloatLmState::Check() const {
  for (size_t i = 0; i < history.size(); i++)
    assert(history[i] > 0 && history[i] != static_cast<int32>(kEosSymbol));
  assert(counts.size() > 0);
  for (size_t i = 0; i < counts.size(); i++) {
    assert(counts[i].first > 0 && counts[i].first != kBosSymbol);
    if (i + 1 < counts.size())
      assert(counts[i].first < counts[i+1].first);
  }
}

void FloatLmState::Print(std::ostream &os) const {
  os << " [ ";
  for (int32 i = 0; i < history.size(); i++)
    os << history[i] << " ";
  os << "]: ";
  os << "total=" << total << " discount=" << discount << " ";
  for (int32 i = 0; i < counts.size(); i++)
    os << counts[i].first << "->" << counts[i].second << " ";
  os << "\n";
}


void GeneralLmState::Print(std::ostream &os) const {
  os << " [ ";
  for (int32 i = 0; i < history.size(); i++)
    os << history[i] << " ";
  os << "]: ";
  for (int32 i = 0; i < counts.size(); i++)
    os << counts[i].first << "->" << counts[i].second << " ";
  os << "\n";
}

void GeneralLmState::Write(std::ostream &os) const {
  if (rand() % 10 == 0)
    Check();
  int32 history_size = history.size(),
      num_counts = counts.size();
  assert(num_counts > 0);
  os.write(reinterpret_cast<const char*>(&history_size), sizeof(int32));
  os.write(reinterpret_cast<const char*>(&num_counts), sizeof(int32));
  if (history_size > 0) {
    os.write(reinterpret_cast<const char*>(&(history[0])),
             sizeof(int32) * history_size);
  }
  size_t pair_size = sizeof(std::pair<int32, Count>);
  // We don't check that this size equals sizeof(int32) + 4 * sizeof(float).
  // Thus, in principle there could be some kind of padding, and we'd be
  // wasting some space on disk (and also run the risk of this not being
  // readable on a different architecture), but these are only
  // intermediate files used on a single machine-- the final output
  // of this toolkit is going to be a text ARPA file.

  os.write(reinterpret_cast<const char*>(&(counts[0])),
           pair_size * num_counts);

  if (!os.good()) {
    std::cerr << "Failure writing GeneralLmState to stream\n";
    exit(1);
  }
}

void GeneralLmState::Read(std::istream &is) {
  int32 history_size, num_counts;

  size_t bytes_read = is.read(reinterpret_cast<char*>(&history_size),
                              sizeof(int32)).gcount();
  if (bytes_read != sizeof(int32)) {
    std::cerr << "Failure reading GeneralLmState, expected 4 bytes, got "
              << bytes_read;
    exit(1);
  }
  if (history_size > 10000 || history_size < 0) {
    std::cerr << "Reading GeneralLmState, expected history size, got "
              << history_size << " (attempting to read wrong file type?)\n";
    exit(1);
  }
  bytes_read = is.read(reinterpret_cast<char*>(&num_counts),
                       sizeof(int32)).gcount();
  if (bytes_read != sizeof(int32)) {
    std::cerr << "Failure reading GeneralLmState, expected 4 bytes, got "
              << bytes_read;
    exit(1);
  }
  if (num_counts <= 0) {
    std::cerr << "Reading GeneralLmState, expected num-counts, got "
              << num_counts << " (attempting to read wrong file type?)\n";
    exit(1);
  }
  history.resize(history_size);
  if (history_size > 0) {
    size_t expected_bytes = sizeof(int32) * history_size;
    bytes_read = is.read(reinterpret_cast<char*>(&(history[0])),
                         expected_bytes).gcount();
    if (bytes_read != expected_bytes) {
      std::cerr << "Failure reading GeneralLmState history, expected "
                << expected_bytes << " bytes, got "
                << bytes_read;
      exit(1);
    }
  }
  counts.resize(num_counts);
  size_t pair_size = sizeof(std::pair<int32, Count>);

  size_t expected_bytes = pair_size * num_counts;
  bytes_read = is.read(reinterpret_cast<char*>(&(counts[0])),
                       expected_bytes).gcount();
  if (bytes_read != expected_bytes) {
    std::cerr << "Failure reading GeneralLmState counts, expected "
              << expected_bytes << " bytes, got "
              << bytes_read;
    exit(1);
  }
  if (rand() % 10 == 0)
    Check();
}


void GeneralLmState::Check() const {
  for (size_t i = 0; i < history.size(); i++)
    assert(history[i] > 0 && history[i] != static_cast<int32>(kEosSymbol));
  assert(counts.size() > 0);
  for (size_t i = 0; i < counts.size(); i++) {
    assert(counts[i].first > 0 && counts[i].first != kBosSymbol);
    if (i + 1 < counts.size())
      assert(counts[i].first < counts[i+1].first);
    // don't do any further checking on the counts, as they could be derivatives
    // and wouldn't pass the normal checks such as for positivity.
  }
}

void GeneralLmStateBuilder::Clear() {
  word_to_pos.clear();
  counts.clear();
}

void GeneralLmStateBuilder::AddCount(int32 word, float count) {
  int32 cur_counts_size = counts.size();
  std::pair<unordered_map<int32, int32>::iterator, bool> pr =
      word_to_pos.insert(std::pair<const int32, int32>(word,
                                                       cur_counts_size));
  if (pr.second) { // inserted an element.
    counts.push_back(Count(count));
  } else {
    int32 pos = pr.first->second;
    assert(pos < cur_counts_size);
    counts[pos].Add(count);
  }
}


void GeneralLmStateBuilder::AddCount(int32 word, float scale, int32 num_pieces) {
  int32 cur_counts_size = counts.size();
  std::pair<unordered_map<int32, int32>::iterator, bool> pr =
      word_to_pos.insert(std::pair<const int32, int32>(word,
                                                       cur_counts_size));
  if (pr.second) { // inserted an element.
    counts.push_back(Count(scale, num_pieces));
  } else {
    int32 pos = pr.first->second;
    assert(pos < cur_counts_size);
    counts[pos].Add(scale, num_pieces);
  }
}


void GeneralLmStateBuilder::AddCounts(const IntLmState &lm_state, float scale) {
  for (std::vector<std::pair<int32, int32> >::const_iterator iter =
           lm_state.counts.begin(); iter != lm_state.counts.end();
       ++iter)
    AddCount(iter->first, scale, iter->second);
}
void GeneralLmStateBuilder::AddCount(int32 word, const Count &count) {
  int32 cur_counts_size = counts.size();
  std::pair<unordered_map<int32, int32>::iterator, bool> pr =
      word_to_pos.insert(std::pair<const int32, int32>(word, cur_counts_size));
  if (pr.second) {
    // we inserted a new element into the word->position hash.
    counts.push_back(count);
  } else {
    int32 pos = pr.first->second;
    assert(pos < cur_counts_size);
    counts[pos].Add(count);
  }
}
void GeneralLmStateBuilder::AddCounts(const GeneralLmState &lm_state) {
  for (std::vector<std::pair<int32, Count> >::const_iterator iter =
           lm_state.counts.begin(); iter != lm_state.counts.end();
       ++iter)
    AddCount(iter->first, iter->second);
}

void GeneralLmStateBuilder::Output(
    std::vector<std::pair<int32, Count> > *output) const {
  size_t size = counts.size();
  assert(counts.size() == word_to_pos.size());
  std::vector<std::pair<int32, int32> > pairs;
  pairs.reserve(size);
  for (unordered_map<int32, int32>::const_iterator iter =
           word_to_pos.begin(); iter != word_to_pos.end(); ++iter)
    pairs.push_back(std::pair<int32,int32>(iter->first, iter->second));
  std::sort(pairs.begin(), pairs.end());
  output->clear();
  output->resize(size);
  for (size_t i = 0; i < size; i++) {
    (*output)[i].first = pairs[i].first;
    (*output)[i].second = counts[pairs[i].second];
  }
}

}

