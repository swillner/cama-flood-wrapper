#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "FC.h"
#include "progressbar.h"
#include "settingsnode.h"
#include "settingsnode/inner.h"
#include "settingsnode/yaml.h"
#include "version.h"

extern "C" {
void FC_init_inputnam_mod_init_inputnam();
void FC_init_map_mod_init_map();
void FC_init_topo_mod_init_topo();
void FC_init_cond_mod_init_cond();
void FC_init_time_mod_init_time();
void FC_control_tstp_mod_control_tstp();
void FC_calc_fldstg_calc_fldstg();
void FC_additional_mod_cleanup_tstp();
void FC_additional_mod_restart_init();
extern int FC_control_inp_mod_lclose;
extern int FC_additional_mod_ifirstin;
extern char FC_mod_input_crunoffcdf[256];
extern char FC_mod_input_crofcdfvar[256];
extern int FC_mod_input_irestart;
extern int FC_mod_input_isyear;   // start year
extern int FC_mod_input_ismon;    // start month
extern int FC_mod_input_isday;    // start day
extern int FC_mod_input_ieyear;   // end year
extern int FC_mod_input_iemon;    // end month
extern int FC_mod_input_ieday;    // end day
extern int FC_mod_input_syearin;  // start year in runoff file
extern int FC_mod_input_smonin;   // start month in runoff file
extern int FC_mod_input_sdayin;   // start day in runoff file
}

template<class TemplateFunction>
static std::string fill_template(const std::string& in, const TemplateFunction& f) {
    const char* beg_mark = "[[";
    const char* end_mark = "]]";
    std::ostringstream ss;
    std::size_t pos = 0;
    while (true) {
        std::size_t start = in.find(beg_mark, pos);
        std::size_t stop = in.find(end_mark, start);
        if (stop == std::string::npos) {
            break;
        }
        ss.write(&*in.begin() + pos, start - pos);
        start += std::strlen(beg_mark);
        std::string key = in.substr(start, stop - start);
        ss << f(key);
        pos = stop + std::strlen(end_mark);
    }
    ss << in.substr(pos, std::string::npos);
    return ss.str();
}

static inline void set_fortran_string(char dest[256], const std::string& src) { std::strncpy(dest, src.c_str(), 255); }

void run(const settings::SettingsNode& settings) {
    if (!settings["verbose"].as<bool>(false)) {
        std::freopen("/dev/null", "w", stdout);
    }
    FC_init_inputnam_mod_init_inputnam();

    const auto spinup_count = settings["spinup"].as<std::size_t>();
    const auto start_year = settings["years"]["from"].as<int>();
    const auto end_year = settings["years"]["to"].as<int>();
    const auto month = 1;
    const auto day = 1;

    set_fortran_string(FC_mod_input_crofcdfvar, settings["input"]["variable"].as<std::string>());
    const auto runoff_filename = settings["input"]["file"].as<std::string>();
    set_fortran_string(FC_mod_input_crunoffcdf, fill_template(runoff_filename, [&](const std::string& key) { return std::to_string(start_year); }));

    FC_mod_input_isyear = start_year;   // start year
    FC_mod_input_ismon = month;         // start month
    FC_mod_input_isday = day;           // start day
    FC_mod_input_iemon = month;         // end month
    FC_mod_input_ieday = day;           // end day
    FC_mod_input_syearin = start_year;  // start year in runoff file
    FC_mod_input_smonin = month;        // start month in runoff file
    FC_mod_input_sdayin = day;          // start day in runoff file

    FC_mod_input_ieyear = start_year + 1;  // end FC_year
    FC_control_inp_mod_lclose = 0;

    FC_mod_input_irestart = 2;  // just make sure camaflood thinks it's in spinup
                                // mode (i.e. does not read the snapshot)
    FC_init_map_mod_init_map();
    FC_init_topo_mod_init_topo();
    FC_init_cond_mod_init_cond();

    progressbar::ProgressBar bar(spinup_count + end_year - start_year + 1, "", false, stderr);
    if (spinup_count > 0) {
        FC_additional_mod_ifirstin = 0;
        FC_init_time_mod_init_time();
        FC_control_tstp_mod_control_tstp();
        ++bar;

        for (std::size_t i = 0; i < spinup_count - 1; ++i) {
            FC_additional_mod_cleanup_tstp();
            FC_additional_mod_restart_init();
            FC_additional_mod_ifirstin = 0;
            FC_init_time_mod_init_time();
            FC_control_tstp_mod_control_tstp();
            ++bar;
        }

        FC_additional_mod_cleanup_tstp();
        FC_additional_mod_restart_init();
    }

    for (int year = start_year; year <= end_year; ++year) {
        set_fortran_string(FC_mod_input_crunoffcdf, fill_template(runoff_filename, [&](const std::string& key) { return std::to_string(year); }));
        FC_mod_input_isyear = year;
        FC_mod_input_ieyear = year + 1;
        FC_mod_input_syearin = year;
        FC_additional_mod_ifirstin = 0;
        if (year > start_year) {
            FC_control_inp_mod_lclose = 1;
        }
        FC_init_time_mod_init_time();
        FC_control_tstp_mod_control_tstp();
        FC_additional_mod_cleanup_tstp();
        FC_additional_mod_restart_init();
        ++bar;
    }
    bar.close();
}

static void print_usage(const char* program_name) {
    std::cerr << "CaMa-Flood Wrapper\n"
                 "Version: " CAMAFLOOD_VERSION
                 "\n"
                 "Included CaMa-Flood Version: 3.6.2_20140909\n"
                 "\n"
                 "Wrapper Author: Sven Willner <sven.willner@pik-potsdam.de>\n"
                 "\n"
                 "Usage:   "
              << program_name
              << " (<option> | <settingsfile>)\n"
                 "Options:\n"
                 "  -h, --help     Print this help text\n"
                 "  -v, --version  Print version"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    const std::string arg = argv[1];
    if (arg.length() > 1 && arg[0] == '-') {
        if (arg == "--version" || arg == "-v") {
            std::cout << CAMAFLOOD_VERSION << std::endl;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    } else {
#ifndef DEBUG
        try {
#endif
            if (arg == "-") {
                std::cin >> std::noskipws;
                run(settings::SettingsNode(std::unique_ptr<settings::YAML>(new settings::YAML(std::cin))));
            } else {
                std::ifstream settings_file(arg);
                if (!settings_file) {
                    throw std::runtime_error("Cannot open " + arg);
                }
                run(settings::SettingsNode(std::unique_ptr<settings::YAML>(new settings::YAML(settings_file))));
            }
#ifndef DEBUG
        } catch (std::runtime_error& ex) {
            std::cerr << ex.what() << std::endl;
            return 255;
        }
#endif
    }
    return 0;
}
