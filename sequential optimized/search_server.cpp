#include "search_server.h"
#include "iterator_range.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>

vector<string> SplitIntoWords(const string& line) {
  istringstream words_input(move(line)); 
  return {istream_iterator<string>(words_input), istream_iterator<string>()};
}

/*vector<string_view> SplitIntoWords(string_view line) {
    vector<string_view> result;
    size_t curr = line.find_first_not_of(' ', 0);
    while (true){
        auto space = line.find(' ', curr);
        result.emplace_back(line.substr(curr, space - curr));

        if (space == line.npos) break;
        else curr = line.find_first_not_of(' ', space);
        if (curr == line.npos) break;
    }
    return result;
}*/

SearchServer::SearchServer(istream& document_input) {
  UpdateDocumentBase(document_input);
}

void SearchServer::UpdateDocumentBase(istream& document_input) {
  InvertedIndex new_index;

  for (string current_document; getline(document_input, current_document); ) {
    new_index.Add(move(current_document));
  }

  index = move(new_index);
}

void SearchServer::AddQueriesStream(istream& query_input, ostream& search_results_output) {
  vector<pair<size_t, size_t>> docid_count(50'000);

  for (string current_query; getline(query_input, current_query); ) {
    const auto words = SplitIntoWords(move(current_query));

    for (const auto& word : words) {
      auto indx_lookup = index.Lookup(word); 
      for (const auto& docid : indx_lookup) {
        const auto ind = docid.get_docid();
        docid_count[ind] = {ind, docid_count[ind].second + docid.get_hitcount()};
      }
    }

    partial_sort(begin(docid_count), begin(docid_count)+5, end(docid_count),
      [](pair<size_t, size_t> lhs, pair<size_t, size_t> rhs) {
        int64_t lhs_docid = lhs.first;
        auto lhs_hit_count = lhs.second;
        int64_t rhs_docid = rhs.first;
        auto rhs_hit_count = rhs.second;
        return make_pair(lhs_hit_count, -lhs_docid) > make_pair(rhs_hit_count, -rhs_docid);
      }
    );

    auto head = Head(docid_count, 5);

    search_results_output << current_query << ':';
    for (auto [docid, hitcount] : head) {
      if (hitcount == 0) break;
      search_results_output << " {"
        << "docid: " << docid << ", "
        << "hitcount: " << hitcount << '}';
    }
    search_results_output << endl;

    for(size_t i = 0; i < 50'000; i++)
      docid_count[i] = {0,0};
  }
}

/*Inverted index functions*/

void InvertedIndex::Add(const string& document) {
  docs++;

  const size_t docid = Index();
  auto doc_split = SplitIntoWords(move(document));
  for (const auto& word : doc_split) {
    auto& temp = index[word];
    if (temp.empty() || temp.back().get_docid() != docid) {
      temp.push_back(DocWordStat{docid, 1u});
    } else {
      temp.back().increase_hitcount();
    }
  }
}

const vector<DocWordStat>& InvertedIndex::Lookup(const string& word) const { 
  if (auto it = index.find(word); it != index.end()) {
    return it->second;
  } else {
    static const vector<DocWordStat> empty_;
    return empty_;
  }
}
