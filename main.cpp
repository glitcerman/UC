// main.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include "codegen.hpp"

// Lit un fichier entier en string
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    // Vérifie qu'un fichier a été passé en argument
    if (argc < 2) {
        std::cerr << "Usage: uc <file.uc>\n";
        return 1;
    }

    std::string sourcePath = argv[1];

    // Chemins des fichiers intermédiaires et finaux
    std::string basePath = sourcePath.substr(0, sourcePath.find_last_of('.'));
    std::string llPath  = basePath + ".ll";
    std::string objPath = basePath + ".o";
    std::string binPath = basePath;

    try {
        // 1. Lecture du fichier source
        std::string source = readFile(sourcePath);
        std::cout << "[ UC ] Compiling " << sourcePath << "...\n";

        // 2. Lexer
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        std::cout << "[ UC ] Lexing done — " << tokens.size() << " tokens\n";

        // 3. Parser
        Parser parser(tokens);
        NodePtr ast = parser.parse();
        std::cout << "[ UC ] Parsing done\n";

        // 4. Analyse sémantique
        SemanticAnalyzer analyzer;
        analyzer.analyze(ast.get());
        std::cout << "[ UC ] Semantic analysis done\n";

        // 5. Génération du code LLVM IR
        CodeGen codegen;
        std::string ir = codegen.generate(ast.get());
        std::cout << "[ UC ] Code generation done\n";

        // 6. Sauvegarde du fichier .ll
        std::ofstream outFile(llPath);
        outFile << ir;
        outFile.close();
        std::cout << "[ UC ] IR written to " << llPath << "\n";

        // 7. Compilation en fichier objet avec llc
        std::cout << "[ UC ] Compiling to object file...\n";
        int llcResult = system(("llc -filetype=obj " + llPath + " -o " + objPath).c_str());
        if (llcResult != 0)
            throw std::runtime_error("llc failed — check your LLVM installation");

        // 8. Liaison en binaire exécutable avec gcc
        std::cout << "[ UC ] Linking...\n";
        int gccResult = system(("gcc " + objPath + " -o " + binPath).c_str());
        if (gccResult != 0)
            throw std::runtime_error("gcc failed — check your GCC installation");

        // 9. Nettoyage des fichiers intermédiaires
        system(("rm " + llPath + " " + objPath).c_str());

        std::cout << "[ UC ] Done ! Binary: ./" << binPath << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n" << e.what() << "\n";
        return 1;
    }

    return 0;
}