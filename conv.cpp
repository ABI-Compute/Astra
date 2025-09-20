#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <vector>

using namespace std;

// Trim whitespace
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// Very basic mapping: HTML tag -> Tcl command
string htmlTagToCommand(const string& tag) {
    if (tag == "h1") return ".header";
    if (tag == "p") return ".para";
    if (tag == "img") return ".img";
    if (tag == "title") return ".title";
    if (tag == "style") return ".styles";
    return "." + tag; // fallback
}

bool startsWith(const string& str, const string& prefix) {
    return str.compare(0, prefix.length(), prefix) == 0;
}

int main(int argc, char* argv[]) {
    int counter = 1;
    vector<string> vars;
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <file.html>" << endl;
        return 1;
    }

    ifstream file(argv[1]);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << argv[1] << endl;
        return 1;
    }

    string outputFilename = argv[1];
    size_t dotPos = outputFilename.find_last_of('.');
    if (dotPos != string::npos) {
        outputFilename = outputFilename.substr(0, dotPos);
    }
    outputFilename += ".ab";

    ofstream outFile(outputFilename);
    if (!outFile.is_open()) {
        cerr << "Failed to create output file: " << outputFilename << endl;
        return 1;
    }
    outFile << "genFrom " << argv[1] << endl;

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();

    // --- Step 1: Parse <style> blocks ---
    map<string, tuple<string,int,string>> styles; 
    regex styleBlockRegex("([#\\w\\-]+)\\s*\\{([\\s\\S]*?)\\}");
    smatch sbMatch;
    string::const_iterator styleStart(content.cbegin());

    while (regex_search(styleStart, content.cend(), sbMatch, styleBlockRegex)) {
        string targetID = sbMatch[1];
        string block = sbMatch[2];

        string colour = "#000000";
        int fontSize = 24;
        string ttf = "Arial.ttf";

        regex propRegex("(color|colour|font-size|font-family)\\s*:\\s*([^;]+);?");
        smatch propMatch;
        string::const_iterator propStart(block.cbegin());
        while (regex_search(propStart, block.cend(), propMatch, propRegex)) {
            string prop = propMatch[1];
            string val = propMatch[2];
            if (prop == "color" || prop == "colour") colour = val;
            else if (prop == "font-size") fontSize = stoi(val);
            else if (prop == "font-family") ttf = val + ".ttf";
            propStart = propMatch.suffix().first;
        }

        styles[targetID] = make_tuple(colour, fontSize, ttf);
        styleStart = sbMatch.suffix().first;
    }

    regex tagRegex("<(/?)(\\w+)([^>]*)>");
    smatch match;
    string::const_iterator searchStart(content.cbegin());

    outFile << ".Doc start" << endl;

    if (!styles.empty()) {
        outFile << ".styles start" << endl;
        for (auto& [id, tpl] : styles) {
            string colour;
            int fontSize;
            string ttf;
            tie(colour, fontSize, ttf) = tpl;

            outFile << ".style start" << endl;
            outFile << "    targetID: " << id.substr(0, id.find('#')) << endl;
            outFile << "    colour " << colour << endl;
            outFile << "    fontSize " << fontSize << endl;
            outFile << "    ttf " << ttf << endl;
            outFile << "  .style end" << endl;
        }
        outFile << ".styles end" << endl;
    }

    while (regex_search(searchStart, content.cend(), match, tagRegex)) {
        string closing = match[1];
        string tag = match[2];
        string attrs = match[3];

        string textBefore = content.substr(searchStart - content.cbegin(), match.position());
        textBefore = trim(textBefore);

        if (!textBefore.empty() &&
            textBefore.find("DOCTYPE") == std::string::npos &&
            tag != "style" &&
            tag != "script") 
        {
            std::cout << "Text: " << textBefore << std::endl;
            outFile << ".txt " << textBefore << std::endl;
        }

        if (closing.empty()) {
            outFile << htmlTagToCommand(tag) << " start" << endl;

            string elementID;
            regex idRegex("id=[\"']([^\"']+)[\"']", regex_constants::icase);
            smatch idMatch;
            if (regex_search(attrs, idMatch, idRegex)) {
                elementID = idMatch[1];
            } else {
                elementID = tag + to_string(counter++);
            }
            outFile << "ID: " << elementID << endl;

            if (tag == "img") {
                regex srcRegex("src=[\"']([^\"']+)[\"']");
                smatch srcMatch;
                if (regex_search(attrs, srcMatch, srcRegex)) {
                    outFile << ".media " << srcMatch[1] << endl;
                }
                regex altRegex("alt=[\"']([^\"']+)[\"']");
                smatch altMatch;
                if (regex_search(attrs, altMatch, altRegex)) {
                    outFile << ".desc " << altMatch[1] << endl;
                }
                outFile << ".img end" << endl;
            } else if (tag == "script") {
                string scriptContent;
                regex srcRegex("src=[\"']([^\"']+)[\"']");
                smatch srcMatch;
                if (regex_search(attrs, srcMatch, srcRegex)) {
                    outFile << ".script src " << srcMatch[1] << endl;
                } else {
                    string::const_iterator scriptStart = match.suffix().first;
                    string::const_iterator scriptEnd = content.cend();
                    smatch endMatch;
                    if (regex_search(scriptStart, content.cend(), endMatch, regex("</script>", regex_constants::icase))) {
                        scriptEnd = endMatch.prefix().second;
                    }
                    scriptContent = string(scriptStart, scriptEnd);
                    scriptContent = trim(scriptContent);
                    vector<string> lines;
                    stringstream ss(scriptContent);
                    string line;

                    while (getline(ss, line)) {
                        string trimmed = trim(line);

                        regex consoleRegex(R"(console\.log\s*\((.*)\))");
                        smatch cm;
                        if (regex_search(trimmed, cm, consoleRegex)) {
                            string logContentR = trim(cm[1]);
                            string logContent;
                            bool inQuotes = false;
                            string var;

                            for (char c : logContentR) {
                                if (c == '"' || c == '\'') {
                                    inQuotes = !inQuotes;
                                    continue;
                                }
                                if (c == ')') break;

                                if (inQuotes) {
                                    logContent += c;
                                } else {
                                    if (isalnum(c) || c == '_') {
                                        var += c;
                                    } else {
                                        if (!var.empty()) {
                                            if (find(vars.begin(), vars.end(), var) != vars.end())
                                                logContent += "\\$" + var;
                                            else
                                                cout << "Warning: Undefined variable " << var << " in console.log" << endl;
                                                logContent += var;
                                            var.clear();
                                        } else if (find(vars.begin(), vars.end(), string(1, c)) != vars.end()) {
                                            logContent += "\\$" + string(1, c);
                                        } else {
                                            cout  << "Warning: Undefined variable " << c << " in console.log" << endl;  
                                            logContent += c;
                                        }
                                    }
                                }
                            }
                            if (!var.empty()) {
                                if (find(vars.begin(), vars.end(), var) != vars.end())
                                    logContent += "\\$" + var;
                                
                                else
                                    if (find(vars.begin(), vars.end(), var) != vars.end())
                                        logContent += "\\$" + var;
                                    else

                                        cout << "Warning: Undefined variable " << var << " in console.log" << endl;
                                        for (string var : vars) {
                                            cout << "Defined variable: " << var << endl;
                                        }
                                        //logContent += var;
                            }

                            cout << "Found console.log: " << logContent << endl;
                            outFile << "log " << logContent << endl;
                        }

                        else if (startsWith(trimmed, "alert")) {
                            string logContentR = trimmed.substr(6);
                            logContentR = trim(logContentR);
                            string logContent;
                            for (char c : logContentR) {
                                if (c == '"' || c == '\'') continue;
                                if (c == ')') break;
                                logContent += c;
                            }
                            cout << "Found Alert: " << logContent << endl;
                            outFile << "alert " << logContent << endl;
                        }

                        else if (startsWith(trimmed, "let")) {
                            string Content = trimmed.substr(3);
                            string Name;
                            string Value;
                            bool foundEqual = false;
                            for (char c : Content) {
                                if (c == '=') foundEqual = true;
                                else if (!foundEqual) Name += c;
                                else if (foundEqual && c != ';') Value += c;
                            }
                            Name = trim(Name);
                            Value = trim(Value);
                            if (startsWith(Value, "prompt(")) {
                                string promptContent;
                                for (char c : Value.substr(7)) {
                                    if (c == '"' || c == '\'') continue;
                                    if (c == ')') break;
                                    promptContent += c;
                                }
                                cout << "Found prompt: " << Name << " with message: " << promptContent << endl;
                                vars.push_back(Name);
                                outFile << "prompt " << Name  << " " << promptContent << endl; 
                            } else {
                                cout << "Found variable: " << Name << " with value: " << Value << endl;
                                vars.push_back(Name);
                                outFile << "let " << Name << " " << Value << endl;
                            }
                        }

                        else if (trimmed.find("function") != string::npos || trimmed.find("}") != string::npos || trimmed.find("{") != string::npos || trimmed.find("//") != string::npos || trimmed.find("if") != string::npos || trimmed.find("else") != string::npos) {
                            // Ignore
                        }

                        else if (!trimmed.empty()) {
                            cout << "Ignoring script line: " << trimmed << endl;
                        }
                    }
                }
                outFile << ".script end" << endl;
            }
        } else {
            outFile << htmlTagToCommand(tag) << " end" << endl;
        }

        searchStart = match.suffix().first;
    }

    string remaining = content.substr(searchStart - content.cbegin());
    remaining = trim(remaining);
    if (!remaining.empty() && remaining.find("DOCTYPE") == string::npos) {
        outFile << ".txt " << remaining << endl;
    }

    outFile << ".Doc end" << endl;

    cout << "Output saved to " << outputFilename << endl;
    return 0;
}
