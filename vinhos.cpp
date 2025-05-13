#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

const string DATA_FILE = "vinhos.csv";
const string INDEX_FILE = "indice.txt";
const string INPUT_FILE = "in.txt";
const string OUTPUT_FILE = "out.txt";
const string META_FILE = "meta.txt";

int ORDER = 3;
int nextNodeId = 0;

struct InsertionResult {
    bool newChildCreated = false;
    int promotedKey = -1;
    int newChildNodeId = -1;
};

struct BPlusNode {
    int id;
    bool isLeaf;
    vector<int> keys;
    vector<int> pointers;
    int nextLeaf = -1;

    string serialize() const {
        stringstream ss;
        ss << (isLeaf ? "L" : "I") << "|";
        for (size_t i = 0; i < keys.size(); i++) {
            ss << keys[i];
            if (i < keys.size() - 1) ss << ",";
        }
        ss << "|";
        for (size_t i = 0; i < pointers.size(); i++) {
            ss << pointers[i];
            if (i < pointers.size() - 1) ss << ",";
        }
        if (isLeaf) ss << "|" << nextLeaf;
        return ss.str();
    }

    static BPlusNode deserialize(const string &line, int id) {
        BPlusNode node;
        node.id = id;
        vector<string> parts;
        stringstream ss(line);
        string part;
        while (getline(ss, part, '|')) parts.push_back(part);

        node.isLeaf = (parts[0] == "L");

        stringstream kss(parts[1]);
        while (getline(kss, part, ',')) if (!part.empty()) node.keys.push_back(stoi(part));

        stringstream pss(parts[2]);
        while (getline(pss, part, ',')) if (!part.empty()) node.pointers.push_back(stoi(part));

        if (node.isLeaf && parts.size() > 3) node.nextLeaf = stoi(parts[3]);

        return node;
    }
};

string readLineFromFile(const string &filename, int lineNumber) {
    ifstream file(filename);
    string line;
    for (int i = 0; i <= lineNumber && getline(file, line); i++) {
        if (i == lineNumber) return line;
    }
    return "";
}

void writeLineToFile(const string &filename, int lineNumber, const string &newLine) {
    fstream file(filename);
    vector<string> lines;
    string line;
    while (getline(file, line)) lines.push_back(line);
    file.close();

    while ((int)lines.size() <= lineNumber) lines.push_back("");
    lines[lineNumber] = newLine;

    ofstream out(filename);
    for (const string &l : lines) out << l << "\n";
}
void writeRootId(int id) {
    ofstream meta(META_FILE);
    meta << id << endl;
}
int readRootId() {
    ifstream meta(META_FILE);
    int id = 0;
    if (meta.is_open()) {
        meta >> id;
    } else {
        writeRootId(0);
    }
    return id;
}

int findDataLineByYear(int key) {
    ifstream data(DATA_FILE);
    string line;
    int lineNum = 0;
    
    if (!data.is_open()) {
        cerr << "Erro ao abrir o arquivo: " << DATA_FILE << endl;
        return -1;
    }
    getline(data, line); 
    lineNum++;

    while (getline(data, line)) {
        stringstream ss(line);
        string campo;
        int col = 0;
        while (getline(ss, campo, ',')) {
            if (col == 2) {
                try {
                    if (stoi(campo) == key) return lineNum;
                } catch (...) {}
                break;
            }
            col++;
        }
        lineNum++;
    }
    return -1;
}


InsertionResult insertRecursive(int nodeId, int key, int dataLine) {
    BPlusNode node = BPlusNode::deserialize(readLineFromFile(INDEX_FILE, nodeId), nodeId);

    if (node.isLeaf) {
        auto it = lower_bound(node.keys.begin(), node.keys.end(), key);
        int idx = it - node.keys.begin();
        node.keys.insert(it, key);
        node.pointers.insert(node.pointers.begin() + idx, dataLine);

        if ((int)node.keys.size() <= ORDER) {
            writeLineToFile(INDEX_FILE, nodeId, node.serialize());
            return {};
        }

        BPlusNode newLeaf;
        newLeaf.id = nextNodeId++;
        newLeaf.isLeaf = true;
        int mid = node.keys.size() / 2;

        newLeaf.keys.assign(node.keys.begin() + mid, node.keys.end());
        newLeaf.pointers.assign(node.pointers.begin() + mid, node.pointers.end());
        node.keys.resize(mid);
        node.pointers.resize(mid);

        newLeaf.nextLeaf = node.nextLeaf;
        node.nextLeaf = newLeaf.id;

        writeLineToFile(INDEX_FILE, nodeId, node.serialize());
        writeLineToFile(INDEX_FILE, newLeaf.id, newLeaf.serialize());

        return {true, newLeaf.keys[0], newLeaf.id};
    } else {
        int i = 0;
        while (i < (int)node.keys.size() && key >= node.keys[i]) i++;
        InsertionResult result = insertRecursive(node.pointers[i], key, dataLine);

        if (!result.newChildCreated) return {};

        auto it = upper_bound(node.keys.begin(), node.keys.end(), result.promotedKey);
        int idx = it - node.keys.begin();
        node.keys.insert(it, result.promotedKey);
        node.pointers.insert(node.pointers.begin() + idx + 1, result.newChildNodeId);

        if ((int)node.keys.size() <= ORDER - 1) {
            writeLineToFile(INDEX_FILE, nodeId, node.serialize());
            return {};
        }

        BPlusNode newInternal;
        newInternal.id = nextNodeId++;
        newInternal.isLeaf = false;

        int mid = node.keys.size() / 2;
        int promote = node.keys[mid];

        newInternal.keys.assign(node.keys.begin() + mid + 1, node.keys.end());
        newInternal.pointers.assign(node.pointers.begin() + mid + 1, node.pointers.end());
        node.keys.resize(mid);
        node.pointers.resize(mid + 1);

        writeLineToFile(INDEX_FILE, nodeId, node.serialize());
        writeLineToFile(INDEX_FILE, newInternal.id, newInternal.serialize());

        return {true, promote, newInternal.id};
    }
}

int insert(int key) {
    int dataLine = findDataLineByYear(key);
    if (dataLine == -1) return 0;

    ifstream idxFile(INDEX_FILE);
    string temp;
    int lines = 0;
    while (getline(idxFile, temp)) lines++;
    nextNodeId = lines;

    int rootId;
    idxFile.clear();
    idxFile.seekg(0, ios::beg);
    if (!idxFile.good() || lines == 0) {
        BPlusNode root;
        rootId = nextNodeId++;
        root.id = rootId;
        root.isLeaf = true;
        root.keys.push_back(key);
        root.pointers.push_back(dataLine);
        writeLineToFile(INDEX_FILE, root.id, root.serialize());
        writeRootId(root.id);
        return 1;
    }

    rootId = readRootId();
    InsertionResult result = insertRecursive(rootId, key, dataLine);

    if (result.newChildCreated) {
        BPlusNode newRoot;
        newRoot.id = nextNodeId++;
        newRoot.isLeaf = false;
        newRoot.keys.push_back(result.promotedKey);
        newRoot.pointers.push_back(rootId);
        newRoot.pointers.push_back(result.newChildNodeId);
        writeLineToFile(INDEX_FILE, newRoot.id, newRoot.serialize());
        writeRootId(newRoot.id);
    }

    return 1;
}

int search(int key) {
    ifstream idxFile(INDEX_FILE);
    if (!idxFile.good()) return 0;

    int nodeId = readRootId();
    while (true) {
        BPlusNode node = BPlusNode::deserialize(readLineFromFile(INDEX_FILE, nodeId), nodeId);
        if (node.isLeaf) {
            for (size_t i = 0; i < node.keys.size(); i++) {
                if (node.keys[i] == key) {
                    string data = readLineFromFile(DATA_FILE, node.pointers[i]);
                    return !data.empty() ? 1 : 0;
                }
            }
            return 0;
        } else {
            int i = 0;
            while (i < (int)node.keys.size() && key >= node.keys[i]) i++;
            nodeId = node.pointers[i];
        }
    }
}

int calculateHeight(int nodeId) {
    BPlusNode node = BPlusNode::deserialize(readLineFromFile(INDEX_FILE, nodeId), nodeId);
    if (node.isLeaf) return 1;
    return 1 + calculateHeight(node.pointers[0]);
}

int main() {


    int linha = findDataLineByYear(1997);
    cout << "Linha do vinho 1997: " << linha << endl;
    ifstream input(INPUT_FILE);
    ofstream output(OUTPUT_FILE);
    string line;

    getline(input, line);
    output << line << "\n";
    if (line.substr(0, 4) == "FLH/") {
        ORDER = stoi(line.substr(4));
    }

    while (getline(input, line)) {
        if (line.substr(0, 4) == "INC:") {
            int key = stoi(line.substr(4));
            int inserted = insert(key);
            output << "INC:" << key << "/" << inserted << "\n";
        } else if (line.substr(0, 5) == "BUS=:") {
            int key = stoi(line.substr(5));
            int found = search(key);
            output << "BUS=:" << key << "/" << found << "\n";
        }
    }

    output << "H/" << calculateHeight(readRootId()) << "\n";
    return 0;
}


