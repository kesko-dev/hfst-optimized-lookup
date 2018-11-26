//       This program is free software: you can redistribute it and/or modify
//       it under the terms of the GNU General Public License as published by
//       the Free Software Foundation, version 3 of the License.
//
//       This program is distributed in the hope that it will be useful,
//       but WITHOUT ANY WARRANTY; without even the implied warranty of
//       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//       GNU General Public License for more details.
//
//       You should have received a copy of the GNU General Public License
//       along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "./transducer.h"

#ifndef MAIN_TEST

namespace hfst_ol {

    bool should_ascii_tokenize(unsigned char c) {
        return ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z'));
    }

    void skip_hfst3_header(std::istream &is) {
        const char *header1 = "HFST";
        unsigned int header_loc = 0; // how much of the header has been found
        int c;
        for (header_loc = 0; header_loc < strlen(header1) + 1; header_loc++) {
            c = is.get();
            if (c != header1[header_loc]) {
                break;
            }
        }
        if (header_loc == strlen(header1) + 1) // we found it
        {
            unsigned short remaining_header_len;
            is.read(reinterpret_cast<char *>(&remaining_header_len),
                    sizeof(remaining_header_len));
            if (is.get() != '\0' || is.fail()) {
                HFST_THROW(TransducerHasWrongTypeException);
            }
            char *headervalue = new char[remaining_header_len];
            is.read(headervalue, remaining_header_len);
            if (headervalue[remaining_header_len - 1] != '\0' || is.fail()) {
                HFST_THROW(TransducerHasWrongTypeException);
            }
            std::string header_tail(headervalue, remaining_header_len);
            size_t type_field = header_tail.find("type");
            if (type_field != std::string::npos) {
                if (header_tail.find("HFST_OL") != type_field + 5 &&
                    header_tail.find("HFST_OLW") != type_field + 5) {
                    delete[] headervalue;
                    HFST_THROW(TransducerHasWrongTypeException);
                }
            }
        } else // nope. put back what we've taken
        {
            is.putback(c); // first the non-matching character
            for (int i = header_loc - 1; i >= 0; i--) {
                // then the characters that did match (if any)
                is.putback(header1[i]);
            }
        }
    }

    TransducerAlphabet::TransducerAlphabet(std::istream &is,
                                           SymbolNumber symbol_count) {
        unknown_symbol = NO_SYMBOL_NUMBER;
        for (SymbolNumber i = 0; i < symbol_count; i++) {
            std::string str;
            std::getline(is, str, '\0');
            symbol_table.push_back(str.c_str());
            if (hfst::FdOperation::is_diacritic(str)) {
                fd_table.define_diacritic(i, str);
            } else if (hfst::is_unknown(str)) {
                unknown_symbol = i;
            }
            if (!is) {
                HFST_THROW(TransducerHasWrongTypeException);
            }
        }
    }

    TransducerAlphabet::TransducerAlphabet(const SymbolTable &st)
            : symbol_table(st) {
        unknown_symbol = NO_SYMBOL_NUMBER;
        for (SymbolNumber i = 0; i < symbol_table.size(); i++) {
            if (hfst::FdOperation::is_diacritic(symbol_table[i])) {
                fd_table.define_diacritic(i, symbol_table[i]);
            } else if (hfst::is_unknown(symbol_table[i])) {
                unknown_symbol = i;
            }
        }
    }

    StringSymbolMap TransducerAlphabet::build_string_symbol_map(void) const {
        StringSymbolMap ss_map;
        for (SymbolNumber i = 0; i < symbol_table.size(); ++i) {
            ss_map[symbol_table[i]] = i;
        }
        return ss_map;
    }

    void TransducerAlphabet::display() const {
        std::cout << "Transducer alphabet:" << std::endl;
        for (size_t i = 0; i < symbol_table.size(); i++)
            std::cout << " Symbol " << i << ": " << symbol_table[i] << std::endl;
    }

    void TransitionIndex::display() const {
        std::cout << "input_symbol: " << input_symbol
                  << ", target: " << first_transition_index
                  << (final() ? " (final)" : "") << std::endl;
    }
    void Transition::display() const {
        std::cout << "input_symbol: " << input_symbol
                  << ", output_symbol: " << output_symbol
                  << ", target: " << target_index << (final() ? " (final)" : "")
                  << std::endl;
    }
    void TransitionW::display() const {
        std::cout << "input_symbol: " << input_symbol
                  << ", output_symbol: " << output_symbol
                  << ", target: " << target_index << ", weight: " << transition_weight
                  << (final() ? " (final)" : "") << std::endl;
    }

    bool TransitionIndex::matches(SymbolNumber s) const {
        return input_symbol != NO_SYMBOL_NUMBER && input_symbol == s;
    }
    bool Transition::matches(SymbolNumber s) const {
        return input_symbol != NO_SYMBOL_NUMBER && input_symbol == s;
    }

    bool TransitionIndex::final() const {
        return input_symbol == NO_SYMBOL_NUMBER &&
               first_transition_index != NO_TABLE_INDEX;
    }
    bool Transition::final() const {
        return input_symbol == NO_SYMBOL_NUMBER &&
               output_symbol == NO_SYMBOL_NUMBER && target_index == 1;
    }

    Weight TransitionWIndex::final_weight(void) const {
        union to_weight {
            TransitionTableIndex i;
            Weight w;
        } weight;
        weight.i = first_transition_index;
        return weight.w;
    }

    void OlLetterTrie::add_string(const char *p, SymbolNumber symbol_key) {
        if (*(p + 1) == 0) {
            symbols[(unsigned char)(*p)] = symbol_key;
            return;
        }
        if (letters[(unsigned char)(*p)] == NULL) {
            letters[(unsigned char)(*p)] = new OlLetterTrie();
        }
        letters[(unsigned char)(*p)]->add_string(p + 1, symbol_key);
    }

    SymbolNumber OlLetterTrie::find_key(char **p) {
        const char *old_p = *p;
        ++(*p);
        if (letters[(unsigned char)(*old_p)] == NULL) {
            return symbols[(unsigned char)(*old_p)];
        }
        SymbolNumber s = letters[(unsigned char)(*old_p)]->find_key(p);
        if (s == NO_SYMBOL_NUMBER) {
            --(*p);
            return symbols[(unsigned char)(*old_p)];
        }
        return s;
    }

    void Encoder::read_input_symbols(const SymbolTable &kt) {
        for (SymbolNumber k = 0; k < number_of_input_symbols; ++k) {
            const char *p = kt[k].c_str();
            if ((strlen(p) == 1) && should_ascii_tokenize((unsigned char)(*p))) {
                ascii_symbols[(unsigned char)(*p)] = k;
            }
            letters.add_string(p, k);
        }
    }

    SymbolNumber Encoder::find_key(char **p) {
        if (ascii_symbols[(unsigned char)(**p)] == NO_SYMBOL_NUMBER) {
            return letters.find_key(p);
        }
        SymbolNumber s = ascii_symbols[(unsigned char)(**p)];
        ++(*p);
        return s;
    }

    bool Transducer::initialize_input(const char *input_str) {
        char *c = strdup(input_str);
        char *c_orig = c;
        int i = 0;
        SymbolNumber k = NO_SYMBOL_NUMBER;
        for (char **Str = &c; **Str != 0;) {
            k = encoder->find_key(Str);
            if (k == NO_SYMBOL_NUMBER) {
                free(c_orig);
                return false; // tokenization failed
            }
            input_tape[i] = k;
            ++i;
        }
        input_tape[i] = NO_SYMBOL_NUMBER;
        free(c_orig);
        return true;
    }

    std::string remove_tags(const std::string &input, const std::regex &tags_regex) {
        // Sample input: @D.NEED@@P.NEED.REST@tuo<Pron><Dem><Sg><Par>@D.LOWERCASED@<cap>
        // Corresponding output: tuo
        // Apparently there are tags like @THIS@ and <this> which we should remove.
        // Also, compounds are sometimes marked with + and sometimes with +#
        // so just remove all occurrences of either character.
        //const std::regex re ("@[^@]*@|<[^>]*>|[+#]|\\[[^\\]]*\\]");
        return std::regex_replace(input, tags_regex, "");
    }

    /*
    std::vector<std::vector<std::string> > Transducer::multi_doc_lookup(const std::vector<std::string> &dokit)
    {
        boost::xpressive::sregex wordsplit = +boost::xpressive::_s;
        // tokens:
        std::vector<std::vector<std::string> > docs(dokit.size());

        #pragma omp parallel for
        for (uint32_t doc_seq = 0; doc_seq < docs.size(); ++doc_seq) {
            const auto & raw_text = dokit[doc_seq];
            boost::xpressive::sregex_token_iterator begin(raw_text.cbegin(), raw_text.cend(), wordsplit, -1), end;
            docs[doc_seq].assign(begin, end);
        }

        std::unordered_set<std::string> unseen_tokens;

        // look up all unseen words
        for (auto & doc : docs) {
            for (auto & token :doc) {
                if (!cached_results.count(token)) {
                    unseen_tokens.insert(token);
                }
            }
        }

        for (auto &token : unseen_tokens) {
            const auto lookup_result = this->lookup(token);
            if (lookup_result.size()) {
                // just pick one result:
                const auto & last = lookup_result.back().first;
                cached_results[token] = last;
            } else {
                // write not found tokens as-is:
                cached_results[token] = token;
            }
        }

        #pragma omp parallel for
        for (uint32_t doc_seq = 0U; doc_seq < docs.size(); ++doc_seq) {
            auto &doc = docs[doc_seq];
            for (uint32_t token_seq = 0U; token_seq < doc.size(); ++token_seq) {
                auto find_it = cached_results.find(doc[token_seq]);
                if (find_it != cached_results.cend()) {
                     doc[token_seq] = remove_tags(find_it->second, this->tags_regex);
                } else if (doc[token_seq].length() < 2) {
                    // remove all strings with size < 2:
                    doc[token_seq] = "";
                }
            }
        }
        return docs;
    }
    */

    std::vector<std::string> Transducer::multi_lookup(const StringVector &strs) {
        std::vector<std::string> result(strs.size());
        for (size_t it = 0; it < strs.size(); ++it) {
            if (cached_results.count(strs[it])) {
                result[it] = cached_results[strs[it]];
            } else {
                auto lookup_result = this->lookup(strs[it]);
                if (lookup_result.size()) {
                    // just pick one result:
                    result[it] = lookup_result.back().first;
                    cached_results[strs[it]] = result[it];
                    // debug:
                    // std::cout<<result[it]<<std::endl;
                } else {
                    //return original token
                    result[it] = strs[it];
                }
            }
        }
        // post process: just get the word + word type

        #pragma omp parallel for
        for (size_t it = 0; it < result.size(); ++it) {
            result[it] = remove_tags(result[it], this->tags_regex);
        }
        // remove all empty strings:
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [&](std::string &s) { return s.size() < 2; }),
                     result.end());
        return result;
    }

    void Transducer::write_lookup_cache() {
        FILE *fp = fopen("hfst_lookup_cache.ssv", "w");
        for (auto it = cached_results.cbegin(); it != cached_results.cend(); ++it) {
            fprintf(fp, "%s %s\n", it->first.c_str(), it->second.c_str());
        }
        fclose(fp);
    }

    std::vector<std::pair<std::string, Weight> >
    Transducer::lookup(const std::string &s) {
        return lookup(s.c_str());
    }

    std::vector<std::pair<std::string, Weight> > Transducer::lookup(const char *s) {
        lookup_results.clear();
        if (!initialize_input(s)) {
            return lookup_results;
        }
        // current_weight += s.second;
        get_analyses(input_tape, output_tape, output_tape, 0);
        // current_weight -= s.second;
        return std::vector<std::pair<std::string, Weight> >(lookup_results);
    }

    void Transducer::try_epsilon_transitions(SymbolNumber *input_symbol,
                                             SymbolNumber *output_symbol,
                                             SymbolNumber *original_output_tape,
                                             TransitionTableIndex i) {
        //        std::cerr << "try_epsilon_transitions, index " << i << std::endl;
        while (true) {
            if (tables->get_transition_input(i) == 0) // epsilon
            {
                *output_symbol = tables->get_transition_output(i);
                current_weight += tables->get_weight(i);
                get_analyses(input_symbol, output_symbol + 1, original_output_tape,
                             tables->get_transition_target(i));
                current_weight -= tables->get_weight(i);
                ++i;
            } else if (alphabet->is_flag_diacritic(tables->get_transition_input(i))) {
                std::vector<short> old_values(flag_state.get_values());
                if (flag_state.apply_operation(
                        *(alphabet->get_operation(tables->get_transition_input(i))))) {
                    // flag diacritic allowed
                    *output_symbol = tables->get_transition_output(i);
                    current_weight += tables->get_weight(i);
                    get_analyses(input_symbol, output_symbol + 1, original_output_tape,
                                 tables->get_transition_target(i));
                    current_weight -= tables->get_weight(i);
                }
                flag_state.assign_values(old_values);
                ++i;
            } else { // it's not epsilon and it's not a flag, so nothing to do
                return;
            }
        }
    }

    void Transducer::try_epsilon_indices(SymbolNumber *input_symbol,
                                         SymbolNumber *output_symbol,
                                         SymbolNumber *original_output_tape,
                                         TransitionTableIndex i) {
        //    std::cerr << "try_epsilon_indices, index " << i << std::endl;
        if (tables->get_index_input(i) == 0) {
            try_epsilon_transitions(input_symbol, output_symbol, original_output_tape,
                                    tables->get_index_target(i) -
                                    TRANSITION_TARGET_TABLE_START);
        }
    }

    void Transducer::find_transitions(SymbolNumber input,
                                      SymbolNumber *input_symbol,
                                      SymbolNumber *output_symbol,
                                      SymbolNumber *original_output_tape,
                                      TransitionTableIndex i) {

        while (tables->get_transition_input(i) != NO_SYMBOL_NUMBER) {
            if (tables->get_transition_input(i) == input) {

                *output_symbol = tables->get_transition_output(i);
                current_weight += tables->get_weight(i);
                get_analyses(input_symbol, output_symbol + 1, original_output_tape,
                             tables->get_transition_target(i));
                current_weight -= tables->get_weight(i);
            } else {
                return;
            }
            ++i;
        }
    }

    void Transducer::find_index(SymbolNumber input, SymbolNumber *input_symbol,
                                SymbolNumber *output_symbol,
                                SymbolNumber *original_output_tape,
                                TransitionTableIndex i) {
        if (tables->get_index_input(i + input) == input) {
            find_transitions(input, input_symbol, output_symbol, original_output_tape,
                             tables->get_index_target(i + input) -
                             TRANSITION_TARGET_TABLE_START);
        }
    }

    void Transducer::get_analyses(SymbolNumber *input_symbol,
                                  SymbolNumber *output_symbol,
                                  SymbolNumber *original_output_tape,
                                  TransitionTableIndex i) {
        if (indexes_transition_table(i)) {
            i -= TRANSITION_TARGET_TABLE_START;

            try_epsilon_transitions(input_symbol, output_symbol, original_output_tape,
                                    i + 1);

            // input-string ended.
            if (*input_symbol == NO_SYMBOL_NUMBER) {
                *output_symbol = NO_SYMBOL_NUMBER;
                if (tables->get_transition_finality(i)) {
                    current_weight += tables->get_weight(i);
                    note_analysis(original_output_tape);
                    current_weight -= tables->get_weight(i);
                }
                return;
            }

            SymbolNumber input = *input_symbol;
            ++input_symbol;

            find_transitions(input, input_symbol, output_symbol, original_output_tape,
                             i + 1);
        } else {
            try_epsilon_indices(input_symbol, output_symbol, original_output_tape,
                                i + 1);

            if (*input_symbol == NO_SYMBOL_NUMBER) { // input-string ended.
                *output_symbol = NO_SYMBOL_NUMBER;
                if (tables->get_index_finality(i)) {
                    current_weight += tables->get_final_weight(i);
                    note_analysis(original_output_tape);
                    current_weight -= tables->get_final_weight(i);
                }
                return;
            }

            SymbolNumber input = *input_symbol;
            ++input_symbol;

            find_index(input, input_symbol, output_symbol, original_output_tape, i + 1);
        }
        *output_symbol = NO_SYMBOL_NUMBER;
    }

    void Transducer::note_analysis(SymbolNumber *whole_output_tape) {
        std::string result;
        for (SymbolNumber *num = whole_output_tape; *num != NO_SYMBOL_NUMBER; ++num) {
            result.append(alphabet->string_from_symbol(*num));
        }
        lookup_results.push_back(
                std::pair<std::string, Weight>(result, current_weight));
    }

    Transducer::Transducer(const std::string &filename) :
            cached_results(256000)
    {
        std::ifstream is(filename.c_str(), std::ifstream::in);
        // the other constructors throw exceptions if data can't be read at some point
        skip_hfst3_header(is);
        header = new TransducerHeader(is);
        alphabet = new TransducerAlphabet(is, header->symbol_count());
        tables = NULL;
        current_weight = 0.0;
        encoder =
                new Encoder(alphabet->get_symbol_table(), header->input_symbol_count());
        input_tape = (SymbolNumber *)(malloc(sizeof(SymbolNumber) * MAX_IO_LEN));
        output_tape = (SymbolNumber *)(malloc(sizeof(SymbolNumber) * MAX_IO_LEN));
        flag_state = alphabet->get_fd_table();
        load_tables(is);
        is.close();
        tags_regex = std::regex("@[^@]*@|<[^>]*>|[+#]|\\[[^\\]]*\\]");
    }

    Transducer::~Transducer() {
        delete header;
        delete alphabet;
        delete tables;
        delete encoder;
        free(input_tape);
        free(output_tape);
    }

    void Transducer::load_tables(std::istream &is) {
        if (header->probe_flag(Weighted))
            tables = new TransducerTables<TransitionWIndex, TransitionW>(
                    is, header->index_table_size(), header->target_table_size());
        else
            tables = new TransducerTables<TransitionIndex, Transition>(
                    is, header->index_table_size(), header->target_table_size());
        if (!is) {
            HFST_THROW(TransducerHasWrongTypeException);
        }
    }

    void Transducer::write(std::ostream &os) const {
        header->write(os);
        alphabet->write(os);
        for (size_t i = 0; i < header->index_table_size(); i++)
            tables->get_index(i).write(os, header->probe_flag(Weighted));
        for (size_t i = 0; i < header->target_table_size(); i++)
            tables->get_transition(i).write(os, header->probe_flag(Weighted));
    }

    void Transducer::display() const {
        std::cout << "-----Displaying optimized-lookup transducer------" << std::endl;
        header->display();
        alphabet->display();
        tables->display();
        std::cout << "-------------------------------------------------" << std::endl;
    }

    TransitionTableIndexSet
    Transducer::get_transitions_from_state(TransitionTableIndex state_index) const {
        TransitionTableIndexSet transitions;

        if (indexes_transition_index_table(state_index)) {
            // for each input symbol that has a transition from this state
            for (SymbolNumber symbol = 0; symbol < header->symbol_count(); symbol++) {
                // There may be flags at index 0 even if there aren't
                // any epsilons, so those have to be checked for
                if (alphabet->is_flag_diacritic(symbol)) {
                    TransitionTableIndex transition_i =
                            get_index(state_index + 1).get_target();
                    if (!get_index(state_index + 1).matches(0)) {
                        continue;
                    }
                    while (true) {
                        // First skip any epsilons
                        if (get_transition(transition_i).matches(0)) {
                            ++transition_i;
                            continue;
                        } else if (get_transition(transition_i).matches(symbol)) {
                            transitions.insert(transition_i);
                            ++transition_i;
                            continue;
                        } else {
                            break;
                        }
                    }
                } else { // not a flag
                    const TransitionIndex &test_transition_index =
                            get_index(state_index + 1 + symbol);
                    if (test_transition_index.matches(symbol)) {
                        // there are one or more transitions with this input symbol,
                        // starting at test_transition_index.get_target()
                        TransitionTableIndex transition_i =
                                test_transition_index.get_target();
                        while (true) {
                            if (get_transition(transition_i)
                                    .matches(test_transition_index.get_input_symbol())) {
                                transitions.insert(transition_i);
                            } else {
                                break;
                            }
                            ++transition_i;
                        }
                    }
                }
            }
        } else { // indexes transition table
            const Transition &transition = get_transition(state_index);
            if (transition.get_input_symbol() != NO_SYMBOL_NUMBER ||
                transition.get_output_symbol() != NO_SYMBOL_NUMBER) {
                std::cerr << "Oops" << std::endl;
                throw;
            }

            TransitionTableIndex transition_i = state_index + 1;
            while (true) {
                if (get_transition(transition_i).get_input_symbol() != NO_SYMBOL_NUMBER)
                    transitions.insert(transition_i);
                else
                    break;
                transition_i++;
            }
        }
        return transitions;
    }

    TransitionTableIndex Transducer::next(const TransitionTableIndex i,
                                          const SymbolNumber symbol) const {
        if (i >= TRANSITION_TARGET_TABLE_START) {
            return i - TRANSITION_TARGET_TABLE_START + 1;
        } else {
            return get_index(i + 1 + symbol).get_target() -
                   TRANSITION_TARGET_TABLE_START;
        }
    }

    bool Transducer::has_transitions(const TransitionTableIndex i,
                                     const SymbolNumber symbol) const {
        if (i >= TRANSITION_TARGET_TABLE_START) {
            return (
                    get_transition(i - TRANSITION_TARGET_TABLE_START).get_input_symbol() ==
                    symbol);
        } else {
            return (get_index(i + symbol).get_input_symbol() == symbol);
        }
    }

    bool Transducer::has_epsilons_or_flags(const TransitionTableIndex i) {
        if (i >= TRANSITION_TARGET_TABLE_START) {
            return (
                    get_transition(i - TRANSITION_TARGET_TABLE_START).get_input_symbol() ==
                    0 ||
                    is_flag(get_transition(i - TRANSITION_TARGET_TABLE_START)
                                    .get_input_symbol()));
        } else {
            return (get_index(i).get_input_symbol() == 0);
        }
    }

    STransition Transducer::take_epsilons(const TransitionTableIndex i) const {
        if (get_transition(i).get_input_symbol() != 0) {
            return STransition(0, NO_SYMBOL_NUMBER);
        }
        return STransition(get_transition(i).get_target(),
                           get_transition(i).get_output_symbol(),
                           get_transition(i).get_weight());
    }

    STransition Transducer::take_epsilons_and_flags(const TransitionTableIndex i) {
        if (get_transition(i).get_input_symbol() != 0 &&
            !is_flag(get_transition(i).get_input_symbol())) {
            return STransition(0, NO_SYMBOL_NUMBER);
        }
        return STransition(get_transition(i).get_target(),
                           get_transition(i).get_output_symbol(),
                           get_transition(i).get_weight());
    }

    STransition Transducer::take_non_epsilons(const TransitionTableIndex i,
                                              const SymbolNumber symbol) const {
        if (get_transition(i).get_input_symbol() != symbol) {
            return STransition(0, NO_SYMBOL_NUMBER);
        }
        return STransition(get_transition(i).get_target(),
                           get_transition(i).get_output_symbol(),
                           get_transition(i).get_weight());
    }

    Weight Transducer::final_weight(const TransitionTableIndex i) const {
        if (i >= TRANSITION_TARGET_TABLE_START) {
            return get_transition(i - TRANSITION_TARGET_TABLE_START).get_weight();
        } else {
            return get_index(i).final_weight();
        }
    }
}

#else // MAIN_TEST was defined

#include <iostream>

int main(int argc, char *argv[]) {
  std::cout << "Unit tests for " __FILE__ ":" << std::endl;

  std::cout << "ok" << std::endl;
  return 0;
}

#endif // MAIN_TEST
