
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace fs = std::filesystem;

struct SortRule {
    char type;
    bool reverse;
};

struct Options {
    bool pause = false;
    bool wide = false;
    bool recursive = false;
    bool bare = false;
    bool lowercase = false;
    bool compression = false;
    bool hostCompression = false;

    char sort = 'N';
    bool reverse = false;

    std::string path = ".";
    std::string pattern = "*";
};


std::vector<SortRule> sortRules;

void parseSort(const std::string& value) {
    sortRules.clear();

    if (value.empty()) {
        sortRules.push_back({'G', false});
        sortRules.push_back({'N', false});
        return;
    }

    bool reverse = false;

    for (char c : value) {
        if (c == '-') {
            reverse = true;
            continue;
        }

        c = std::toupper(static_cast<unsigned char>(c));

        if (c == 'N' || c == 'E' ||
            c == 'D' || c == 'S' || c == 'G') {
            sortRules.push_back({c, reverse});
        }

        reverse = false;
    }
}

uintmax_t getSize(const fs::directory_entry& e) {
    std::error_code ec;
    auto size = e.file_size(ec);
    return ec ? 0 : size;
}

void sortEntries(std::vector<fs::directory_entry>& entries) {
    std::stable_sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b) {
            for (const auto& rule : sortRules) {
                int comparison = 0;

                switch (rule.type) {
                    case 'G': {
                        bool ad = a.is_directory();
                        bool bd = b.is_directory();

                        if (ad != bd) {
                            return rule.reverse ? !ad : ad;
                        }
                        continue;
                    }

                    case 'N': {
                        std::string an = a.path().filename().string();
                        std::string bn = b.path().filename().string();

                        std::transform(an.begin(), an.end(), an.begin(),
                            [](unsigned char c) { return std::tolower(c); });

                        std::transform(bn.begin(), bn.end(), bn.begin(),
                            [](unsigned char c) { return std::tolower(c); });

                        comparison = an.compare(bn);
                        break;
                    }

                    case 'E':
                        comparison =
                            a.path().extension().string().compare(
                            b.path().extension().string());
                        break;

                    case 'S': {
                        auto as = getSize(a);
                        auto bs = getSize(b);

                        comparison = as < bs ? -1 : as > bs ? 1 : 0;
                        break;
                    }

                    case 'D': {
                        std::error_code ec1, ec2;

                        auto ad = a.last_write_time(ec1);
                        auto bd = b.last_write_time(ec2);

                        if (ec1 || ec2) continue;

                        comparison = ad < bd ? -1 : ad > bd ? 1 : 0;
                        break;
                    }
                }

                if (comparison != 0)
                    return rule.reverse
                        ? comparison > 0
                        : comparison < 0;
            }

            return false;
        });
}


std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return s;
}

bool wildcardMatch(const std::string& pattern,
                   const std::string& name) {
    std::string p = upper(pattern);
    std::string n = upper(name);

    size_t i = 0, j = 0;
    size_t star = std::string::npos, mark = 0;

    while (j < n.size()) {
        if (i < p.size() &&
            (p[i] == '?' || p[i] == n[j])) {
            i++;
            j++;
        }
        else if (i < p.size() && p[i] == '*') {
            star = i++;
            mark = j;
        }
        else if (star != std::string::npos) {
            i = star + 1;
            j = ++mark;
        }
        else return false;
    }

    while (i < p.size() && p[i] == '*') i++;

    return i == p.size();
}

int main(int argc, char* argv[]) {
    Options opt;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg.size() > 1 && (arg[0] == '/' || arg[0] == '-')) {
            std::string sw = upper(arg.substr(1));

            if (sw == "P") opt.pause = true;
            else if (sw == "W") opt.wide = true;
            else if (sw == "S") opt.recursive = true;
            else if (sw == "B") opt.bare = true;
            else if (sw == "L") opt.lowercase = true;
            else if (sw == "C") opt.compression = true;
            else if (sw == "CH") opt.hostCompression = true;

            else if (sw.rfind("O:", 0) == 0) {
                std::string order = sw.substr(2);
                if (!order.empty()) {
                    opt.reverse = order[0] == '-';
                    opt.sort = opt.reverse ? order[1] : order[0];
                }
            }

            else if (sw == "O") {
                parseSort("");
            }
            else if (sw.rfind("O:", 0) == 0) {
                parseSort(sw.substr(2));
            }
            else if (sw == "-P") {
                opt.pause = false;
            }
            else if (sw == "-W") {
                opt.wide = false;
            }
            else if (sw == "P") {
                opt.pause = true;
            }
            else if (sw == "W") {
                opt.wide = true;
            }
        }
        else {
            std::string input = arg;
            fs::path p(input);

            if (input.find('*') != std::string::npos ||
                input.find('?') != std::string::npos) {
                opt.path = p.has_parent_path()
                    ? p.parent_path().string() : ".";
                opt.pattern = p.filename().string();
            }
            else {
                opt.path = input;
            }
        }
    }

    try {
        std::vector<fs::directory_entry> entries;

        auto addEntry = [&](const fs::directory_entry& e) {
            if (wildcardMatch(opt.pattern,
                              e.path().filename().string())) {
                entries.push_back(e);
            }
        };

        if (opt.recursive) {
            for (const auto& e :
                 fs::recursive_directory_iterator(opt.path)) {
                addEntry(e);
            }
        } else {
            for (const auto& e :
                 fs::directory_iterator(opt.path)) {
                addEntry(e);
            }
        }

        std::sort(entries.begin(), entries.end(),
            [&](const auto& a, const auto& b) {
                bool result = false;

                switch (opt.sort) {
                    case 'S':
                        result = fs::file_size(a) <
                                 fs::file_size(b);
                        break;
                    case 'E':
                        result = a.path().extension().string() <
                                 b.path().extension().string();
                        break;
                    case 'D':
                        result = fs::last_write_time(a) <
                                 fs::last_write_time(b);
                        break;
                    case 'N':
                    default:
                        result = upper(a.path().filename().string()) <
                                 upper(b.path().filename().string());
                        break;
                }

                return opt.reverse ? !result : result;
            });

        for (size_t i = 0; i < entries.size(); i++) {
            std::string name =
                entries[i].path().filename().string();

            if (opt.lowercase)
                std::transform(name.begin(), name.end(),
                    name.begin(), [](unsigned char c) {
                        return std::tolower(c);
                    });

            if (opt.bare) {
                std::cout << name << '\n';
            }
            else {
                std::cout << name;

                if (entries[i].is_directory())
                    std::cout << " <DIR>";
                else if (entries[i].is_regular_file())
                    std::cout << " " << fs::file_size(entries[i])
                              << " bytes";

                std::cout << '\n';
            }

            if (opt.pause && (i + 1) % 20 == 0) {
                std::cout << "Press Enter to continue...";
                std::cin.get();
            }
        }

    } catch (const fs::filesystem_error& e) {
        std::cerr << "DIR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
