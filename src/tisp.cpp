#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>
#include <vector>
using namespace std;
namespace fs = filesystem;

constexpr auto VERSION = "1.0.0";
enum class TokenType { Int, Float, Op, Ident, Def, Loop, If, Cond, LParen, RParen, LBrack, RBrack, Eof };
enum class Type { Int, Float };
struct Token { TokenType type; string val; };
struct Value { Type type; string name; bool ptr = false; };

class Compiler {
    string src, blk = "entry";
    size_t pos = 0;
    int tmp = 0, lbl = 0;
    ostringstream ir, allocs, funcDefs;
    Token cur;
    map<string, Value> vars;
    struct Func { Type retType; };
    map<string, Func> funcs;
    
    static inline map<string, pair<string,string>> ops = {
        {"+", {"add","fadd"}}, {"-", {"sub","fsub"}},
        {"*", {"mul","fmul"}}, {"/", {"sdiv","fdiv"}}
    };

    static inline map<string, pair<string,string>> cmps = {
        {"<", {"slt","lt"}}, {">", {"sgt","gt"}}, {"=", {"eq","eq"}}
    };

    void skip() { 
        while (pos < src.size()) {
            if (isspace(src[pos])) {
                ++pos;
            } else if (pos + 1 < src.size() && src[pos] == ';') {
                while (pos < src.size() && src[pos] != '\n') ++pos;
            } else {
                break;
            }
        }
    }

    void expect(TokenType t, const char* msg) {
        if (cur.type != t) err(msg);
        adv();
    }

    [[noreturn]] void err(const char* m) { cerr << "error: " << m << "\n"; exit(1); }
    string ty(Type t) { return t == Type::Float ? "double" : "i32"; }

    Token next() {
        skip();

        if (pos >= src.size()) {
            return {TokenType::Eof, ""};
        }

        char c = src[pos];

        // single-character tokens
        if (c == '(') { ++pos; return {TokenType::LParen, "("}; }
        if (c == ')') { ++pos; return {TokenType::RParen, ")"}; }
        if (c == '[') { ++pos; return {TokenType::LBrack, "["}; }
        if (c == ']') { ++pos; return {TokenType::RBrack, "]"}; }

        // operators
        if (ops.count(string(1, c)) || cmps.count(string(1, c))) {
            return {TokenType::Op, string(1, src[pos++])};
        }

        // identifiers
        if (isalpha(c) || c == '_') {
            string id;
            while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_')) {
                id += src[pos++];
            }
            if (id == "define") return {TokenType::Def, id};
            if (id == "loop") return {TokenType::Loop, id};
            if (id == "if") return {TokenType::If, id};
            if (id == "cond") return {TokenType::Cond, id};
            return {TokenType::Ident, id};
        }

        // numbers (int or float)
        bool isSignedNumber = (c == '-' || c == '+') && 
                              pos + 1 < src.size() && 
                              (isdigit(src[pos + 1]) || src[pos + 1] == '.');

        if (isdigit(c) || c == '.' || isSignedNumber) {
            string num;

            if (c == '-' || c == '+') {
                num += src[pos++];
            }

            bool hasDot = false;
            while (pos < src.size() && (isdigit(src[pos]) || src[pos] == '.')) {
                if (src[pos] == '.') hasDot = true;
                num += src[pos++];
            }

            return {hasDot ? TokenType::Float : TokenType::Int, num};
        }

        err("unexpected token");
    }

    void adv() { 
        cur = next(); 
    }

    string t() { 
        return "%" + to_string(tmp++); 
    }

    string L() { 
        return "L" + to_string(lbl++); 
    }

    Value load(Value v) {
        if (!v.ptr) {
            return v;
        }

        string reg = t();
        ir << "  " << reg << " = load " << ty(v.type) << ", " 
           << ty(v.type) << "* " << v.name << "\n";

        return {v.type, reg};
    }

    Value conv(Value v) {
        v = load(v);

        if (v.type == Type::Int) {
            string reg = t();
            ir << "  " << reg << " = sitofp i32 " << v.name << " to double\n";
            return {Type::Float, reg};
        }

        return v;
    }

    Value binop(const string& op, Value left, Value right) {
        left = load(left);
        right = load(right);

        bool isFloat = (left.type == Type::Float || right.type == Type::Float);

        if (isFloat) {
            left = conv(left);
            right = conv(right);
        }

        string res = t();
        string opcode = isFloat ? ops[op].second : ops[op].first;
        Type resultType = isFloat ? Type::Float : Type::Int;

        ir << "  " << res << " = " << opcode << " " 
           << ty(resultType) << " " << left.name << ", " << right.name << "\n";

        return {resultType, res};
    }

    Value cmpop(const string& op, Value left, Value right) {
        left = load(left); right = load(right);
        bool isFl = (left.type == Type::Float || right.type == Type::Float);
        if (isFl) { left = conv(left); right = conv(right); }
        string res = t();
        ir << "  " << res << " = " << (isFl ? "fcmp o" : "icmp ")
           << (isFl ? cmps[op].second : cmps[op].first) << " "
           << ty(isFl ? Type::Float : Type::Int) << " " << left.name << ", " << right.name << "\n";
        return {Type::Int, res};
    }

    Value parse() {
        if (cur.type == TokenType::LParen) {
            adv();

            // (define name value) or (define (name args...) body)
            if (cur.type == TokenType::Def) {
                adv();

                // (define (name args...) body)
                if (cur.type == TokenType::LParen) {
                    adv();
                    string fname = cur.val; adv();
                    vector<string> args;
                    while (cur.type == TokenType::Ident) { args.push_back(cur.val); adv(); }
                    expect(TokenType::RParen, "expected )");

                    auto savedVars = vars; auto savedIr = ir.str(); auto savedAllocs = allocs.str();
                    int savedTmp = tmp; string savedBlk = blk;
                    vars.clear(); ir.str(""); allocs.str(""); tmp = 0; blk = "entry";

                    funcs[fname] = {Type::Int};  // pre-register for recursion
                    for (auto& a : args) vars[a] = {Type::Int, "%" + a, false};
                    Value result = load(parse());
                    funcs[fname].retType = result.type;

                    funcDefs << "define " << ty(result.type) << " @" << fname << "(";
                    for (size_t i = 0; i < args.size(); ++i) funcDefs << (i ? ", " : "") << "i32 %" << args[i];
                    funcDefs << ") {\nentry:\n" << allocs.str() << ir.str();
                    funcDefs << "  ret " << ty(result.type) << " " << result.name << "\n}\n\n";

                    vars = savedVars; ir.str(savedIr); ir.seekp(0, ios::end);
                    allocs.str(savedAllocs); allocs.seekp(0, ios::end); tmp = savedTmp; blk = savedBlk;

                    expect(TokenType::RParen, "expected )");
                    return {Type::Int, ""};
                }

                // (define name value)
                string name = cur.val;
                expect(TokenType::Ident, "expected identifier");

                Value val = load(parse());

                if (!vars.count(name)) {
                    string ptr = t();
                    allocs << "  " << ptr << " = alloca " << ty(val.type) << "\n";
                    vars[name] = {val.type, ptr, true};
                }

                ir << "  store " << ty(val.type) << " " << val.name << ", "
                   << ty(val.type) << "* " << vars[name].name << "\n";

                expect(TokenType::RParen, "expected )");
                return {Type::Int, ""};
            }

            // (loop count body...)
            if (cur.type == TokenType::Loop) {
                adv();

                Value count = load(parse());

                string preBlock = blk;
                string headerBlock = L();
                string bodyBlock = L();
                string exitBlock = L();
                int phiId = tmp++;

                ir << "  br label %" << headerBlock << "\n";
                ir << headerBlock << ":\n";

                // reserve space for phi node (will be patched later)
                size_t phiPos = ir.str().size();
                ir << string(99, ' ') << "\n";

                string cond = t();
                ir << "  " << cond << " = icmp slt i32 %" << phiId << ", " << count.name << "\n";
                ir << "  br i1 " << cond << ", label %" << bodyBlock << ", label %" << exitBlock << "\n";

                ir << bodyBlock << ":\n";
                blk = bodyBlock;

                while (cur.type != TokenType::RParen) {
                    parse();
                }

                string backBlock = blk;  // may differ from bodyBlock if nested loops
                string next = t();
                ir << "  " << next << " = add i32 %" << phiId << ", 1\n";
                ir << "  br label %" << headerBlock << "\n";

                ir << exitBlock << ":\n";
                blk = exitBlock;

                // patch in the phi node
                string code = ir.str();
                string phi = "  %" + to_string(phiId) + " = phi i32 [0, %" + preBlock + "], [" + next + ", %" + backBlock + "]";
                phi.resize(99, ' ');
                code.replace(phiPos, 99, phi);
                ir.str(code);
                ir.seekp(0, ios::end);

                adv();
                return {Type::Int, ""};
            }

            // (if exp true false)
            if (cur.type == TokenType::If) {
                adv();
                Value c = load(parse());
                string thenL = L(), elseL = L(), endL = L();
                ir << "  br i1 " << c.name << ", label %" << thenL << ", label %" << elseL << "\n";
                ir << thenL << ":\n"; blk = thenL;
                Value th = load(parse()); string thB = blk;
                ir << "  br label %" << endL << "\n";
                ir << elseL << ":\n"; blk = elseL;
                Value el = load(parse()); string elB = blk;
                ir << "  br label %" << endL << "\n";
                ir << endL << ":\n"; blk = endL;
                string res = t();
                ir << "  " << res << " = phi " << ty(th.type) << " [" << th.name << ", %" << thB << "], [" << el.name << ", %" << elB << "]\n";
                expect(TokenType::RParen, "expected )");
                return {th.type, res};
            }

            // (cond [exp true] [exp true] ...)
            if (cur.type == TokenType::Cond) {
                adv();
                string endL = L();
                vector<pair<Value, string>> rs;
                while (cur.type == TokenType::LBrack) {
                    adv();
                    Value c = load(parse());
                    string thenL = L(), nextL = L();
                    ir << "  br i1 " << c.name << ", label %" << thenL << ", label %" << nextL << "\n";
                    ir << thenL << ":\n"; blk = thenL;
                    Value r = load(parse()); rs.push_back({r, blk});
                    ir << "  br label %" << endL << "\n";
                    ir << nextL << ":\n"; blk = nextL;
                    expect(TokenType::RBrack, "expected ]");
                }
                rs.push_back({{Type::Int, "0"}, blk});
                ir << "  br label %" << endL << "\n";
                ir << endL << ":\n"; blk = endL;
                string res = t();
                ir << "  " << res << " = phi " << ty(rs[0].first.type);
                for (size_t i = 0; i < rs.size(); ++i)
                    ir << (i ? ", " : " ") << "[" << rs[i].first.name << ", %" << rs[i].second << "]";
                ir << "\n";
                expect(TokenType::RParen, "expected )");
                return {rs[0].first.type, res};
            }

            // (funcname args...)
            if (cur.type == TokenType::Ident && funcs.count(cur.val)) {
                string fname = cur.val; adv();
                vector<Value> args;
                while (cur.type != TokenType::RParen) args.push_back(load(parse()));
                adv();
                Func& fn = funcs[fname];
                string res = t();
                ir << "  " << res << " = call " << ty(fn.retType) << " @" << fname << "(";
                for (size_t i = 0; i < args.size(); ++i) ir << (i ? ", " : "") << ty(args[i].type) << " " << args[i].name;
                ir << ")\n";
                return {fn.retType, res};
            }

            // (op args...)
            string op = cur.val;
            expect(TokenType::Op, "expected operator");

            Value acc = parse();
            if (cmps.count(op)) {
                acc = cmpop(op, acc, parse());
            } else {
                while (cur.type != TokenType::RParen) {
                    acc = binop(op, acc, parse());
                }
            }
            adv();

            return acc;
        }

        // variable reference
        if (cur.type == TokenType::Ident) {
            string name = cur.val;
            adv();

            if (!vars.count(name)) err("undefined variable");

            return vars[name];
        }

        // numeric literals
        if (cur.type == TokenType::Int || cur.type == TokenType::Float) {
            Type type = (cur.type == TokenType::Float) ? Type::Float : Type::Int;
            Value val = {type, cur.val};
            adv();
            return val;
        }

        err("unexpected token");
    }

public:
    string compile(const string& source) {
        src = source; pos = tmp = lbl = 0; blk = "entry";
        ir.str(""); allocs.str(""); funcDefs.str(""); vars.clear(); funcs.clear();
        ostringstream hdr;

        hdr << "; generated by tisp " << VERSION << "\n"
            << "declare i32 @printf(i8*, ...)\n\n"
            << "@.str.int = private constant [4 x i8] c\"%d\\0A\\00\"\n"
            << "@.str.float = private constant [4 x i8] c\"%f\\0A\\00\"\n\n";
        
        for (adv(); cur.type != TokenType::Eof;) {
            Value res = load(parse());
            if (!res.name.empty())
                ir << "  " << t() << " = call i32 (i8*, ...) @printf(i8* getelementptr ([4 x i8], "
                   << "[4 x i8]* @.str." << (res.type == Type::Int ? "int" : "float") << ", i32 0, i32 0), "
                   << ty(res.type) << " " << res.name << ")\n";
        }

        ir << "  ret i32 0\n}\n";
        return hdr.str() + funcDefs.str() + "define i32 @main() {\nentry:\n" + allocs.str() + ir.str();
    }
};

void help(const char* p) {
    cout << "Usage: " << p << " <input.tsp> [options]\n\n"
         << "Options:\n"
         << "  -o <output>   Specify output executable name\n"
         << "  --emit-ir     Emit LLVM IR only (.ll)\n"
         << "  --emit-asm    Emit assembly only (.s)\n"
         << "  --emit-obj    Emit object file only (.o)\n"
         << "  --verbose     Preserve all intermediates\n"
         << "  --help        Show this help message\n"
         << "  --version     Show version information\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { help(argv[0]); return 1; }
    string input, output;
    int emit = 0; // 0=exe, 1=ir, 2=asm, 3=obj
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "--help" || a == "-h") { help(argv[0]); return 0; }
        if (a == "--version" || a == "-v") { cout << "tisp " << VERSION << " - Tiny Lisp\n"; return 0; }
        if (a == "-o" && i + 1 < argc) { output = argv[++i]; continue; }
        if (a == "--emit-ir") { emit = 1; continue; }
        if (a == "--emit-asm") { emit = 2; continue; }
        if (a == "--emit-obj") { emit = 3; continue; }
        if (a == "--verbose") { verbose = true; continue; }
        if (a[0] != '-') { input = a; continue; }
        cerr << "error: unknown option: " << a << "\n"; return 1;
    }
    if (input.empty()) { cerr << "error: no input file\n"; return 1; }

    ifstream ifs(input);
    if (!ifs) { cerr << "error: cannot open " << input << "\n"; return 1; }
    string src((istreambuf_iterator<char>(ifs)), {});
    ifs.close();

    string llvm = Compiler().compile(src);

    string base = fs::path(input).stem().string();
    if (output.empty()) output = base;
    string ll = base + ".ll", as = base + ".s", ob = base + ".o";

    ofstream(ll) << llvm;
    if (emit == 1) return 0;

    string llc = "llc -O2 " + ll + (emit == 3 ? " --filetype=obj -o " + ob : " -o " + as);
    if (system(llc.c_str())) { cerr << "error: llc failed\n"; return 1; }

    if (emit == 2 || emit == 3) { if (!verbose) fs::remove(ll); return 0; }
    
#ifdef _WIN32
    string exe = output + ".exe", link = "gcc " + as + " -o " + exe, alt = "clang " + as + " -o " + exe;
#else
    string exe = output, link = "clang " + as + " -o " + exe, alt = "gcc " + as + " -o " + exe;
#endif
    if (system(link.c_str()) && (verbose && system(alt.c_str())))
        { cerr << "error: linking failed\n"; return 1; }

    if (!verbose) fs::remove(ll), fs::remove(as);
    return 0;
}