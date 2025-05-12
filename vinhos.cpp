#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

const string DATA_FILE = "C:/Users/felipe.albuquerque/Downloads/vinhos.csv/vinhos.csv";
const string INDEX_FILE = "indice.txt";
const string INPUT_FILE = "in.txt";
const string OUTPUT_FILE = "out.txt";

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


int findDataLineByYear(int key) {
    ifstream data(DATA_FILE);
    string line;
    int lineNum = 0;
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
        // Inserir em folha de forma ordenada
        auto it = lower_bound(node.keys.begin(), node.keys.end(), key);
        int idx = it - node.keys.begin();
        node.keys.insert(it, key);
        node.pointers.insert(node.pointers.begin() + idx, dataLine);

        // Se há espaço, salva e retorna
        if ((int)node.keys.size() < ORDER) {
            writeLineToFile(INDEX_FILE, nodeId, node.serialize());
            return {};
        }

        // Split da folha
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
        // Nó interno: encontrar filho apropriado
        int i = 0;
        while (i < (int)node.keys.size() && key >= node.keys[i]) i++;
        InsertionResult result = insertRecursive(node.pointers[i], key, dataLine);

        if (!result.newChildCreated) return {};

        // Inserir chave promovida
        auto it = upper_bound(node.keys.begin(), node.keys.end(), result.promotedKey);
        int idx = it - node.keys.begin();
        node.keys.insert(it, result.promotedKey);
        node.pointers.insert(node.pointers.begin() + idx + 1, result.newChildNodeId);

        if ((int)node.keys.size() < ORDER) {
            writeLineToFile(INDEX_FILE, nodeId, node.serialize());
            return {};
        }

        // Split de nó interno
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
    if (!idxFile.good()) {
        BPlusNode root;
        root.id = nextNodeId++;
        root.isLeaf = true;
        root.keys.push_back(key);
        root.pointers.push_back(dataLine);
        writeLineToFile(INDEX_FILE, root.id, root.serialize());
        return 1;
    }

    InsertionResult result = insertRecursive(0, key, dataLine);

    if (result.newChildCreated) {
        // Criar nova raiz
        BPlusNode newRoot;
        newRoot.id = nextNodeId++;
        newRoot.isLeaf = false;
        newRoot.keys.push_back(result.promotedKey);
        newRoot.pointers.push_back(0); // antigo root
        newRoot.pointers.push_back(result.newChildNodeId);
        writeLineToFile(INDEX_FILE, newRoot.id, newRoot.serialize());
    }

    return 1;
}


int search(int key) {
    ifstream idxFile(INDEX_FILE);
    if (!idxFile.good()) return 0;

    string line;
    int id = 0;
    while (getline(idxFile, line)) {
        BPlusNode node = BPlusNode::deserialize(line, id);
        if (node.isLeaf) {
            for (size_t i = 0; i < node.keys.size(); i++) {
                if (node.keys[i] == key) {
                    string data = readLineFromFile(DATA_FILE, node.pointers[i]);
                    return !data.empty() ? 1 : 0;
                }
            }
        }
        id++;
    }
    return 0;
}
void printNode(const BPlusNode &node, const vector<BPlusNode> &allNodes, int level = 0) {
    string indent(level * 4, ' '); // Indentação com 4 espaços por nível

    cout << indent << (node.isLeaf ? "[Leaf]" : "[Internal]") << " Node ID: " << node.id << "\n";
    cout << indent << "  Keys: ";
    for (int k : node.keys) cout << k << " ";
    cout << "\n";

    if (node.isLeaf) {
        cout << indent << "  Pointers (data lines): ";
        for (int p : node.pointers) cout << p << " ";
        cout << "\n";
        if (node.nextLeaf != -1)
            cout << indent << "  Next Leaf -> " << node.nextLeaf << "\n";
    } else {
        cout << indent << "  Pointers (child node IDs): ";
        for (int p : node.pointers) cout << p << " ";
        cout << "\n";
        for (int p : node.pointers) {
            if (p >= 0 && p < (int)allNodes.size())
                printNode(allNodes[p], allNodes, level + 1);
        }
    }
}

void printTree() {
    ifstream idxFile(INDEX_FILE);
    if (!idxFile.good()) {
        cout << "Árvore vazia.\n";
        return;
    }

    vector<BPlusNode> nodes;
    string line;
    int id = 0;
    while (getline(idxFile, line)) {
        if (!line.empty())
            nodes.push_back(BPlusNode::deserialize(line, id));
        id++;
    }

    if (!nodes.empty()) {
        cout << "\n=== Estrutura da Árvore B+ ===\n";
        printNode(nodes[0], nodes);
    } else {
        cout << "Nenhum nó encontrado.\n";
    }
}


int main() {
    ifstream input(INPUT_FILE);
    ofstream output(OUTPUT_FILE);
    string line;

    getline(input, line);
    output << line << "\n";

    if (line.rfind("FLH/", 0) != 0) {
        cerr << "Erro: primeira linha do in.txt deve começar com FLH/\n";
        return 1;
    }

    try {
        ORDER = stoi(line.substr(line.find("/") + 1));
    } catch (...) {
        cerr << "Erro: valor de filhos inválido na linha: " << line << "\n";
        return 1;
    }

while (getline(input, line)) {
    if (line.substr(0, 4) == "INC:") {
        int key = stoi(line.substr(4));
        int inserted = insert(key);  
        output << "INC:" << key << "/" << inserted << "\n";
    } else if (line.substr(0, 5) == "BUS=:") {
        int key = stoi(line.substr(5));
        int count = search(key);
        output << "BUS=:" << key << "/" << count << "\n";
    }
}


    output << "H/1\n"; 
    printTree();

    return 0;
}

