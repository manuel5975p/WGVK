#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <iomanip>

using nlohmann::json;

struct type_info {
    std::string name;
};

void emitStructDefinition(
    const std::string& key,
    const json& root,
    const std::unordered_map<std::string, type_info>& types,
    const std::unordered_map<std::string, std::vector<std::string>>& dependencies,
    std::unordered_map<std::string, bool>& emitted,
    const std::unordered_set<std::string>& excluded_tags
);

std::string toPascalCase(const std::string& input) {
    std::stringstream ss(input);
    std::string word, result;
    while (ss >> word) {
        if (!word.empty()) {
            word[0] = std::toupper(static_cast<unsigned char>(word[0]));
            result += word;
        }
    }
    return result;
}

std::string toCamelCase(const std::string& input) {
    std::string pascal = toPascalCase(input);
    if (!pascal.empty()) {
        pascal[0] = std::tolower(static_cast<unsigned char>(pascal[0]));
    }
    return pascal;
}

std::string getCTypeName(const std::string& type_name, const std::unordered_map<std::string, type_info>& types) {
    if (type_name == "uint64_t") return "uint64_t";
    if (type_name == "uint32_t") return "uint32_t";
    if (type_name == "uint16_t") return "uint16_t";
    if (type_name == "uint8_t") return "uint8_t";
    if (type_name == "size_t") return "size_t";
    if (type_name == "int32_t") return "int32_t";
    if (type_name == "int") return "int";
    if (type_name == "bool") return "WGPUBool";
    if (type_name == "float") return "float";
    if (type_name == "double") return "double";
    if (type_name == "char") return "char";
    if (type_name == "void") return "void";
    if (type_name == "void *") return "void*";
    if (type_name == "void const *") return "const void*";

    auto it = types.find(type_name);
    if (it != types.end()) {
        return it->second.name;
    }

    return "WGPU" + toPascalCase(type_name);
}

std::string formatFullType(const json& j, const std::string& type_key, const std::unordered_map<std::string, type_info>& types) {
    std::string type_str = getCTypeName(j.value(type_key, ""), types);
    std::string prefix;
    std::string suffix;

    if (j.contains("annotation")) {
        std::string annotation = j["annotation"];
        if (annotation == "const*") {
            prefix = "const ";
            suffix = "*";
        } else if (annotation == "*") {
            suffix = "*";
        } else if (annotation == "const*const*") {
            prefix = "const ";
            suffix = "* const*";
        }
    }

    if (type_str == "const char*") {
        prefix = "";
    }

    return prefix + type_str + suffix;
}

// Block prefix added to tagged enum values, mirroring Dawn's dawn_json_generator.py:
// dawn -> 0x00050000, else emscripten -> 0x00040000, else native -> 0x00010000.
// Untagged (core) and wgvk-tagged values carry their absolute value already.
uint32_t enumTagPrefix(const json& node) {
    if (!node.contains("tags") || !node["tags"].is_array()) return 0;
    bool dawn = false, emscripten = false, native = false;
    for (const auto& tag : node["tags"]) {
        if (!tag.is_string()) continue;
        const std::string s = tag.get<std::string>();
        dawn |= (s == "dawn");
        emscripten |= (s == "emscripten");
        native |= (s == "native");
    }
    if (dawn) return 0x00050000;
    if (emscripten) return 0x00040000;
    if (native) return 0x00010000;
    return 0;
}

bool hasExcludedTag(const json& node, const std::unordered_set<std::string>& excluded_tags) {
    if (node.is_null() || !node.contains("tags")) {
        return false;
    }
    const auto& tags = node["tags"];
    if (!tags.is_array()) {
        return false;
    }
    for (const auto& tag : tags) {
        if (tag.is_string() && excluded_tags.count(tag.get<std::string>())) {
            return true;
        }
    }
    return false;
}

void emitStructDefinition(
    const std::string& key,
    const json& root,
    const std::unordered_map<std::string, type_info>& types,
    const std::unordered_map<std::string, std::vector<std::string>>& dependencies,
    std::unordered_map<std::string, bool>& emitted,
    const std::unordered_set<std::string>& excluded_tags
) {
    if (emitted.at(key)) {
        return;
    }

    emitted[key] = true;

    if (dependencies.count(key)) {
        for (const auto& dep_key : dependencies.at(key)) {
            emitStructDefinition(dep_key, root, types, dependencies, emitted, excluded_tags);
        }
    }

    const auto& value = root.at(key);
    if (hasExcludedTag(value, excluded_tags)) return;

    std::string struct_name = types.at(key).name;
    std::cout << "typedef struct " << struct_name << " {\n";

    if (value.contains("extensible")) {
        const auto& prop = value.at("extensible");
        if (prop.is_string()) {
            std::string extensible_type = prop.get<std::string>();
            if (extensible_type == "in" || extensible_type == "out") {
                std::cout << "    WGPUChainedStruct const * nextInChain;\n";
            }
        }
    } else if (value.contains("chained")) {
        const auto& prop = value.at("chained");
        if (prop.is_string()) {
            std::cout << "    WGPUChainedStruct chain;\n";
        }
    }

    if (value.contains("members")) {
        for (const auto& member : value["members"]) {
             if(hasExcludedTag(member, excluded_tags)) continue;
             std::cout << "    " << formatFullType(member, "type", types) << " " << toCamelCase(member["name"]) << ";\n";
        }
    }
    std::cout << "} " << struct_name << ";\n\n";
}

int main(int argc, char* argv[]) {
    std::unordered_set<std::string> excluded_tags = {};

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-e" || arg == "--excluded-tags") && i + 1 < argc) {
            std::string tags_str = argv[++i];
            std::stringstream ss(tags_str);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                excluded_tags.insert(tag);
                std::cerr << "Excluding tag " << tag << "\n";
            }
        }
    }
    

    std::ifstream dawn_json("dawn.json");
    if (!dawn_json.is_open()) {
        std::cerr << "Error: Could not open dawn.json" << std::endl;
        return 1;
    }
    json root;
    try {
        root = json::parse(dawn_json);
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return 1;
    }

    std::unordered_map<std::string, type_info> types;
    std::vector<std::string> generation_order;

    for (const auto& [key, value] : root.items()) {
        if (value.is_object() && value.contains("category")) {
             std::string category = value["category"];
             if (category == "structure" || category == "object" || category == "enum" || category == "bitmask" || category == "function pointer" || category == "callback function" || category == "callback info") {
                if (key.rfind("_", 0) != 0) {
                    types.emplace(key, type_info{"WGPU" + toPascalCase(key)});
                    generation_order.push_back(key);
                }
             }
        }
    }

    std::cout << "#ifndef WEBGPU_H_\n";
    std::cout << "#define WEBGPU_H_\n\n";
    std::cout << "#if defined(WGPU_SHARED_LIBRARY)\n";
    std::cout << "#  if defined(_WIN32)\n";
    std::cout << "#    if defined(WGPU_IMPLEMENTATION)\n";
    std::cout << "#      define WGPU_EXPORT __declspec(dllexport)\n";
    std::cout << "#    else\n";
    std::cout << "#      define WGPU_EXPORT __declspec(dllimport)\n";
    std::cout << "#    endif\n";
    std::cout << "#  else\n";
    std::cout << "#    if defined(WGPU_IMPLEMENTATION)\n";
    std::cout << "#      define WGPU_EXPORT __attribute__((visibility(\"default\")))\n";
    std::cout << "#    else\n";
    std::cout << "#      define WGPU_EXPORT\n";
    std::cout << "#    endif\n";
    std::cout << "#  endif\n";
    std::cout << "#else\n";
    std::cout << "#  define WGPU_EXPORT\n";
    std::cout << "#endif\n\n";
    std::cout << "#include <stdint.h>\n";
    std::cout << "#include <stddef.h>\n\n";
    std::cout << "typedef uint32_t WGPUBool;\n";
    std::cout << "typedef uint64_t WGPUFlags;\n\n";

    for(const auto& key : generation_order) {
        const auto& value = root[key];
        if (hasExcludedTag(value, excluded_tags)) continue;
        if (value["category"] == "object") {
             std::cout << "typedef struct " << types[key].name << "Impl* " << types[key].name << ";\n";
        }
    }
    std::cout << "\n";

    for(const auto& key : generation_order) {
        const auto& value = root[key];
        if (hasExcludedTag(value, excluded_tags)) continue;
        std::string category = value.value("category", "");
        if (category == "structure" || category == "callback info") {
             std::cout << "typedef struct " << types[key].name << " " << types[key].name << ";\n";
        }
    }
    std::cout << "\n";

    for(const auto& key : generation_order) {
        const auto& value = root.value(key, json::object());
        if (hasExcludedTag(value, excluded_tags)) continue;
        std::string category = value.value("category", "");

        if (category == "enum") {
            std::string enum_name = types[key].name;
            std::cout << "typedef enum " << enum_name << " {\n";
            if (value.contains("values")) {
                for(const auto& enum_value : value["values"]) {
                    if (hasExcludedTag(enum_value, excluded_tags)) continue;
                    std::string member_name_str = enum_value["name"];
                    std::replace(member_name_str.begin(), member_name_str.end(), '-', ' ');
                    std::string member_name = toPascalCase(member_name_str);
                    uint32_t val = enum_value["value"].get<uint32_t>() + enumTagPrefix(enum_value);
                    std::cout << "    " << enum_name << "_" << member_name << " = 0x" << std::hex << std::setw(8) << std::setfill('0') << val << std::dec << ",\n";
                }
            }
            std::cout << "    " << enum_name << "_Force32 = 0x7FFFFFFF\n";
            std::cout << "} " << enum_name << ";\n\n";
        } else if (category == "bitmask") {
            std::string enum_name = types[key].name;
            std::cout << "typedef WGPUFlags " << enum_name << ";\n";
            if (value.contains("values")) {
                for(const auto& enum_value : value["values"]) {
                    if (hasExcludedTag(enum_value, excluded_tags)) continue;
                    std::string member_name_str = enum_value["name"];
                    std::replace(member_name_str.begin(), member_name_str.end(), '-', ' ');
                    std::string member_name = toPascalCase(member_name_str);
                    // Bitmask values are bit patterns; Dawn applies no tag prefix here.
                    uint64_t val = enum_value["value"].get<uint64_t>();
                    std::cout << "static const " << enum_name << " " << enum_name << "_" << member_name << " = 0x" << std::hex << std::setw(16) << std::setfill('0') << val << std::dec << ";\n";
                }
            }
            std::cout << "\n";
        }
    }

    if (types.count("s type")) {
        std::cout << "typedef WGPUSType WGPUSType;\n";
    }
    std::cout << "typedef struct WGPUChainedStruct {\n";
    std::cout << "    const struct WGPUChainedStruct * next;\n";
    std::cout << "    WGPUSType sType;\n";
    std::cout << "} WGPUChainedStruct;\n\n";

    for (const auto& key : generation_order) {
        const auto& value = root.value(key, json::object());
        if (hasExcludedTag(value, excluded_tags)) continue;
        std::string category = value.value("category", "");

        if (category == "function pointer" || category == "callback function") {
            std::string return_type = value.contains("returns") ? getCTypeName(value["returns"], types) : "void";

            std::stringstream args_ss;
            if (value.contains("args") && !value["args"].empty()) {
                 bool first = true;
                 for (const auto& arg : value["args"]) {
                    if (!first) args_ss << ", ";
                    args_ss << formatFullType(arg, "type", types) << " " << toCamelCase(arg["name"]);
                    first = false;
                 }
            } else {
                args_ss << "void";
            }

            std::cout << "typedef " << return_type << " (*" << types[key].name << ")(" << args_ss.str() << ");\n";
        }
    }

    std::unordered_map<std::string, std::vector<std::string>> dependencies;
    for (const auto& key : generation_order) {
        const auto& value = root.at(key);
        std::string category = value.value("category", "");
        if ((category == "structure" || category == "callback info") && value.contains("members")) {
            for (const auto& member : value["members"]) {
                if (member.contains("type")) {
                    std::string member_type_key = member["type"];
                    if (types.count(member_type_key)) {
                        std::string member_category = root.at(member_type_key).value("category", "");
                        if (member_category == "structure" || member_category == "callback info") {
                           dependencies[key].push_back(member_type_key);
                        }
                    }
                }
            }
        }
    }

    std::unordered_map<std::string, bool> emitted;
    for (const auto& key : generation_order) {
        emitted[key] = false;
    }

    for (const auto& key : generation_order) {
        const auto& value = root.at(key);
        std::string category = value.value("category", "");
        if (category == "structure" || category == "callback info") {
            emitStructDefinition(key, root, types, dependencies, emitted, excluded_tags);
        }
    }

    std::cout << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";

    for (const auto& [key, value] : root.items()) {
         if (!value.is_object() || !value.contains("category") || key.rfind("_", 0) == 0) continue;
         if (hasExcludedTag(value, excluded_tags)) continue;

         if (value["category"] == "function") {
            std::string return_type = value.contains("returns") ? getCTypeName(value["returns"], types) : "void";
            std::stringstream args_ss;
            if (value.contains("args")) {
                bool first = true;
                for (const auto& arg : value["args"]) {
                    if(hasExcludedTag(arg, excluded_tags)) continue;
                    if (!first) args_ss << ", ";
                    args_ss << formatFullType(arg, "type", types) << " " << toCamelCase(arg["name"]);
                    first = false;
                }
            }

            std::cout << "WGPU_EXPORT " << return_type << " wgpu" << toPascalCase(key) << "(" << args_ss.str() << ");\n";
         }
         else if (value["category"] == "object") {
            if(!value.contains("methods")){
                value.emplace("methods", json::array());
            }
            json releaseMethod;
            json addRefMethod;
            releaseMethod["name"] = "release";
            addRefMethod["name"] = "add ref";
            value["methods"].push_back(std::move(releaseMethod));
            value["methods"].push_back(std::move(addRefMethod));

            if(value.contains("methods")){
            for (const auto& method : value["methods"]) {
                if (hasExcludedTag(method, excluded_tags)) continue;
                std::string return_type_str = "void";
                if(method.contains("returns")){
                     const auto& ret_val = method["returns"];
                     if (ret_val.is_string()){
                        return_type_str = getCTypeName(ret_val.get<std::string>(), types);
                     } else if (ret_val.is_object()){
                        return_type_str = getCTypeName(ret_val.value("type", "void"), types);
                     }
                }

                std::stringstream args_ss;
                args_ss << types[key].name << " " << toCamelCase(key);

                if (method.contains("args")) {
                   for(const auto& arg : method["args"]) {
                       if(hasExcludedTag(arg, excluded_tags)) continue;
                       args_ss << ", " << formatFullType(arg, "type", types) << " " << toCamelCase(arg["name"]);
                   }
                }
                std::cout << "WGPU_EXPORT " << return_type_str << " wgpu" << toPascalCase(key) << toPascalCase(method["name"]) << "(" << args_ss.str() << ");\n";
            }
          }
         }
    }

    std::cout << "#ifdef __cplusplus\n} // extern \"C\"\n#endif\n\n";
    std::cout << "#endif // WEBGPU_H_\n";

    return 0;
}