#include "search_server.h"
#include "iterator_range.h"
#include "parse.h"

#include "profile.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>
#include <numeric>

/*Inverted index functions start*/

InvertedIndex::InvertedIndex(istream& document_input) {
  for (string current_doc; getline(document_input, current_doc); ) {
    docs++;
    const size_t docid = Index();
    auto doc_split = SplitIntoWords(current_doc);
    for (const auto& word : doc_split) {
      auto& temp = index[word];
      if (temp.empty() || temp.back().get_docid() != docid) {
        temp.push_back({docid, 1u});
      } else {
        temp.back().increase_hitcount();
      }
    }
  }
}

const vector<InvertedIndex::DocWordStat>& InvertedIndex::Lookup(const string& word) const {
  if (auto it = index.find(word); it != index.end()) {
    return it->second;
  } else {
    static const vector<DocWordStat> empty_;
    return empty_;
  }
}

/*Inverted index functions end*/

void UpdateIndex(istream& document_input, Synchronized<InvertedIndex>& index) {
  InvertedIndex new_index(document_input);
  swap(index.GetAccess().ref_to_value, new_index);
}

void SearchIndex(istream& query_input, ostream& search_results_output, Synchronized<InvertedIndex>& index) {
  vector<size_t> docid_count;
  vector<int64_t> docids;

  for (string current_query; getline(query_input, current_query); ) {
    const auto words = SplitIntoWords(current_query);

    {
        auto access = index.GetAccess();

        const size_t N = access.ref_to_value.Index();
        docid_count.assign(N, 0); 
        docids.resize(N);

        auto &ind = access.ref_to_value;
        for (const auto& word : words) {
          for (const auto& id : ind.Lookup(word)) {
            docid_count[id.get_docid()] += id.get_hitcount();
        }
      }
    }

    iota(docids.begin(), docids.end(), 0);
    {
        partial_sort(begin(docids), Head(docids, 5).end(), end(docids), 
        [&docid_count](int64_t lhs, int64_t rhs) {
          return pair(docid_count[lhs], -lhs) > pair(docid_count[rhs], -rhs);
      });
    }
    

    search_results_output << current_query << ':';
    for (size_t docid : Head(docids, 5)) {
      size_t hitcount = docid_count[docid];
      if (hitcount == 0) break;
      search_results_output << " {" << "docid: " << docid << ", " << "hitcount: " << hitcount << '}';
    }
    search_results_output << "\n";
  }
}

void SearchServer::UpdateDocumentBase(istream& document_input) {
  tasks.push_back(async(UpdateIndex, ref(document_input), ref(index)));
}

void SearchServer::AddQueriesStream(istream& query_input, ostream& search_results_output) {
  tasks.push_back(async(SearchIndex, ref(query_input), ref(search_results_output), ref(index))); 
}
