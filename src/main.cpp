#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include <ncDim.h>
#include <ncFile.h>
#include <ncGroupAtt.h>
#include <ncType.h>
#include <ncVar.h>
#include <netcdf>

#pragma GCC diagnostic pop

#include "FC.h"
#include "progressbar.h"
#include "settingsnode.h"
#include "settingsnode/inner.h"
#include "settingsnode/yaml.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

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
extern int FC_additional_mod_ifirstin;
extern char FC_mod_input_crunoffcdf[256];
extern int FC_mod_input_irestart;
extern int FC_mod_input_isyear;  // start year
extern int FC_mod_input_ismon;   // start month
extern int FC_mod_input_isday;   // start day
extern int FC_mod_input_ieyear;  // end year
extern int FC_mod_input_iemon;   // end month
extern int FC_mod_input_ieday;   // end day
extern int FC_mod_input_syearin; // start year in runoff file
extern int FC_mod_input_smonin;  // start month in runoff file
extern int FC_mod_input_sdayin;  // start day in runoff file
}

void run(const settings::SettingsNode &settings) {
  if (!settings["verbose"].as<bool>(false)) {
    std::freopen("/dev/null", "w", stdout);
  }
  const auto spinup_count = settings["spinup_years"].as<std::size_t>();

  FC_init_inputnam_mod_init_inputnam();

  const auto runoff_filename = settings["input"]["file"].as<std::string>();
  std::strncpy(FC_mod_input_crunoffcdf, runoff_filename.c_str(), 255);

  netCDF::NcFile runoff_file;
  try {
    runoff_file.open(runoff_filename, netCDF::NcFile::read);
  } catch (netCDF::exceptions::NcException &e) {
    throw std::runtime_error(runoff_filename + ": " + e.what());
  }

  const auto runoff_varname = settings["input"]["variable"].as<std::string>();
  const auto runoff_variable = runoff_file.getVar(runoff_varname);
  if (runoff_variable.isNull()) {
    throw std::runtime_error(runoff_filename + ": Variable '" + runoff_varname +
                             "' not found");
  }
  const auto time_variable = runoff_file.getVar("time");
  if (time_variable.isNull()) {
    throw std::runtime_error(runoff_filename + ": Variable 'time' not found");
  }

  const auto days_count = time_variable.getDim(0).getSize();
  if (days_count % 365 != 0) {
    throw std::runtime_error(runoff_filename + ": Invalid timestep count");
  }
  std::string time_units;
  try {
    time_variable.getAtt("units").getValues(time_units);
  } catch (netCDF::exceptions::NcException &e) {
    throw std::runtime_error(runoff_filename +
                             ": Invalid time variable: " + e.what());
  }
  std::vector<int> times(days_count);
  time_variable.getVar({0}, {days_count}, &times[0]);
  unsigned int year, month, day;
  if (std::sscanf(time_units.c_str(), "days since %u-%u-%u", &year, &month,
                  &day) != 3) {
    throw std::runtime_error(runoff_filename + ": Invalid time variable");
  }
  const std::size_t years_count = days_count / 365;
  std::string calendar;
  try {
    time_variable.getAtt("calendar").getValues(calendar);
  } catch (netCDF::exceptions::NcException &e) {
    calendar = "365_day";
  }
  const bool simple_calendar = calendar == "365_day";
  year = 1971;

  FC_mod_input_isyear = year;  // start year
  FC_mod_input_ismon = month;  // start month
  FC_mod_input_isday = day;    // start day
  FC_mod_input_iemon = month;  // end month
  FC_mod_input_ieday = day;    // end day
  FC_mod_input_syearin = year; // start year in runoff file
  FC_mod_input_smonin = month; // start month in runoff file
  FC_mod_input_sdayin = day;   // start day in runoff file

  FC_mod_input_ieyear = year + 1; // end FC_year

  FC_additional_mod_ifirstin = 0;
  FC_mod_input_irestart = 2; // just make sure cmaflood thinks it's in spinup
                             // mode (i.e. does not read the snapshot)
  FC_init_map_mod_init_map();
  FC_init_topo_mod_init_topo();
  FC_init_cond_mod_init_cond();

  if (spinup_count > 0) {
    progressbar::ProgressBar spinup_bar(spinup_count, "Spinup", false, stderr);
    FC_init_time_mod_init_time();
    FC_control_tstp_mod_control_tstp();
    ++spinup_bar;

    for (std::size_t i = 0; i < spinup_count - 1; ++i) {
      FC_additional_mod_cleanup_tstp();
      FC_additional_mod_restart_init();
      FC_additional_mod_ifirstin = 0;
      FC_init_time_mod_init_time();
      FC_control_tstp_mod_control_tstp();
      ++spinup_bar;
    }
    spinup_bar.close();
  }

  progressbar::ProgressBar calc_bar(years_count, "Calc", false, stderr);
  FC_additional_mod_cleanup_tstp();
  FC_additional_mod_restart_init();
  FC_additional_mod_ifirstin = 0;
  FC_init_time_mod_init_time();
  FC_mod_input_ieyear = year + years_count; // end FC_year
  FC_control_tstp_mod_control_tstp();
  ++calc_bar;
  calc_bar.close();
}

static void print_usage(const char *program_name) {
  std::cerr << "CaMa-Flood Wrapper\n"
               "\n"
               "Author: Sven Willner <sven.willner@pik-potsdam.de>\n"
               "\n"
               "Usage:   "
            << program_name << " <settingsfile>\n"
            << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    print_usage(argv[0]);
    return 1;
  }
  const std::string arg = argv[1];
  if (arg.length() > 1 && arg[0] == '-') {
    if (arg == "--help" || arg == "-h") {
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
        run(settings::SettingsNode(
            std::unique_ptr<settings::YAML>(new settings::YAML(std::cin))));
      } else {
        std::ifstream settings_file(arg);
        if (!settings_file) {
          throw std::runtime_error("Cannot open " + arg);
        }
        run(settings::SettingsNode(std::unique_ptr<settings::YAML>(
            new settings::YAML(settings_file))));
      }
#ifndef DEBUG
    } catch (std::runtime_error &ex) {
      std::cerr << ex.what() << std::endl;
      return 255;
    }
#endif
  }
  return 0;
}
