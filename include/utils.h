
namespace ques {
    pid_t pcreate(int fds[2], const char* cmd);
    std::string exec_filter(const char* cmd, std::string request, 
            std::string data);
    std::string exec(const char* cmd);
    void apply_config(std::string config_name, 
            rasty::Configuration *config,
            std::map<std::string, 
            ques::rasty_container>* rasty_map);
}
