#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <map>
#include <assert.h>

using std::string;
using std::vector;
using std::pair;
using std::map;
using std::ofstream;
using std::ifstream;
using std::getline;


// Represents a single configuration containing list of attribute/value pairs
class Config {
public:
    vector<pair<string, string>> data;

    void add(const string& key, const string& value) {
        data.push_back({key, value});
    }


    static string list_to_string(const vector<string>& list_values) {
        string s = "[";
        for (size_t i = 0; i < list_values.size(); ++i) {
            s += "\"" + escape(list_values[i]) + "\"";
            if (i + 1 < list_values.size()) s += ", ";
        }
        s += "]";
        return s;
    }


    static vector<string> string_to_list(const string &arr_str) {
        vector<string> result;
        bool in_quotes = false, escaped = false;
        string current;
        for (char c : arr_str) {
            if (escaped) {
                if (c == 'n') current += '\n';
                else if (c == 't') current += '\t';
                else current += c;
                escaped = false;
            } else if (c == '\\') escaped = true;
            else if (c == '"') {
                in_quotes = !in_quotes;
                if (!in_quotes) { result.push_back(current); current.clear(); }
            } else if (in_quotes) current += c;
        }
        return result;
    }


    // count how many times key appears in configuration
    int count(const string &key) {
        int count = 0;
        for (const auto &pair : data) {
            if (pair.first == key) {
                count++;
            }
        }
        return count;
    }


    int remove_key(const string &key) {
        size_t readi = 0;
        size_t writei = 0;
        while (readi < data.size()) {
            if (data[readi].first != key) {
                if (writei < readi) {
                    data[writei] = std::move(data[readi]);  // shift back
                }
                writei++;
            }
            readi++;
        }
        data.erase(data.begin() + writei, data.end());
        return (int) (readi - writei);  // how many removed, may be 0
    }


    // set the nth value (default first) with matching key to value
    void set_value(const string &key, const string &value, int index = 0) {
        int count = 0;
        for (auto &pair : data) {
            if (pair.first == key) {
                if (count == index) {
                    pair.second = value;
                    return;
                }
                count++;
            }
        }
        add(key, value);  // no current match to key found, so add new key/val
    }


    vector<string> get_values(const string& key) const {
        vector<string> results;
        for (const auto& pair : data) {
            if (pair.first == key) results.push_back(pair.second);
        }
        return results;
    }


    // get the first value matching key as a string
    const string get_string_value(const string& key) const {
        for (const auto& pair : data) {
            if (pair.first == key) {
                return pair.second;
            }
        }
        return "";
    }

private:
    static string escape(const string& s) {
        string res;
        for (char c : s) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\n') res += "\\n";
            else if (c == '\t') res += "\\t";
            else res += c;
        }
        return res;
    }
};



// Top-level Manager to hold named configurations
class ConfigManager {
private:
    map<string, Config> configs; // Keeps config names unique

    string escape(const string& s) const {
        string res;
        for (char c : s) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\n') res += "\\n";
            else if (c == '\t') res += "\\t";
            else res += c;
        }
        return res;
    }

public:
    // Test if configuration exists:
    bool has_config(const string& name) const {
        return configs.find(name) != configs.end();
    }

    // Fetch or create a specific named configuration
    Config& config(const string& name) {
        if (!has_config(name)) {  // create an empty default configuration
            configs[name].set_value("name", name);
            // that's really all we need in a configuration
        }
        return configs[name];
    }

    // Test if there are any configs:
    bool is_empty() const {
        return configs.empty();
    }

    bool remove(const string &name) {
        return configs.erase(name) != 0;
    }

    // Get any config from a non-empty set of configurations
    Config& get_first_config() {
        assert(!configs.empty());
        // find any config other than the "pseudo configuration"
        // used to name the default/initial one:
        for (auto& config: configs) {
            if (config.first != "__configuration__") {
                return config.second;
            }
        }
    }


    vector<string> get_configuration_names() {
        vector<string> names;
        for (const auto& config : configs) {
            if (config.first != "__configuration__") {
                names.push_back(config.first);
            }
        }
        return names;
    }
    
    
    string new_config(string new_name) {
        new_name = unique_name(new_name);
        // set_value does overwrite, [] will construct new Config
        configs[new_name].set_value("name", new_name);
        return new_name;
    }


    string save_copy_as(Config *current, string new_name) {
        new_name = new_config(new_name);  // constructs empty new config
        for (size_t i = 0; i < current->data.size(); ++i) {
            string key = current->data[i].first;
            if (key != "name") {
                configs[new_name].add(key, current->data[i].second);
            }
        }
        return new_name;  // may be changed to a unique name
    }


    void save(const string& filepath) const {
        ofstream file(filepath);
        if (!file.is_open()) return;

        file << "{\n";
        size_t config_index = 0;
        for (const auto& config : configs) {
            const string &config_name = config.first;
            const Config &config_obj = config.second;
            file << "  \"" << escape(config_name) << "\": [\n";
            
            for (size_t i = 0; i < config_obj.data.size(); ++i) {
                file << "    {\"" << escape(config_obj.data[i].first) << "\": ";
                const string &val = config_obj.data[i].second;
                if (val.size() > 1 && val[0] == '[') {
                    file << val;
                } else {
                    file << "\"" << escape(val) << "\"";
                }
                file << "}";
                if (i + 1 < config_obj.data.size()) file << ",";
                file << "\n";
            }
            
            file << "  ]";
            if (++config_index < configs.size()) file << ",";
            file << "\n";
        }
        file << "}\n";
    }

    
    // append number to req(uested)_name to get a unique config name
    string unique_name(string req_name) {
        int suffix = 2;
        string name = req_name;
        while (has_config(name)) {
            name = req_name + std::to_string(suffix++);
        }
        return name;
    }


    void load(const string& filepath) {
        configs.clear();

        ifstream file(filepath);
        if (!file.is_open()) {
            // make an empty configuration
            config("__configuration__").add("__configuration__", "default");
            config("default");
            return;
        }
        string line;
        string name = "";
        bool top_level = true;

        while (getline(file, line)) {
            // 1. Detect a new top-level unique config key (look for ": [")
            // Assumes one line with: "configname": [
            if (top_level && line.find('[') != string::npos &&
                line.find(':') != string::npos) {
                size_t start = line.find('"');
                size_t end = line.find('"', start + 1);
                if (start != string::npos && end != string::npos) {
                    name = line.substr(start + 1, end - start - 1);
                    // find a unique name and make sure the config knows
                    // it's own name:
                    name = unique_name(name);
                    configs[name].set_value("name", name);
                    top_level = false;  // we passed "[" for config array
                }
                continue;
            }

            if (name.empty()) continue;

            // 2. Parse inner properties out
            size_t key_start = line.find('"');
            if (key_start == string::npos) {
                if (!top_level) {
                    key_start = line.find(']');
                    if (key_start != string::npos) {
                        top_level = true;  // found "]" after config array
                    }
                }
                continue;
            }
            size_t key_end = line.find('"', key_start + 1);
            if (key_end == string::npos) continue;  // this should never happen

            string key = line.substr(key_start + 1,
                                     key_end - key_start - 1);
            if (key == "name") continue;  // we established the name above
            size_t colon_pos = line.find(':', key_end);
            if (colon_pos == string::npos) continue;

            size_t val_start = line.find_first_not_of(" \t", colon_pos + 1);
            size_t val_end = line.find_last_of("}");
            string unparsed;
            if (val_end != string::npos && val_end > val_start) {
                string val_part = line.substr(val_start, val_end - val_start);
                if (val_part.front() == '[' && val_part.back() == ']') {
                    configs[name].add(key, val_part);
                } else if (val_part.front() == '"' && val_part.back() == '"') {
                    string string_token = val_part.substr(1,
                                      val_part.length() - 2);
                    string unescaped;
                    bool esc = false;
                    for (char ch : string_token) {
                        if (esc) {
                            if (ch == 'n') unescaped += '\n';
                            else if (ch == 't') unescaped += '\t';
                            else unescaped += ch;
                            esc = false;
                        } else if (ch == '\\') esc = true;
                        else unescaped += ch;
                    }
                    configs[name].add(key, unescaped);
                }
                unparsed = line.substr(val_end);
            } else {
                unparsed = line;
            }
            if (unparsed.find(']') != string::npos) {
                top_level = true;  // found closing "]" after config array
            }
        }
    }
};
/*

using std::cout;

int main() {
    ConfigManager manager;

    // Populating Development Config
    manager.config("DevelopmentConfig").add("BannerText",
                             "Debug Mode Active\nLine Two");
    manager.config("DevelopmentConfig").add("AllowedIPs", 
            vector<string>{"127.0.0.1", "192.168.1.50"});

    // Populating Production Config
    manager.config("ProductionConfig").add("BannerText",
                             "Live Environment\nLine Two");
    manager.config("ProductionConfig").add("AllowedIPs",
                      vector<string>{"10.0.0.1"});
    manager.config("ProductionConfig").add("AllowedIPs",
                    vector<string>{"172.16.0.1"}); // duplicate key

    manager.save("configs.json");

    // Testing Read-back
    ConfigManager reader;
    reader.load("configs.json");

    cout << "--- Verified Production IPs ---\n";
    auto prod_ips = reader.config("ProductionConfig").get_values("AllowedIPs");
    for (const auto& entry : prod_ips) {
        for (const auto& ip : entry.as_list()) {
            cout << " Allowed Production Target: " << ip << "\n";
        }
    }

    return 0;
}
*/
