#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>
#include <cctype>
#include <algorithm>

enum LINE{
    EMPTY,
    INFO,
    CHORD,
    LYRIC
};

bool DEBUG = false;

void debugMsg(std::string msg) {
    if (DEBUG) {
        std::cout << msg << std::endl;
    }
}

LINE getLineType(const std::string& line) {
    // Trim leading/trailing spaces (optional)
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t")); // Trim left
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1); // Trim right

    // Check for empty line
    if (trimmed.empty()) {
        return LINE::EMPTY;
    }
    
    // Check if it's a CHORD line (contains only valid chord patterns or spaces)
    static const std::regex chord_regex(R"(^\s*([A-G][#b]?(m|maj|min|dim|aug|sus|add)?[0-9]?(/[A-G][#b]?)?\s*)+$)");
    if (std::regex_match(trimmed, chord_regex)) {
        return LINE::CHORD;
    }

    // Check if it's an INFO line (matches [something] with optional trailing spaces)
    static const std::regex info_regex(R"(^\[\s*.*\s*\]\s*$)");
    if (std::regex_match(trimmed, info_regex)) {
        return LINE::INFO;
    }

    // If it doesn't match any of the above, it's a LYRIC line
    return LINE::LYRIC;
}


std::vector<std::string> readFileLines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);

    if (!file) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return lines; // Return empty vector
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    file.close();
    return lines;
}

std::vector<std::pair<std::string, size_t>> getWordsAndPositions(const std::string& line) {
    std::vector<std::pair<std::string, size_t>> wordsWithPositions;
    std::istringstream stream(line);
    std::string word;
    size_t position = 0; // Tracks the starting position of each word in the line

    while (stream >> word) {
        size_t wordStartPos = line.find(word, position);  // Find the word's position in the line
        wordsWithPositions.push_back({word, wordStartPos});
        position = wordStartPos + word.length();  // Move the position forward to the end of the current word
    }

    return wordsWithPositions;
}

void writeToFile(const std::vector<std::string>& lines, const std::string& filename) {
    std::ofstream outputFile(filename);  // Open file for writing
    
    if (!outputFile.is_open()) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return;
    }

    for (const auto& line : lines) {
        outputFile << line << "\n";  // Write each line to the file, followed by a newline
    }

    outputFile.close();  // Close the file after writing
}

// Function to convert a string to CamelCase and remove special characters
std::string makeFilenameSafe(const std::string& input) {
    std::string result;
    bool capitalizeNext = true;  // To capitalize the first letter of each word

    for (char ch : input) {
        // Check if the character is alphanumeric
        if (std::isalnum(ch)) {
            if (capitalizeNext) {
                result += std::toupper(ch);  // Capitalize the character
                capitalizeNext = false;
            } else {
                result += std::tolower(ch);  // Convert to lowercase
            }
        } else {
            // If it's not alphanumeric, skip it (allow -)
            if (ch == '-') {
                result += ch;
            }
            capitalizeNext = true;  // Start a new word
        }
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <filename> <artist> <title>" << std::endl;
        return 1;
    }

    std::string filename = argv[1]; // Read the filename from command-line argument
    std::vector<std::string> lines = readFileLines(filename);

    std::string artist = argv[2];
    std::string songtitle = argv[3];

    // Modify text
    for (int i = 0; i < lines.size(); i++) {
        switch (getLineType(lines.at(i)))
        {
        case LINE::EMPTY: {
            //lines.erase(lines.begin() + i);
            break;
        }

        case LINE::INFO: {
            debugMsg("INFO: " + lines.at(i));
            lines.at(i) = "\\beginverse";
            auto it = lines.begin() + i;
            lines.insert(it,"\\endverse");
            break;
        }
        
        case LINE::CHORD: {
            if (i+1 < lines.size() && getLineType(lines.at(i+1)) == LINE::LYRIC) {
                debugMsg("CHORD_L: " + lines.at(i) + "\nLYRICS: " + lines.at(i+1));
                // next line is lyric line -> modify next line
                std::string lyricLine = lines.at(i+1);
                auto chordsandpos = getWordsAndPositions(lines.at(i));
                int offset = 0; // offset from adding chords
                for (auto [chord, pos] : chordsandpos) {
                    std::string chordpattern = "\\[" + chord + "]";
                    if (pos + offset < lyricLine.size()) {
                        lyricLine.insert(pos + offset, chordpattern);
                    } else {
                        lyricLine.append(" " + chordpattern);
                    }
                    offset += chordpattern.size();
                }
                // delete chord line & skip next lyric line
                lines.at(i+1) = lyricLine;
                lines.erase(lines.begin() + i);
            } else {
                debugMsg("CHORD_X: " + lines.at(i));
                // next line is non-lyric line -> modify this line
                std::istringstream stream(lines.at(i));
                std::string chord;
                std::string result;
                
                while (stream >> chord) {  // Extract words separated by whitespace
                    result += "\\[" + chord + "]";
                }

                lines.at(i) = result;
            }
            break;
        }

        case LINE::LYRIC: {
            debugMsg("LYRIC: " + lines.at(i));
            break;
        }

        default:
            break;
        }
    }

    lines.insert(lines.begin(), "\\beginverse");
    lines.insert(lines.begin(), "\\beginsong{" + songtitle + "}[by={" + artist + "}]");
    lines.push_back("\\endverse");
    lines.push_back("\\endsong");

    // Output the lines for debugging
    debugMsg("\n\nRESULT:\n");
    for (const auto& line : lines) {
        debugMsg(line);
    }

    // write to file
    std::string file_out = makeFilenameSafe(artist + "-" + songtitle);
    writeToFile(lines, "songs/" + file_out + ".tex");
    // print according input command
    std::cout << "\\input{" << "songs/" << file_out << "}" << std::endl;

    return 0;
}
