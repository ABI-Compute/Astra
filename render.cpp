#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype> 
#include <map>
#include <functional>

#include <SDL.h>
#include <SDL_main.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

using namespace std;


// --- Globals ---
SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;
int gWindowWidth = 800;
int gWindowHeight = 600;
string SrcName;
string infile;
vector<int> executed_idxs;

// --- Simple State Machine ---
class State {
private:
    vector<string> stack;
public:
    void push(const string& tag) { stack.push_back(tag); }
    void pop() { if (!stack.empty()) stack.pop_back(); }
    string current() const { return stack.empty() ? "" : stack.back(); }
};

// --- Style strct ---
struct Style {
    SDL_Color colour; // SDL still needs SDL_Color, but field name is "colour"
    int fontSize;
    string ttf_path;
};

// Dev Tools structs and functions
// Context menu items
struct MenuItem {
    string label;
    function<void()> action;
};

struct ContextMenu {
    vector<MenuItem> items;
    int x, y;
    int width, height;
    bool visible = false;
};



// --- SDL Setup ---
bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cerr << "SDL Init Error: " << SDL_GetError() << endl;
        return false;
    }
    if (TTF_Init() == -1) {
        cerr << "TTF Init Error: " << TTF_GetError() << endl;
        return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        cerr << "IMG Init Error: " << IMG_GetError() << endl;
        return false;
    }

    gWindow = SDL_CreateWindow("Astra Render",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!gWindow) {
        cerr << "Window Error: " << SDL_GetError() << endl;
        return false;
    }

    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
    if (!gRenderer) {
        cerr << "Renderer Error: " << SDL_GetError() << endl;
        return false;
    }

    return true;
}

void cleanupSDL() {
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

// --- Rendering helpers ---

void renderText(const string& text, int x, int y, const Style& style, int wrapWidth, int& outHeight) {
    TTF_Font* font = TTF_OpenFont(style.ttf_path.c_str(), style.fontSize);
    if (!font) {
        cerr << "Font Error: " << TTF_GetError() << endl;
        outHeight = style.fontSize;
        return;
    }

    SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text.c_str(), style.colour, wrapWidth);
    if (!surface) {
        cerr << "Text Surface Error: " << TTF_GetError() << endl;
        TTF_CloseFont(font);
        outHeight = style.fontSize;
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(gRenderer, surface);
    SDL_Rect dst = { x, y, surface->w, surface->h };
    SDL_RenderCopy(gRenderer, texture, nullptr, &dst);

    outHeight = surface->h;  // actual rendered height for cursor increment

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    TTF_CloseFont(font);
}


void renderImage(const string& path, int x, int y) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        cerr << "Image Load Error (" << path << "): " << IMG_GetError() << endl;
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(gRenderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_RenderCopy(gRenderer, texture, nullptr, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

vector<string> used_alert_messages;

void js_alert(const std::string& message, const std::string& documentTitle) {
    // Use document title or fallback
    // check if message was already shown
    if (find(used_alert_messages.begin(), used_alert_messages.end(), message) != used_alert_messages.end())
        return; // already shown
    used_alert_messages.push_back(message);
    std::string title = documentTitle.empty() ? "Untitled Page" : documentTitle;

    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_INFORMATION,
        title.c_str(),        // popup title
        message.c_str(),      // content from alert("...")
        nullptr               // no parent window
    );
}


// --- Parser + Renderer ---
bool startsWith(const string& str, const string& prefix) {
    return str.compare(0, prefix.length(), prefix) == 0;
}

unordered_map<string, Style> parseStyles(const vector<string>& lines) {
    unordered_map<string, Style> styles;

    auto trim = [](const string &s) -> string {
        auto start = find_if_not(s.begin(), s.end(), ::isspace);
        auto end = find_if_not(s.rbegin(), s.rend(), ::isspace).base();
        return (start < end ? string(start, end) : "");
    };

    bool inStyles = false;
    bool inStyleBlock = false;
    string currentTarget;
    Style currentStyle;

    for (auto line : lines) {
        line = trim(line);

        if (line == ".styles start") {
            inStyles = true;
        }
        else if (line == ".styles end") {
            inStyles = false;
        }
        else if (inStyles && line == ".style start") {
            inStyleBlock = true;
            currentTarget.clear();
            currentStyle = {{0,0,0,255}, 24, "Arial.ttf"};
        }
        else if (inStyles && line == ".style end") {
            if (!currentTarget.empty()) {
                styles[currentTarget] = currentStyle;
            }
            inStyleBlock = false;
        }
        else if (inStyleBlock && line.rfind("targetID: ", 0) == 0) {
            currentTarget = line.substr(10);
        }
        else if (inStyleBlock && line.rfind("colour ", 0) == 0) {
            string hex = line.substr(7);
            int r = stoi(hex.substr(1, 2), nullptr, 16);
            int g = stoi(hex.substr(3, 2), nullptr, 16);
            int b = stoi(hex.substr(5, 2), nullptr, 16);
            currentStyle.colour = {Uint8(r), Uint8(g), Uint8(b), 255};
        }
        else if (inStyleBlock && line.rfind("fontSize ", 0) == 0) {
            currentStyle.fontSize = stoi(line.substr(9));
        }
        else if (inStyleBlock && line.rfind("ttf ", 0) == 0) {
            currentStyle.ttf_path = line.substr(4);
        }
    }

    return styles;
}

// Helper to trim whitespace
string trim(const string& str) {
    auto start = find_if_not(str.begin(), str.end(), ::isspace);
    auto end = find_if_not(str.rbegin(), str.rend(), ::isspace).base();
    return (start < end ? string(start, end) : "");
}

// Helper to split a string into a vector of strings
vector<string> split(const string& str, char delimiter) {
    vector<string> result;
    stringstream ss(str);
    string token;
    while (getline(ss, token, delimiter)) {
        result.push_back(token);
    }
    return result;
}

string promptConsole(const string& message);
void logToConsole(const string& msg);

void exec(const vector<string>& lines, const unordered_map<string, Style>& styles, int windowWidth, bool first_time) {
    State state;
    int baseX = 50;
    int cursorX = baseX;
    int cursorY = 50;
    int lineSpacing = 5;
    vector<int> indentStack;
    string pendingImgSrc, pendingImgDesc;
    string currentID;
    map<string, string> variables;
    int pc = 0;

    for (const auto& line : lines) {
        pc += 1;
        // --- State handling ---
        if (line == ".Doc start") state.push("Doc");
        else if (line == ".Doc end") state.pop();
        else if (line == ".html start") state.push("html");
        else if (line == ".html end") state.pop();
        else if (line == ".body start") state.push("body");
        else if (line == ".body end") state.pop();

        else if (startsWith(line, "genFrom ")) SrcName = line.substr(8);

        else if (line == ".title start") state.push("title");
        else if (line == ".title end") state.pop();

        else if (line == ".header start") state.push("header");
        else if (line == ".header end") state.pop();

        else if (line == ".para start") state.push("para");
        else if (line == ".para end") { state.pop(); cursorY += lineSpacing; }

        else if (line == ".h2 start") state.push("h2");
        else if (line == ".h2 end") { state.pop(); cursorY += lineSpacing; }

        else if (line == ".ul start") { state.push("ul"); indentStack.push_back(20); }
        else if (line == ".ul end") { state.pop(); indentStack.pop_back(); }

        else if (line == ".li start") state.push("li");
        else if (line == ".li end") { state.pop(); cursorY += lineSpacing; }

        else if (line == ".script start") state.push("script");
        else if (line == ".script end") { state.pop(); cursorY += lineSpacing; }

        else if (line == ".strong start") state.push("strong");
        else if (line == ".strong end") state.pop();

        else if (line.rfind("ID: ", 0) == 0) currentID = line.substr(4);

        // --- Text rendering ---
        else if (line.rfind(".txt ", 0) == 0) {
            string text = line.substr(5);
            Style style = {{0,0,0,255}, 24, "Arial.ttf"};
            if (!currentID.empty() && styles.count(currentID)) style = styles.at(currentID);

            string curState = state.current();
            if (curState == "header") style.fontSize += 8;
            else if (curState == "h2") style.fontSize += 6;

            if (state.current() == "title") {
                SDL_SetWindowTitle(gWindow, text.c_str());
            } else {
                int x = baseX + (indentStack.empty() ? 0 : indentStack.back());
                if (state.current() == "li") text = "- " + text;

                int textHeight = 0;
                renderText(text, x, cursorY, style, windowWidth - x - baseX, textHeight);
                cursorY += textHeight + lineSpacing;  // increment by actual rendered height
            }
        }


        // --- Image handling ---
        else if (line == ".img start") { state.push("img"); pendingImgSrc.clear(); pendingImgDesc.clear(); }
        else if (line == ".img end") {
            if (!pendingImgSrc.empty()) { renderImage(pendingImgSrc, cursorX, cursorY); cursorY += 200; }
            if (!pendingImgDesc.empty()) {
                Style style = {{0,0,0,255}, 18, "Arial.ttf"};
                if (!currentID.empty() && styles.count(currentID)) style = styles.at(currentID);
                int textHeight = 0;
                renderText(pendingImgDesc, cursorX, cursorY, style, windowWidth - cursorX - baseX, textHeight);
                cursorY += textHeight + lineSpacing;  // increment by actual rendered height
                cursorY += style.fontSize + lineSpacing;
            }
            state.pop();
        } else if (startsWith(line, "log")) {
            string logContentRAW = line.substr(4);
            string logContent;
            for (string str : split(logContentRAW, ' ')) {
                if (startsWith(str, "\\$")) {
                    string var = str.substr(2);
                    if (variables.count(var)) logContent += variables[var];
                } else {
                    logContent += str + " ";
                }
            }
            if (first_time) {
                logToConsole("[" + SrcName + "]: " + logContent + "\n");
            }
        }
        else if (startsWith(line, "alert")) {
            string alertContent = line.substr(6);
            if (first_time) {
                js_alert(alertContent, SDL_GetWindowTitle(gWindow));
            }
        }
        else if (startsWith(line, "let ")) {
            string rest = line.substr(4);
            size_t eqPos = rest.find('=');
            if (eqPos != string::npos) {
                string varName = trim(rest.substr(0, eqPos));
                string varValue = trim(rest.substr(eqPos + 1));
                variables[varName] = varValue;
                cout << "Variable set: " << varName << " = " << varValue << endl;
            }
        }
        else if (startsWith(line, "prompt ") && first_time) {
            string rest = line.substr(7);
            cout << "Prompt command: " << rest << endl;
            size_t spacePos = rest.find(' ');
            if (spacePos != string::npos) {
                string varName = trim(rest.substr(0, spacePos));
                string promptMsg = trim(rest.substr(spacePos + 1));
                
                string val = promptConsole(promptMsg);
                variables[varName] = val;
                cout << "Prompted for: " << varName << " with message: " << promptMsg << endl;
            }
        }
        else if (line.rfind("src ", 0) == 0) pendingImgSrc = line.substr(4);

        
        else if (line.rfind(".desc ", 0) == 0) pendingImgDesc = line.substr(6);
    }
}

struct Console {
    vector<string> lines;      // stored log lines
    string inputBuffer;        // for prompt() input
    bool active = false;       // whether console is visible
    int width, height;
    float opacity;             // 0.0 - 1.0
};

ContextMenu contextMenu;
Console devConsole;

void showContextMenu(int mouseX, int mouseY) {
    int menuWidth = 150;
    int menuHeight = 50; // 2 items, 25px each

    contextMenu.x = mouseX;
    contextMenu.y = mouseY;
    contextMenu.width = menuWidth;
    contextMenu.height = menuHeight;

    if (mouseX + menuWidth > gWindowWidth) {
        contextMenu.x = mouseX - menuWidth; // align top-right
    }

    contextMenu.visible = true;
    contextMenu.items = {
        {"Dev Tools", [](){ devConsole.active = true; }},
        {"Refresh", [](){
            vector<string> lines;
            ifstream file(infile);
            if (!file.is_open()) {
                cerr << "Failed to open file: " << infile << endl;
                return;
            }
            string line;
            while (getline(file, line)) lines.push_back(line);
            auto styles = parseStyles(lines);
            SDL_RenderClear(gRenderer);
            exec(lines, styles, gWindowWidth, true); // force re-exec
        }}
    };
}

void renderContextMenu() {
    if (!contextMenu.visible) return;

    SDL_Rect rect = {contextMenu.x, contextMenu.y, contextMenu.width, contextMenu.height};
    SDL_SetRenderDrawColor(gRenderer, 50, 50, 50, 255);
    SDL_RenderFillRect(gRenderer, &rect);

    int itemHeight = 25;
    for (int i = 0; i < contextMenu.items.size(); ++i) {
        renderText(contextMenu.items[i].label,
                   contextMenu.x + 5,
                   contextMenu.y + i * itemHeight,
                   { {255,255,255,255}, 18, "Arial.ttf" },
                   contextMenu.width - 10, itemHeight);
    }
}

void handleContextMenuClick(int mouseX, int mouseY) {
    if (!contextMenu.visible) return;

    if (mouseX >= contextMenu.x && mouseX <= contextMenu.x + contextMenu.width &&
        mouseY >= contextMenu.y && mouseY <= contextMenu.y + contextMenu.height) {
        
        int idx = (mouseY - contextMenu.y) / 25;
        if (idx >= 0 && idx < contextMenu.items.size()) {
            contextMenu.items[idx].action();
        }
    }

    contextMenu.visible = false; // hide after click
}

void logToConsole(const string& msg) {
    devConsole.lines.push_back(msg);
    if (devConsole.lines.size() > 100) devConsole.lines.erase(devConsole.lines.begin());

}

void renderDevConsole();

string promptConsole(const string& message) {
    logToConsole(message);
    devConsole.inputBuffer.clear();
    devConsole.active = true;

    // Wait for user input via keyboard events in main SDL loop
    while (devConsole.inputBuffer.empty()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_TEXTINPUT) {
                devConsole.inputBuffer += e.text.text;
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) {
                return devConsole.inputBuffer;
            }
        }
        SDL_Delay(16);
    }

    return devConsole.inputBuffer;
}


void renderDevConsole() {
    if (!devConsole.active) return;

    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, Uint8(0.8f * 255));

    devConsole.width = 300;
    devConsole.height = gWindowHeight;

    SDL_Rect panel = {gWindowWidth - devConsole.width, 0, devConsole.width, devConsole.height};
    SDL_RenderFillRect(gRenderer, &panel);

    // render console lines
    int y = 5;
    for (auto &line : devConsole.lines) {
        renderText(line, gWindowWidth - devConsole.width + 5, y,
                   { {255,255,255,255}, 16, "Arial.ttf" }, devConsole.width - 10, y);
        y += 18;
    }

    // render input buffer (for prompt)
    renderText("> " + devConsole.inputBuffer,
               gWindowWidth - devConsole.width + 5, devConsole.height - 25,
               { {200,200,200,255}, 16, "Arial.ttf" }, devConsole.width - 10, y);
}


// --- Main ---
int main(int argc, char* argv[]) {
    cout << "Astra Render - SDL2 Renderer\n";

    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <file.ab>\n";
        return 1;
    }
    infile = argv[1];

    if (!initSDL()) return 1;

    vector<string> lines;
    ifstream file(argv[1]);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << argv[1] << endl;
        cleanupSDL();
        return 1;
    }

    string line;
    while (getline(file, line)) lines.push_back(line);

    // --- Pass 1: Parse styles
    auto styles = parseStyles(lines);
    cout << "Parsed " << styles.size() << " styles.\n";
    for (const auto& [id, style] : styles) {
        cout << "Style for " << id << ": colour("
             << int(style.colour.r) << "," << int(style.colour.g) << "," << int(style.colour.b) << "), "
             << "fontSize(" << style.fontSize << "), ttf(" << style.ttf_path << ")\n";
    }

    // --- Background from body
    if (styles.count("body1")) {
        SDL_Color bg = styles["body1"].colour;
       SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
    } else {
        SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
    }
    SDL_RenderClear(gRenderer);

    // --- Pass 2: Render content
    bool running = true;
    SDL_Event e;
    bool first = true;

    // Enable text input for Dev Tools
    SDL_StartTextInput();

    while (running) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                        gWindowWidth = e.window.data1;
                        gWindowHeight = e.window.data2;
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_RIGHT) {
                        showContextMenu(e.button.x, e.button.y);
                    } else if (e.button.button == SDL_BUTTON_LEFT) {
                        handleContextMenuClick(e.button.x, e.button.y);
                    }
                    break;

                case SDL_TEXTINPUT:
                    if (devConsole.active) {
                        devConsole.inputBuffer += e.text.text;
                    }
                    break;

                case SDL_KEYDOWN:
                    if (devConsole.active) {
                        if (e.key.keysym.sym == SDLK_BACKSPACE && !devConsole.inputBuffer.empty()) {
                            devConsole.inputBuffer.pop_back();
                        } else if (e.key.keysym.sym == SDLK_RETURN) {
                            // Add input to console and clear buffer
                            logToConsole("> " + devConsole.inputBuffer);
                            // Here you could store or process prompt responses
                            devConsole.inputBuffer.clear();
                        }
                    }
                    break;
            }
        }

        // --- Clear and render everything ---
        SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
        SDL_RenderClear(gRenderer);

        exec(lines, styles, gWindowWidth, first);   // Your state machine rendering
        renderContextMenu();                 // Draw right-click menu if visible
        renderDevConsole();                  // Draw Dev Tools overlay if active
        first = false;

        SDL_RenderPresent(gRenderer);

        SDL_Delay(16);
    }

    // Stop text input and cleanup SDL
    SDL_StopTextInput();
    cleanupSDL();

    return 0;
}
