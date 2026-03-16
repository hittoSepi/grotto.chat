/*****************************************************************//**
 * \file   main.cpp
 * \brief  Entry point for the Grotto-Server application.
 *         Handles command-line argument parsing, configuration loading,
 *         and server startup. Provides usage and version information.
 *
 * \author hitto
 * \date   March 2026
 *********************************************************************/
#include "config.hpp"
#include "server.hpp"
#include "utils/string_utils.hpp"
#include "version.hpp"
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>



namespace {

	using namespace grotto;

	utils::template_map template_strings {
		{"program_name", "Grotto-Server" },
		{"program_description", "End-to-end encrypted chat & voice server" },
		{"version_major", "0" },
		{"version_minor", "01.0" },
		{"git_url", "https://github.com/hittoSepi/grotto" },
		{"config_path",  default_config_path },
		{"args", "" }
	};


	const std::string &version_string = R"(
${program_name} vv.${version_major}.${version_minor}
${program_description}
${git_url}	
	)";


	const std::string &usage_string = R"(
${program_description} v.${version_major}.${version_minor}

Usage: 
${args} [options]

Options:
	--config <path>   Path to configuration file (default: ./config/server.toml)
	--headless        Run without TUI (daemon mode)
	--daemon          Alias for --headless
	--help, -h        Show this help message
	--version, -V     Show version information

Example:
${args} --config ./server.toml

	)";


	void print_usage( const char *args ) {
		template_strings[std::string( "args" )] = args;
		std::cout << utils::format_template( usage_string, template_strings );
		template_strings[std::string( "args" )] = "";
	}

	void print_version() {
		std::cout << "Grotto-Server v" << grotto::kProjectVersion << "\n"
				  << "Git: " << grotto::kGitCommitHash << "\n"
				  << "Built: " << grotto::kBuildTimestamp << "\n"
				  << "https://github.com/hittoSepi/grotto\n";
	}

	void log_startup_version() {
		spdlog::info("========================================");
		spdlog::info("Grotto-Server v{} (git: {})", grotto::kProjectVersion, grotto::kGitCommitHash);
		spdlog::info("Built: {}", grotto::kBuildTimestamp);
		spdlog::info("========================================");
	}


} // anonymous namespace

using namespace grotto;

std::string config_path = get_default_config_path();
bool headless_flag = false;

int  parse_command_line( int argc, char *argv[] ) {
	// Parse command line arguments
	for ( int i = 1; i < argc; ++i ) {
		std::string arg = argv[i];

		if ( arg == "--help" || arg == "-h" ) {
			print_usage( argv[0] );
			return 0;
		}

		if ( arg == "--version" || arg == "-V" ) {
			print_version();
			return 0;
		}

		if ( arg == "--headless" || arg == "--daemon" ) {
			headless_flag = true;
			continue;
		}

		if ( arg == "--config" ) {
			if ( i + 1 < argc ) {
				config_path = argv[++i];
			} else {
				std::cerr << "Error: --config requires a path argument\n";
				print_usage( argv[0] );
				return 1;
			}
		} else {
			std::cerr << "Error: Unknown argument: " << arg << "\n";
			print_usage( argv[0] );
			return 1;
		}
	}
	return -1;
}

int main( int argc, char *argv[] ) {

	int parsed = parse_command_line( argc, argv );
	if ( parsed != -1 ) return parsed;

	// Load configuration
	grotto::ServerConfig config;
	config = grotto::ConfigLoader::load( config_path );

	// Apply command-line overrides
	if ( headless_flag ) {
		config.headless = true;
	}

	// Validate configuration
	grotto::ConfigLoader::validate( config );

	// Log version info
	log_startup_version();

	// Create and run server
	grotto::Server server( config );
	server.run();

	return 0;

}
